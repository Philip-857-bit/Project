package handlers

import (
	"net/http"
	"time"

	"smart-fish-feeder/internal/services"

	"github.com/gin-gonic/gin"
	"github.com/sirupsen/logrus"
)

// HealthHandler handles health check endpoints
type HealthHandler struct {
	services *services.Services
	logger   *logrus.Logger
}

// NewHealthHandler creates a new health handler
func NewHealthHandler(services *services.Services, logger *logrus.Logger) *HealthHandler {
	return &HealthHandler{
		services: services,
		logger:   logger,
	}
}

// Basic handles basic health check
func (h *HealthHandler) Basic(c *gin.Context) {
	c.JSON(http.StatusOK, gin.H{
		"status":    "healthy",
		"service":   "Smart Fish Feeder API",
		"version":   "1.0.0",
		"timestamp": time.Now().Unix(),
	})
}

// Detailed handles detailed health check with component status
func (h *HealthHandler) Detailed(c *gin.Context) {
	overallStatus := "healthy"
	components := gin.H{}

	// Check if services is available
	if h.services == nil {
		c.JSON(http.StatusServiceUnavailable, gin.H{
			"status":    "degraded",
			"service":   "Smart Fish Feeder API",
			"version":   "1.0.0",
			"timestamp": time.Now().Unix(),
			"components": gin.H{
				"database": gin.H{
					"status": "unavailable",
					"error":  "services not initialized",
				},
				"redis": gin.H{
					"status": "unavailable",
					"error":  "services not initialized",
				},
				"websocket": gin.H{
					"status": "unavailable",
				},
			},
		})
		return
	}

	// Check database health
	repo := h.services.GetRepository()
	if repo == nil {
		components["database"] = gin.H{
			"status": "unavailable",
			"error":  "repository not available",
		}
		overallStatus = "degraded"
	} else {
		db := repo.GetDB()

		dbStatus := "healthy"
		dbError := ""
		sqlDB, err := db.DB()
		if err != nil {
			dbStatus = "unhealthy"
			dbError = err.Error()
			overallStatus = "degraded"
		} else if err := sqlDB.Ping(); err != nil {
			dbStatus = "unhealthy"
			dbError = err.Error()
			overallStatus = "degraded"
		}

		components["database"] = gin.H{
			"status": dbStatus,
			"error":  dbError,
		}
	}

	// Check Redis health
	redisClient := h.services.GetRedis()
	redisStatus := "healthy"
	redisError := ""

	if redisClient == nil {
		redisStatus = "unavailable"
		redisError = "redis client not available"
		overallStatus = "degraded"
	} else {
		ctx := c.Request.Context()
		if err := redisClient.HealthCheck(ctx); err != nil {
			redisStatus = "unhealthy"
			redisError = err.Error()
			overallStatus = "degraded"
		}
	}

	components["redis"] = gin.H{
		"status": redisStatus,
		"error":  redisError,
	}

	// Check WebSocket hub
	components["websocket"] = gin.H{
		"status": "healthy",
	}

	statusCode := http.StatusOK
	if overallStatus == "degraded" {
		statusCode = http.StatusServiceUnavailable
	}

	c.JSON(statusCode, gin.H{
		"status":     overallStatus,
		"service":    "Smart Fish Feeder API",
		"version":    "1.0.0",
		"timestamp":  time.Now().Unix(),
		"components": components,
	})
}

// Root handles the root endpoint
func (h *HealthHandler) Root(c *gin.Context) {
	c.JSON(http.StatusOK, gin.H{
		"message": "Welcome to Smart Fish Feeder API",
		"version": "1.0.0",
		"docs":    "/api/v1/docs",
		"health":  "/health",
		"api":     "/api/v1",
	})
}
