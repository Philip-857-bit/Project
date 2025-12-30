package handlers

import (
	"net/http"

	"github.com/gin-gonic/gin"
	"github.com/go-playground/validator/v10"
	"github.com/sirupsen/logrus"
	"smart-fish-feeder/internal/models"
	"smart-fish-feeder/internal/services"
)

// AuthHandler handles authentication endpoints
type AuthHandler struct {
	services  *services.Services
	logger    *logrus.Logger
	validator *validator.Validate
}

// NewAuthHandler creates a new auth handler
func NewAuthHandler(services *services.Services, logger *logrus.Logger) *AuthHandler {
	return &AuthHandler{
		services:  services,
		logger:    logger,
		validator: validator.New(),
	}
}

// Register handles user registration
func (h *AuthHandler) Register(c *gin.Context) {
	var req models.RegisterRequest

	// Bind JSON request
	if err := c.ShouldBindJSON(&req); err != nil {
		h.logger.WithError(err).Error("Failed to bind registration request")
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "Invalid request format",
			"details": err.Error(),
		})
		return
	}

	// Validate request
	if err := h.validator.Struct(&req); err != nil {
		h.logger.WithError(err).Error("Registration request validation failed")
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "Validation failed",
			"details": err.Error(),
		})
		return
	}

	// Register user
	user, err := h.services.Auth.RegisterUser(c.Request.Context(), &req)
	if err != nil {
		h.logger.WithError(err).Error("User registration failed")
		c.JSON(http.StatusConflict, gin.H{
			"error": err.Error(),
		})
		return
	}

	h.logger.WithField("user_id", user.ID).Info("User registered successfully")
	c.JSON(http.StatusCreated, gin.H{
		"message": "User registered successfully",
		"user":    user,
	})
}

// Login handles user login
func (h *AuthHandler) Login(c *gin.Context) {
	var req models.LoginRequest

	// Bind JSON request
	if err := c.ShouldBindJSON(&req); err != nil {
		h.logger.WithError(err).Error("Failed to bind login request")
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "Invalid request format",
			"details": err.Error(),
		})
		return
	}

	// Validate request
	if err := h.validator.Struct(&req); err != nil {
		h.logger.WithError(err).Error("Login request validation failed")
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "Validation failed",
			"details": err.Error(),
		})
		return
	}

	// Authenticate user
	tokens, err := h.services.Auth.LoginUser(c.Request.Context(), &req)
	if err != nil {
		h.logger.WithError(err).WithField("email", req.Email).Error("User login failed")
		c.JSON(http.StatusUnauthorized, gin.H{
			"error": err.Error(),
		})
		return
	}

	h.logger.WithField("email", req.Email).Info("User logged in successfully")
	c.JSON(http.StatusOK, tokens)
}

// RefreshToken handles token refresh
func (h *AuthHandler) RefreshToken(c *gin.Context) {
	var req struct {
		RefreshToken string `json:"refresh_token" validate:"required"`
	}

	// Bind JSON request
	if err := c.ShouldBindJSON(&req); err != nil {
		h.logger.WithError(err).Error("Failed to bind refresh token request")
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "Invalid request format",
			"details": err.Error(),
		})
		return
	}

	// Validate request
	if err := h.validator.Struct(&req); err != nil {
		h.logger.WithError(err).Error("Refresh token request validation failed")
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "Validation failed",
			"details": err.Error(),
		})
		return
	}

	// Refresh token
	tokens, err := h.services.Auth.RefreshToken(c.Request.Context(), req.RefreshToken)
	if err != nil {
		h.logger.WithError(err).Error("Token refresh failed")
		c.JSON(http.StatusUnauthorized, gin.H{
			"error": err.Error(),
		})
		return
	}

	h.logger.Info("Token refreshed successfully")
	c.JSON(http.StatusOK, tokens)
}

// Logout handles user logout
func (h *AuthHandler) Logout(c *gin.Context) {
	// Get user ID from context (set by auth middleware)
	userID, exists := c.Get("user_id")
	if !exists {
		c.JSON(http.StatusUnauthorized, gin.H{
			"error": "User not authenticated",
		})
		return
	}

	// Logout user
	if err := h.services.Auth.LogoutUser(c.Request.Context(), userID.(uint)); err != nil {
		h.logger.WithError(err).WithField("user_id", userID).Error("User logout failed")
		c.JSON(http.StatusInternalServerError, gin.H{
			"error": "Failed to logout user",
		})
		return
	}

	h.logger.WithField("user_id", userID).Info("User logged out successfully")
	c.JSON(http.StatusOK, gin.H{
		"message": "Logged out successfully",
	})
}
