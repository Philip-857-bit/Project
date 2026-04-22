package app

import (
	"context"
	"fmt"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	"smart-fish-feeder/internal/config"
	"smart-fish-feeder/internal/database"
	"smart-fish-feeder/internal/handlers"
	"smart-fish-feeder/internal/middleware"
	"smart-fish-feeder/internal/mqtt"
	"smart-fish-feeder/internal/redis"
	"smart-fish-feeder/internal/repository"
	"smart-fish-feeder/internal/services"

	"github.com/gin-gonic/gin"
	"github.com/sirupsen/logrus"
)

// App represents the main application
type App struct {
	config     *config.Config
	server     *http.Server
	logger     *logrus.Logger
	mqttClient *mqtt.Client
}

// New creates a new application instance
func New(cfg *config.Config) *App {
	return &App{
		config: cfg,
		logger: setupLogger(cfg),
	}
}

// Run starts the application
func (a *App) Run() error {
	// Initialize database
	db, err := database.New(
		a.config.Database.GetDSN(),
		a.config.Server.Debug,
		a.config.Logging.Level,
	)
	if err != nil {
		return fmt.Errorf("failed to initialize database: %w", err)
	}

	// Initialize Redis
	redisClient, err := redis.New(a.config.Redis.GetRedisAddr(), a.config.Redis.Password, a.config.Redis.DB)
	if err != nil {
		return fmt.Errorf("failed to initialize Redis: %w", err)
	}

	// Initialize MQTT client (optional - only if configured)
	if a.config.MQTT.BrokerURL != "" {
		mqttConfig := &mqtt.Config{
			BrokerURL:        a.config.MQTT.BrokerURL,
			ClientID:         a.config.MQTT.ClientID,
			Username:         a.config.MQTT.Username,
			Password:         a.config.MQTT.Password,
			CleanSession:     true,
			KeepAlive:        60 * time.Second,
			ConnectTimeout:   30 * time.Second,
			ReconnectBackoff: 5 * time.Second,
			MaxReconnect:     10,
			QoS:              1,
			TLSEnabled:       a.config.MQTT.TLSEnabled,
		}

		mqttClient, err := mqtt.NewClient(mqttConfig, a.logger)
		if err != nil {
			a.logger.WithError(err).Warn("Failed to create MQTT client, continuing without MQTT")
		} else {
			ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
			if err := mqttClient.Connect(ctx); err != nil {
				a.logger.WithError(err).Warn("Failed to connect to MQTT broker, continuing without MQTT")
			} else {
				a.mqttClient = mqttClient
				a.logger.Info("MQTT client connected successfully")

				// Setup MQTT message handlers
				a.setupMQTTHandlers()
			}
			cancel()
		}
	} else {
		a.logger.Info("MQTT not configured, skipping MQTT initialization")
	}

	// Initialize repositories
	repos := repository.New(db)

	// Initialize services
	services := services.New(repos, redisClient, a.config, a.logger)

	// Ensure calculator species exist for mobile feed calculator dropdown.
	if err := services.Calculator.SeedDefaultSpecies(); err != nil {
		a.logger.WithError(err).Warn("Failed to seed default calculator species")
	} else {
		a.logger.Info("Calculator species seed check completed")
	}

	// Initialize handlers
	handlers := handlers.New(services, a.logger)
	handlers.Device.SetMQTTClient(a.mqttClient)

	// Setup router
	router := a.setupRouter(handlers)

	// Create HTTP server
	a.server = &http.Server{
		Addr:         fmt.Sprintf("%s:%d", a.config.Server.Host, a.config.Server.Port),
		Handler:      router,
		ReadTimeout:  a.config.Server.ReadTimeout,
		WriteTimeout: a.config.Server.WriteTimeout,
	}

	// Start server in a goroutine
	go func() {
		a.logger.Infof("Starting Smart Fish Feeder API server on %s", a.server.Addr)
		if err := a.server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			a.logger.Fatalf("Failed to start server: %v", err)
		}
	}()

	// Wait for interrupt signal to gracefully shutdown the server
	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit

	a.logger.Info("Shutting down server...")

	// Disconnect MQTT client
	if a.mqttClient != nil {
		a.mqttClient.Disconnect()
		a.logger.Info("MQTT client disconnected")
	}

	// Graceful shutdown with timeout
	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()

	if err := a.server.Shutdown(ctx); err != nil {
		return fmt.Errorf("server forced to shutdown: %w", err)
	}

	a.logger.Info("Server exited")
	return nil
}

// setupMQTTHandlers configures MQTT topic subscriptions and message handlers
func (a *App) setupMQTTHandlers() {
	if a.mqttClient == nil {
		return
	}

	// Subscribe to device telemetry
	if err := a.mqttClient.Subscribe(mqtt.TopicDeviceTelemetryAll, 1, func(topic string, payload []byte) error {
		deviceID := mqtt.ExtractDeviceID(topic)
		a.logger.WithFields(logrus.Fields{
			"device_id": deviceID,
			"topic":     topic,
		}).Debug("Received device telemetry")
		return nil
	}); err != nil {
		a.logger.WithError(err).Error("Failed to subscribe to device telemetry")
	}

	// Subscribe to device sensor data
	if err := a.mqttClient.Subscribe(mqtt.TopicDeviceSensorDataAll, 1, func(topic string, payload []byte) error {
		deviceID := mqtt.ExtractDeviceID(topic)
		a.logger.WithFields(logrus.Fields{
			"device_id": deviceID,
			"topic":     topic,
		}).Debug("Received sensor data via MQTT")
		return nil
	}); err != nil {
		a.logger.WithError(err).Error("Failed to subscribe to sensor data")
	}

	// Subscribe to device feeding events
	if err := a.mqttClient.Subscribe(mqtt.TopicDeviceFeedingAll, 1, func(topic string, payload []byte) error {
		deviceID := mqtt.ExtractDeviceID(topic)
		a.logger.WithFields(logrus.Fields{
			"device_id": deviceID,
			"topic":     topic,
		}).Debug("Received feeding event via MQTT")
		return nil
	}); err != nil {
		a.logger.WithError(err).Error("Failed to subscribe to feeding events")
	}

	// Subscribe to device status updates
	if err := a.mqttClient.Subscribe(mqtt.TopicDeviceStatusAll, 1, func(topic string, payload []byte) error {
		deviceID := mqtt.ExtractDeviceID(topic)
		a.logger.WithFields(logrus.Fields{
			"device_id": deviceID,
			"topic":     topic,
		}).Debug("Received device status update")
		return nil
	}); err != nil {
		a.logger.WithError(err).Error("Failed to subscribe to device status")
	}

	// Subscribe to device alerts
	if err := a.mqttClient.Subscribe(mqtt.TopicDeviceAlertAll, 1, func(topic string, payload []byte) error {
		deviceID := mqtt.ExtractDeviceID(topic)
		a.logger.WithFields(logrus.Fields{
			"device_id": deviceID,
			"topic":     topic,
		}).Warn("Received device alert via MQTT")
		return nil
	}); err != nil {
		a.logger.WithError(err).Error("Failed to subscribe to device alerts")
	}

	a.logger.Info("MQTT handlers configured successfully")
}

// setupRouter configures the Gin router with all routes and middleware
func (a *App) setupRouter(h *handlers.Handlers) *gin.Engine {
	// Allow explicit GIN_MODE override, otherwise derive from server debug flag.
	if mode := os.Getenv("GIN_MODE"); mode != "" {
		gin.SetMode(mode)
	} else if !a.config.Server.Debug {
		gin.SetMode(gin.ReleaseMode)
	} else {
		gin.SetMode(gin.DebugMode)
	}

	router := gin.New()

	// Global middleware
	router.Use(middleware.Logger(a.logger))
	router.Use(middleware.Recovery(a.logger))
	router.Use(middleware.CORS())
	router.Use(middleware.RequestID())

	// Health check endpoints
	router.GET("/health", h.Health.Basic)
	router.GET("/health/detailed", h.Health.Detailed)

	// Root endpoint
	router.GET("/", h.Health.Root)

	// API v1 routes
	v1 := router.Group("/api/v1")
	{
		// Authentication routes
		auth := v1.Group("/auth")
		{
			auth.POST("/register", h.Auth.Register)
			auth.POST("/login", h.Auth.Login)
			auth.POST("/refresh", h.Auth.RefreshToken)
			auth.POST("/logout", middleware.AuthRequired(), h.Auth.Logout)
			auth.POST("/password-reset/request", h.Auth.RequestPasswordReset)
			auth.POST("/password-reset/verify", h.Auth.VerifyPasswordResetCode)
			auth.POST("/password-reset/confirm", h.Auth.ConfirmPasswordReset)
		}

		// User routes
		users := v1.Group("/users")
		users.Use(middleware.AuthRequired())
		{
			users.GET("/profile", h.User.GetProfile)
			users.PUT("/profile", h.User.UpdateProfile)
		}

		// Device routes
		devices := v1.Group("/devices")
		{
			devices.POST("/register", h.Device.Register) // Arduino registration
			devices.GET("/binding-code", middleware.AuthRequired(), h.Device.GenerateBindingCode)
			devices.POST("/bind", middleware.AuthRequired(), h.Device.Bind)
			devices.GET("", middleware.AuthRequired(), h.Device.List)
			devices.GET("/:id", middleware.AuthRequired(), h.Device.Get)
			devices.POST("/:id/capture-video", middleware.AuthRequired(), h.Device.CaptureVideo)
			devices.PUT("/:id", middleware.AuthRequired(), h.Device.Update)
			devices.DELETE("/:id", middleware.AuthRequired(), h.Device.Delete)
		}

		// Feeding routes
		feeding := v1.Group("/feeding")
		feeding.Use(middleware.AuthRequired())
		{
			feeding.GET("/schedules", h.Feeding.GetSchedules)
			feeding.POST("/schedules", h.Feeding.CreateSchedule)
			feeding.PUT("/schedules/:id", h.Feeding.UpdateSchedule)
			feeding.DELETE("/schedules/:id", h.Feeding.DeleteSchedule)
			feeding.POST("/manual", h.Feeding.ManualFeed)
			feeding.GET("/history", h.Feeding.GetHistory)
			feeding.GET("/analytics", h.Feeding.GetAnalytics)
		}

		// Monitoring routes
		monitoring := v1.Group("/monitoring")
		monitoring.Use(middleware.AuthRequired())
		{
			monitoring.GET("/sensors", h.Monitoring.GetSensorData)
			monitoring.POST("/sensors", h.Monitoring.ReceiveSensorData) // Arduino endpoint
			monitoring.GET("/sensors/aggregation", h.Monitoring.GetSensorDataAggregation)
			monitoring.GET("/sensors/stream", h.Monitoring.StreamSensorData) // WebSocket endpoint
			monitoring.GET("/status", h.Monitoring.GetDeviceStatus)
			monitoring.GET("/alerts", h.Monitoring.GetAlerts)
			monitoring.GET("/trends", h.Monitoring.GetDeviceTrends)
			monitoring.GET("/health-score", h.Monitoring.GetDeviceHealthScore)
		}

		// Calculator routes
		calculator := v1.Group("/calculator")
		calculator.Use(middleware.AuthRequired())
		{
			calculator.POST("/recommend", h.Calculator.CalculateRecommendation)
			calculator.GET("/species", h.Calculator.GetSpecies)
			calculator.GET("/species/:id", h.Calculator.GetSpeciesByID)
			calculator.POST("/species", h.Calculator.CreateSpecies)
			calculator.PUT("/species/:id", h.Calculator.UpdateSpecies)
			calculator.DELETE("/species/:id", h.Calculator.DeleteSpecies)
		}

		// Certificate management routes
		certificates := v1.Group("/certificates")
		certificates.Use(middleware.AuthRequired())
		{
			certificates.POST("/issue", h.Certificate.IssueCertificate)
			certificates.POST("/verify", h.Certificate.VerifyCertificate)
			certificates.POST("/revoke", h.Certificate.RevokeCertificate)
			certificates.POST("/rotate", h.Certificate.RotateCertificate)
			certificates.GET("/:device_id/status", h.Certificate.GetCertificateStatus)
			certificates.GET("/ca", h.Certificate.GetCACertificate)
			certificates.GET("", h.Certificate.ListCertificates)
			certificates.GET("/expiring", h.Certificate.GetExpiringCertificates)
			certificates.POST("/firmware/sign", h.Certificate.SignFirmware)
			certificates.POST("/firmware/verify", h.Certificate.VerifyFirmware)
		}

		// FCR Analytics routes
		fcr := v1.Group("/fcr")
		fcr.Use(middleware.AuthRequired())
		{
			fcr.POST("/feeding", h.FCRAnalytics.RecordFeedingData)
			fcr.POST("/growth", h.FCRAnalytics.RecordGrowthData)
			fcr.GET("/:device_id/analytics", h.FCRAnalytics.GetFCRAnalytics)
			fcr.POST("/calculate", h.FCRAnalytics.CalculateFCR)
			fcr.GET("/:device_id/correlations", h.FCRAnalytics.GetEnvironmentalCorrelations)
			fcr.GET("/compare", h.FCRAnalytics.CompareDevices)
			fcr.POST("/:device_id/predict", h.FCRAnalytics.PredictGrowth)
			fcr.GET("/:device_id/history", h.FCRAnalytics.GetFCRHistory)
		}

		// Vision/Video routes (ESP32-CAM uploads)
		vision := v1.Group("/vision")
		vision.Use(middleware.AuthRequired())
		{
			vision.POST("/upload", h.Vision.UploadVideo)
			vision.POST("/upload/chunk", h.Vision.UploadVideoChunk)
			vision.GET("/clips", h.Vision.GetVideoClips)
			vision.GET("/clips/:id", h.Vision.GetVideoClip)
			vision.GET("/clips/device/:device_id", h.Vision.GetVideoClipsByDevice)
			vision.GET("/clips/feeding/:feeding_event_id", h.Vision.GetVideoClipsByFeedingEvent)
			vision.GET("/clips/:id/stream", h.Vision.StreamVideoClip)
			vision.DELETE("/clips/:id", h.Vision.DeleteVideoClip)
			vision.POST("/analyze/image", h.Vision.AnalyzeImage)
			vision.POST("/analyze/boil-index", h.Vision.AnalyzeBoilIndex)
			vision.GET("/analyses/device/:device_id", h.Vision.GetImageAnalyses)
			vision.GET("/boil-index/device/:device_id", h.Vision.GetBoilIndexAnalyses)
			vision.GET("/stats/:device_id", h.Vision.GetVisionStats)
			vision.GET("/storage/:device_id", h.Vision.GetStorageUsage)
		}

		feedingEvents := v1.Group("/feeding-events")
		feedingEvents.Use(middleware.AuthRequired())
		{
			feedingEvents.GET("/:feeding_event_id/verification", h.Vision.GetFeedingVerification)
		}

		// Power management routes
		power := v1.Group("/power")
		power.Use(middleware.AuthRequired())
		{
			power.GET("/:device_id/status", h.Power.GetPowerStatus)
			power.POST("/:device_id/status", h.Power.UpdatePowerStatus)
			power.GET("/:device_id/history", h.Power.GetPowerHistory)
			power.GET("/:device_id/stats", h.Power.GetPowerStats)
			power.GET("/:device_id/battery", h.Power.GetBatteryHealth)
			power.GET("/:device_id/solar", h.Power.GetSolarStatus)
			power.POST("/:device_id/sleep", h.Power.TriggerDeepSleep)
		}

		// Cellular connectivity routes
		cellular := v1.Group("/cellular")
		cellular.Use(middleware.AuthRequired())
		{
			cellular.GET("/:device_id/status", h.Cellular.GetCellularStatus)
			cellular.POST("/:device_id/signal", h.Cellular.UpdateSignalStrength)
			cellular.POST("/:device_id/usage", h.Cellular.RecordDataUsage)
			cellular.GET("/:device_id/report", h.Cellular.GetDataUsageReport)
			cellular.GET("/:device_id/limit", h.Cellular.CheckDataLimit)
			cellular.GET("/:device_id/optimize", h.Cellular.GetOptimizationPlan)
		}

		// Device diagnostics routes
		diagnostics := v1.Group("/diagnostics")
		diagnostics.Use(middleware.AuthRequired())
		{
			diagnostics.GET("/:device_id/health", h.Power.GetDeviceHealth)
			diagnostics.POST("/:device_id", h.Power.RecordDiagnostics)
			diagnostics.GET("/:device_id/maintenance", h.Power.GetMaintenancePrediction)
			diagnostics.GET("/:device_id/stallguard", h.Power.GetStallGuardStatus)
		}
	}

	return router
}

// setupLogger configures the application logger
func setupLogger(cfg *config.Config) *logrus.Logger {
	logger := logrus.New()

	// Set log level
	level, err := logrus.ParseLevel(cfg.Logging.Level)
	if err != nil {
		level = logrus.InfoLevel
	}
	logger.SetLevel(level)

	// Set log format
	if cfg.Logging.Format == "json" {
		logger.SetFormatter(&logrus.JSONFormatter{
			TimestampFormat: time.RFC3339,
		})
	} else {
		logger.SetFormatter(&logrus.TextFormatter{
			FullTimestamp:   true,
			TimestampFormat: time.RFC3339,
		})
	}

	return logger
}
