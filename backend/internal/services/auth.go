package services

import (
	"context"
	"errors"
	"fmt"
	"time"

	"github.com/golang-jwt/jwt/v5"
	"golang.org/x/crypto/bcrypt"
	"smart-fish-feeder/internal/config"
	"smart-fish-feeder/internal/models"
	"smart-fish-feeder/internal/redis"
	"smart-fish-feeder/internal/repository"
)

// AuthService handles authentication business logic
type AuthService struct {
	repo   *repository.Repository
	redis  *redis.Client
	config *config.Config
}

// NewAuthService creates a new auth service
func NewAuthService(repo *repository.Repository, redisClient *redis.Client, cfg *config.Config) *AuthService {
	return &AuthService{
		repo:   repo,
		redis:  redisClient,
		config: cfg,
	}
}

// JWTClaims represents the JWT token claims
type JWTClaims struct {
	UserID uint   `json:"user_id"`
	Email  string `json:"email"`
	jwt.RegisteredClaims
}

// RegisterUser creates a new user account
func (s *AuthService) RegisterUser(ctx context.Context, req *models.RegisterRequest) (*models.User, error) {
	// Check if user already exists
	existingUser, err := s.repo.User.GetByEmail(req.Email)
	if err == nil && existingUser != nil {
		return nil, errors.New("user with this email already exists")
	}

	// Hash password
	hashedPassword, err := s.hashPassword(req.Password)
	if err != nil {
		return nil, fmt.Errorf("failed to hash password: %w", err)
	}

	// Create user
	user := &models.User{
		Email:        req.Email,
		PasswordHash: hashedPassword,
		FirstName:    req.FirstName,
		LastName:     req.LastName,
		PhoneNumber:  req.PhoneNumber,
		IsActive:     true,
	}

	if err := s.repo.User.Create(user); err != nil {
		return nil, fmt.Errorf("failed to create user: %w", err)
	}

	// Remove password hash from response
	user.PasswordHash = ""
	return user, nil
}

// LoginUser authenticates a user and returns JWT tokens
func (s *AuthService) LoginUser(ctx context.Context, req *models.LoginRequest) (*models.AuthToken, error) {
	// Get user by email
	user, err := s.repo.User.GetByEmail(req.Email)
	if err != nil {
		return nil, errors.New("invalid email or password")
	}

	// Check if user is active
	if !user.IsActive {
		return nil, errors.New("user account is deactivated")
	}

	// Verify password
	if !s.verifyPassword(req.Password, user.PasswordHash) {
		return nil, errors.New("invalid email or password")
	}

	// Generate tokens
	accessToken, err := s.generateAccessToken(user)
	if err != nil {
		return nil, fmt.Errorf("failed to generate access token: %w", err)
	}

	refreshToken, err := s.generateRefreshToken(user)
	if err != nil {
		return nil, fmt.Errorf("failed to generate refresh token: %w", err)
	}

	// Store refresh token in Redis
	refreshKey := fmt.Sprintf("refresh_token:%d", user.ID)
	if err := s.redis.Set(ctx, refreshKey, refreshToken, s.config.JWT.RefreshTokenDuration); err != nil {
		return nil, fmt.Errorf("failed to store refresh token: %w", err)
	}

	return &models.AuthToken{
		AccessToken:  accessToken,
		RefreshToken: refreshToken,
		TokenType:    "bearer",
		ExpiresIn:    int64(s.config.JWT.AccessTokenDuration.Seconds()),
	}, nil
}

// RefreshToken generates new access token using refresh token
func (s *AuthService) RefreshToken(ctx context.Context, refreshToken string) (*models.AuthToken, error) {
	// Parse refresh token
	claims, err := s.parseToken(refreshToken)
	if err != nil {
		return nil, errors.New("invalid refresh token")
	}

	// Check if refresh token exists in Redis
	refreshKey := fmt.Sprintf("refresh_token:%d", claims.UserID)
	storedToken := ""
	if err := s.redis.Get(ctx, refreshKey, &storedToken); err != nil {
		return nil, errors.New("refresh token not found or expired")
	}

	if storedToken != refreshToken {
		return nil, errors.New("invalid refresh token")
	}

	// Get user
	user, err := s.repo.User.GetByID(claims.UserID)
	if err != nil {
		return nil, errors.New("user not found")
	}

	if !user.IsActive {
		return nil, errors.New("user account is deactivated")
	}

	// Generate new access token
	accessToken, err := s.generateAccessToken(user)
	if err != nil {
		return nil, fmt.Errorf("failed to generate access token: %w", err)
	}

	return &models.AuthToken{
		AccessToken:  accessToken,
		RefreshToken: refreshToken, // Keep the same refresh token
		TokenType:    "bearer",
		ExpiresIn:    int64(s.config.JWT.AccessTokenDuration.Seconds()),
	}, nil
}

// LogoutUser invalidates the user's refresh token
func (s *AuthService) LogoutUser(ctx context.Context, userID uint) error {
	refreshKey := fmt.Sprintf("refresh_token:%d", userID)
	return s.redis.Delete(ctx, refreshKey)
}

// ValidateToken validates and parses a JWT token
func (s *AuthService) ValidateToken(tokenString string) (*JWTClaims, error) {
	return s.parseToken(tokenString)
}

// hashPassword hashes a password using bcrypt
func (s *AuthService) hashPassword(password string) (string, error) {
	bytes, err := bcrypt.GenerateFromPassword([]byte(password), bcrypt.DefaultCost)
	return string(bytes), err
}

// verifyPassword verifies a password against its hash
func (s *AuthService) verifyPassword(password, hash string) bool {
	err := bcrypt.CompareHashAndPassword([]byte(hash), []byte(password))
	return err == nil
}

// generateAccessToken generates a JWT access token
func (s *AuthService) generateAccessToken(user *models.User) (string, error) {
	claims := &JWTClaims{
		UserID: user.ID,
		Email:  user.Email,
		RegisteredClaims: jwt.RegisteredClaims{
			ExpiresAt: jwt.NewNumericDate(time.Now().Add(s.config.JWT.AccessTokenDuration)),
			IssuedAt:  jwt.NewNumericDate(time.Now()),
			NotBefore: jwt.NewNumericDate(time.Now()),
			Issuer:    "smart-fish-feeder",
			Subject:   fmt.Sprintf("%d", user.ID),
		},
	}

	token := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	return token.SignedString([]byte(s.config.JWT.SecretKey))
}

// generateRefreshToken generates a JWT refresh token
func (s *AuthService) generateRefreshToken(user *models.User) (string, error) {
	claims := &JWTClaims{
		UserID: user.ID,
		Email:  user.Email,
		RegisteredClaims: jwt.RegisteredClaims{
			ExpiresAt: jwt.NewNumericDate(time.Now().Add(s.config.JWT.RefreshTokenDuration)),
			IssuedAt:  jwt.NewNumericDate(time.Now()),
			NotBefore: jwt.NewNumericDate(time.Now()),
			Issuer:    "smart-fish-feeder",
			Subject:   fmt.Sprintf("%d", user.ID),
		},
	}

	token := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	return token.SignedString([]byte(s.config.JWT.SecretKey))
}

// parseToken parses and validates a JWT token
func (s *AuthService) parseToken(tokenString string) (*JWTClaims, error) {
	token, err := jwt.ParseWithClaims(tokenString, &JWTClaims{}, func(token *jwt.Token) (interface{}, error) {
		if _, ok := token.Method.(*jwt.SigningMethodHMAC); !ok {
			return nil, fmt.Errorf("unexpected signing method: %v", token.Header["alg"])
		}
		return []byte(s.config.JWT.SecretKey), nil
	})

	if err != nil {
		return nil, err
	}

	if claims, ok := token.Claims.(*JWTClaims); ok && token.Valid {
		return claims, nil
	}

	return nil, errors.New("invalid token")
}
