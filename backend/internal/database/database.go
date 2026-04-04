package database

import (
	"fmt"
	"strings"
	"time"

	"smart-fish-feeder/internal/models"

	"gorm.io/driver/postgres"
	"gorm.io/gorm"
	"gorm.io/gorm/logger"
)

// New creates a new database connection
func New(dsn string, debug bool, appLogLevel string) (*gorm.DB, error) {
	// Configure GORM
	config := &gorm.Config{
		Logger: logger.Default.LogMode(resolveGormLogLevel(debug, appLogLevel)),
		NowFunc: func() time.Time {
			return time.Now().UTC()
		},
	}

	// Connect to database
	db, err := gorm.Open(postgres.Open(dsn), config)
	if err != nil {
		return nil, fmt.Errorf("failed to connect to database: %w", err)
	}

	// Configure connection pool
	sqlDB, err := db.DB()
	if err != nil {
		return nil, fmt.Errorf("failed to get underlying sql.DB: %w", err)
	}

	// Set connection pool settings
	sqlDB.SetMaxIdleConns(10)
	sqlDB.SetMaxOpenConns(100)
	sqlDB.SetConnMaxLifetime(time.Hour)

	// Auto-migrate database schema
	if err := autoMigrate(db); err != nil {
		return nil, fmt.Errorf("failed to auto-migrate database: %w", err)
	}

	return db, nil
}

// autoMigrate runs database migrations for all models
func autoMigrate(db *gorm.DB) error {
	return db.AutoMigrate(
		// Core models
		&models.User{},
		&models.Device{},
		&models.DeviceBinding{},
		&models.FeedingEvent{},
		&models.SensorData{},
		&models.FishSpecies{},
		&models.FeedingSchedule{},
		// Alert and monitoring
		&models.Alert{},
		// Vision/Video models
		&models.VideoClip{},
		&models.ImageAnalysis{},
		&models.BoilIndexAnalysis{},
		// Cellular and connectivity
		&models.CellularDataUsage{},
		// Device diagnostics and power
		&models.DeviceDiagnostics{},
		&models.PowerEvent{},
		// FCR and growth tracking
		&models.PredictiveGrowthData{},
		&models.FeedingPrecisionData{},
		// Provisioning and offline sync
		&models.BLEProvisioningSession{},
		&models.OfflineDataBuffer{},
	)
}

func resolveGormLogLevel(debug bool, appLogLevel string) logger.LogLevel {
	level := strings.ToLower(strings.TrimSpace(appLogLevel))

	switch level {
	case "trace", "debug":
		return logger.Info
	case "warn", "warning":
		return logger.Warn
	case "error":
		return logger.Error
	case "silent":
		return logger.Silent
	case "info":
		if debug {
			return logger.Info
		}
		return logger.Warn
	default:
		if debug {
			return logger.Info
		}
		return logger.Warn
	}
}

// HealthCheck checks if the database is accessible
func HealthCheck(db *gorm.DB) error {
	sqlDB, err := db.DB()
	if err != nil {
		return err
	}
	return sqlDB.Ping()
}
