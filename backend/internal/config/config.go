package config

import (
	"fmt"
	"strconv"
	"strings"
	"time"

	"github.com/spf13/viper"
)

// Config holds all configuration for the Smart Fish Feeder API
type Config struct {
	// Server configuration
	Server ServerConfig `mapstructure:"server"`

	// Database configuration
	Database DatabaseConfig `mapstructure:"database"`

	// Redis configuration
	Redis RedisConfig `mapstructure:"redis"`

	// JWT configuration
	JWT JWTConfig `mapstructure:"jwt"`

	// Logging configuration
	Logging LoggingConfig `mapstructure:"logging"`

	// Device configuration
	Device DeviceConfig `mapstructure:"device"`

	// Vision configuration
	Vision VisionConfig `mapstructure:"vision"`

	// Cloudinary configuration
	Cloudinary CloudinaryConfig `mapstructure:"cloudinary"`

	// MQTT configuration
	MQTT MQTTConfig `mapstructure:"mqtt"`

	// BLE provisioning configuration
	BLE BLEConfig `mapstructure:"ble"`

	// WebSocket configuration
	WebSocket WebSocketConfig `mapstructure:"websocket"`

	// Offline sync configuration
	OfflineSync OfflineSyncConfig `mapstructure:"offline_sync"`

	// Cellular configuration
	Cellular CellularConfig `mapstructure:"cellular"`

	// Power management configuration
	Power PowerConfig `mapstructure:"power"`
}

// VisionConfig holds computer vision and video storage configuration
type VisionConfig struct {
	StoragePath      string `mapstructure:"storage_path"`
	ImageStoragePath string `mapstructure:"image_storage_path"`
	MaxStorageMB     int64  `mapstructure:"max_storage_mb"`
	CompressionOn    bool   `mapstructure:"compression_on"`
}

// CloudinaryConfig holds Cloudinary cloud storage configuration
type CloudinaryConfig struct {
	CloudName string `mapstructure:"cloud_name"`
	APIKey    string `mapstructure:"api_key"`
	APISecret string `mapstructure:"api_secret"`
	Folder    string `mapstructure:"folder"`
	Enabled   bool   `mapstructure:"enabled"`
}

// MQTTConfig holds MQTT broker configuration
type MQTTConfig struct {
	BrokerURL        string        `mapstructure:"broker_url"`
	ClientID         string        `mapstructure:"client_id"`
	Username         string        `mapstructure:"username"`
	Password         string        `mapstructure:"password"`
	CleanSession     bool          `mapstructure:"clean_session"`
	KeepAlive        time.Duration `mapstructure:"keep_alive"`
	ConnectTimeout   time.Duration `mapstructure:"connect_timeout"`
	ReconnectBackoff time.Duration `mapstructure:"reconnect_backoff"`
	MaxReconnect     int           `mapstructure:"max_reconnect"`
	QoS              int           `mapstructure:"qos"`
	TLSEnabled       bool          `mapstructure:"tls_enabled"`
}

// BLEConfig holds BLE provisioning configuration
type BLEConfig struct {
	SessionTimeout    time.Duration `mapstructure:"session_timeout"`
	DeviceNamePrefix  string        `mapstructure:"device_name_prefix"`
	MaxActiveSessions int           `mapstructure:"max_active_sessions"`
}

// WebSocketConfig holds WebSocket configuration
type WebSocketConfig struct {
	PingInterval   time.Duration `mapstructure:"ping_interval"`
	WriteTimeout   time.Duration `mapstructure:"write_timeout"`
	ReadTimeout    time.Duration `mapstructure:"read_timeout"`
	MaxMessageSize int64         `mapstructure:"max_message_size"`
	BufferSize     int           `mapstructure:"buffer_size"`
}

// OfflineSyncConfig holds offline synchronization configuration
type OfflineSyncConfig struct {
	MaxRetries       int           `mapstructure:"max_retries"`
	RetryBackoff     time.Duration `mapstructure:"retry_backoff"`
	CleanupInterval  time.Duration `mapstructure:"cleanup_interval"`
	DataRetention    time.Duration `mapstructure:"data_retention"`
	HighPriorityMin  int           `mapstructure:"high_priority_min"`
	CompressionLevel int           `mapstructure:"compression_level"`
}

// CellularConfig holds cellular connectivity configuration
type CellularConfig struct {
	DataLimitMB        int64         `mapstructure:"data_limit_mb"`
	CostPerMB          float64       `mapstructure:"cost_per_mb"`
	LowSignalThreshold int           `mapstructure:"low_signal_threshold"`
	ReportInterval     time.Duration `mapstructure:"report_interval"`
	AlertThresholdPct  float64       `mapstructure:"alert_threshold_pct"`
}

// PowerConfig holds power management configuration
type PowerConfig struct {
	LowBatteryThreshold      float64       `mapstructure:"low_battery_threshold"`
	CriticalBatteryThreshold float64       `mapstructure:"critical_battery_threshold"`
	SolarMinVoltage          float64       `mapstructure:"solar_min_voltage"`
	BatteryFullVoltage       float64       `mapstructure:"battery_full_voltage"`
	BatteryEmptyVoltage      float64       `mapstructure:"battery_empty_voltage"`
	PowerCheckInterval       time.Duration `mapstructure:"power_check_interval"`
}

// ServerConfig holds server-related configuration
type ServerConfig struct {
	Host         string        `mapstructure:"host"`
	Port         int           `mapstructure:"port"`
	ReadTimeout  time.Duration `mapstructure:"read_timeout"`
	WriteTimeout time.Duration `mapstructure:"write_timeout"`
	Debug        bool          `mapstructure:"debug"`
}

// DatabaseConfig holds database connection configuration
type DatabaseConfig struct {
	Host     string `mapstructure:"host"`
	Port     int    `mapstructure:"port"`
	User     string `mapstructure:"user"`
	Password string `mapstructure:"password"`
	DBName   string `mapstructure:"dbname"`
	SSLMode  string `mapstructure:"sslmode"`
	TimeZone string `mapstructure:"timezone"`
}

// RedisConfig holds Redis connection configuration
type RedisConfig struct {
	Host     string `mapstructure:"host"`
	Port     int    `mapstructure:"port"`
	Password string `mapstructure:"password"`
	DB       int    `mapstructure:"db"`
}

// JWTConfig holds JWT token configuration
type JWTConfig struct {
	SecretKey            string        `mapstructure:"secret_key"`
	AccessTokenDuration  time.Duration `mapstructure:"access_token_duration"`
	RefreshTokenDuration time.Duration `mapstructure:"refresh_token_duration"`
}

// LoggingConfig holds logging configuration
type LoggingConfig struct {
	Level  string `mapstructure:"level"`
	Format string `mapstructure:"format"`
}

// DeviceConfig holds device-related configuration
type DeviceConfig struct {
	BindingCodeExpiration time.Duration `mapstructure:"binding_code_expiration"`
	MaxBindingAttempts    int           `mapstructure:"max_binding_attempts"`
}

// Load loads configuration from environment variables and config files
func Load() (*Config, error) {
	// Set default values
	setDefaults()

	// Set up Viper to read from environment variables
	viper.AutomaticEnv()
	viper.SetEnvPrefix("SFF") // Smart Fish Feeder prefix

	// Bind specific environment variables for Railway compatibility
	// Railway provides DATABASE_URL, REDIS_URL, PORT
	bindRailwayEnvVars()

	// Try to read from config file
	viper.SetConfigName("config")
	viper.SetConfigType("yaml")
	viper.AddConfigPath(".")
	viper.AddConfigPath("./configs")
	viper.AddConfigPath("/configs")
	viper.AddConfigPath("/etc/smart-fish-feeder")

	// Read config file (optional)
	if err := viper.ReadInConfig(); err != nil {
		if _, ok := err.(viper.ConfigFileNotFoundError); !ok {
			return nil, fmt.Errorf("error reading config file: %w", err)
		}
		// Config file not found, continue with defaults and env vars
	}

	// Unmarshal configuration
	var cfg Config
	if err := viper.Unmarshal(&cfg); err != nil {
		return nil, fmt.Errorf("error unmarshaling config: %w", err)
	}

	// Parse Railway DATABASE_URL if provided
	if dbURL := viper.GetString("DATABASE_URL"); dbURL != "" {
		if err := parseDatabaseURL(&cfg.Database, dbURL); err != nil {
			return nil, fmt.Errorf("error parsing DATABASE_URL: %w", err)
		}
	}

	// Parse Railway REDIS_URL if provided
	if redisURL := viper.GetString("REDIS_URL"); redisURL != "" {
		if err := parseRedisURL(&cfg.Redis, redisURL); err != nil {
			return nil, fmt.Errorf("error parsing REDIS_URL: %w", err)
		}
	}

	// Use Railway PORT if provided
	if port := viper.GetInt("PORT"); port != 0 {
		cfg.Server.Port = port
	}

	return &cfg, nil
}

// bindRailwayEnvVars binds Railway-specific environment variables
func bindRailwayEnvVars() {
	// Railway provides these without prefix
	_ = viper.BindEnv("PORT")
	_ = viper.BindEnv("DATABASE_URL")
	_ = viper.BindEnv("REDIS_URL")
	_ = viper.BindEnv("RAILWAY_ENVIRONMENT")
}

// parseDatabaseURL parses a PostgreSQL connection URL
// Format: postgresql://user:password@host:port/dbname?sslmode=disable
func parseDatabaseURL(cfg *DatabaseConfig, url string) error {
	// Remove postgresql:// or postgres:// prefix
	url = strings.TrimPrefix(url, "postgresql://")
	url = strings.TrimPrefix(url, "postgres://")

	// Split by @ to get credentials and host
	parts := strings.SplitN(url, "@", 2)
	if len(parts) != 2 {
		return fmt.Errorf("invalid database URL format")
	}

	// Parse credentials (user:password)
	credentials := strings.SplitN(parts[0], ":", 2)
	if len(credentials) >= 1 {
		cfg.User = credentials[0]
	}
	if len(credentials) >= 2 {
		cfg.Password = credentials[1]
	}

	// Parse host:port/dbname?params
	hostPart := parts[1]

	// Split by ? to separate params
	hostAndParams := strings.SplitN(hostPart, "?", 2)
	hostPart = hostAndParams[0]

	// Parse params if present
	if len(hostAndParams) == 2 {
		params := strings.Split(hostAndParams[1], "&")
		for _, param := range params {
			kv := strings.SplitN(param, "=", 2)
			if len(kv) == 2 && kv[0] == "sslmode" {
				cfg.SSLMode = kv[1]
			}
		}
	}

	// Split by / to get host:port and dbname
	hostAndDB := strings.SplitN(hostPart, "/", 2)
	if len(hostAndDB) >= 2 {
		cfg.DBName = hostAndDB[1]
	}

	// Parse host:port
	hostPort := strings.SplitN(hostAndDB[0], ":", 2)
	if len(hostPort) >= 1 {
		cfg.Host = hostPort[0]
	}
	if len(hostPort) >= 2 {
		port, err := strconv.Atoi(hostPort[1])
		if err == nil {
			cfg.Port = port
		}
	}

	// Railway PostgreSQL requires SSL
	if cfg.SSLMode == "" || cfg.SSLMode == "disable" {
		cfg.SSLMode = "require"
	}

	return nil
}

// parseRedisURL parses a Redis connection URL
// Format: redis://[:password@]host:port[/db]
func parseRedisURL(cfg *RedisConfig, url string) error {
	// Remove redis:// prefix
	url = strings.TrimPrefix(url, "redis://")
	url = strings.TrimPrefix(url, "rediss://") // TLS variant

	// Check for password
	if strings.Contains(url, "@") {
		parts := strings.SplitN(url, "@", 2)
		// Password might be :password or just password
		password := strings.TrimPrefix(parts[0], ":")
		cfg.Password = password
		url = parts[1]
	}

	// Parse host:port/db
	hostPart := url
	if strings.Contains(hostPart, "/") {
		parts := strings.SplitN(hostPart, "/", 2)
		hostPart = parts[0]
		if len(parts) == 2 {
			db, err := strconv.Atoi(parts[1])
			if err == nil {
				cfg.DB = db
			}
		}
	}

	// Parse host:port
	hostPort := strings.SplitN(hostPart, ":", 2)
	if len(hostPort) >= 1 {
		cfg.Host = hostPort[0]
	}
	if len(hostPort) >= 2 {
		port, err := strconv.Atoi(hostPort[1])
		if err == nil {
			cfg.Port = port
		}
	}

	return nil
}

// setDefaults sets default configuration values
func setDefaults() {
	// Server defaults
	viper.SetDefault("server.host", "0.0.0.0")
	viper.SetDefault("server.port", 8080)
	viper.SetDefault("server.read_timeout", "30s")
	viper.SetDefault("server.write_timeout", "30s")
	viper.SetDefault("server.debug", false)

	// Database defaults
	viper.SetDefault("database.host", "localhost")
	viper.SetDefault("database.port", 5432)
	viper.SetDefault("database.user", "smartfeeder")
	viper.SetDefault("database.password", "smartfeeder123")
	viper.SetDefault("database.dbname", "smart_fish_feeder")
	viper.SetDefault("database.sslmode", "disable")
	viper.SetDefault("database.timezone", "UTC")

	// Redis defaults
	viper.SetDefault("redis.host", "localhost")
	viper.SetDefault("redis.port", 6379)
	viper.SetDefault("redis.password", "")
	viper.SetDefault("redis.db", 0)

	// JWT defaults
	viper.SetDefault("jwt.secret_key", "your-secret-key-change-in-production")
	viper.SetDefault("jwt.access_token_duration", "1h")
	viper.SetDefault("jwt.refresh_token_duration", "720h") // 30 days

	// Logging defaults
	viper.SetDefault("logging.level", "info")
	viper.SetDefault("logging.format", "json")

	// Device defaults
	viper.SetDefault("device.binding_code_expiration", "10m")
	viper.SetDefault("device.max_binding_attempts", 3)

	// Vision defaults
	viper.SetDefault("vision.storage_path", "./storage/videos")
	viper.SetDefault("vision.image_storage_path", "./storage/images")
	viper.SetDefault("vision.max_storage_mb", 1024)
	viper.SetDefault("vision.compression_on", true)

	// Cloudinary defaults
	viper.SetDefault("cloudinary.cloud_name", "")
	viper.SetDefault("cloudinary.api_key", "")
	viper.SetDefault("cloudinary.api_secret", "")
	viper.SetDefault("cloudinary.folder", "smart-fish-feeder")
	viper.SetDefault("cloudinary.enabled", false)

	// MQTT defaults
	viper.SetDefault("mqtt.broker_url", "tcp://localhost:1883")
	viper.SetDefault("mqtt.client_id", "smart-fish-feeder-backend")
	viper.SetDefault("mqtt.username", "")
	viper.SetDefault("mqtt.password", "")
	viper.SetDefault("mqtt.clean_session", true)
	viper.SetDefault("mqtt.keep_alive", "60s")
	viper.SetDefault("mqtt.connect_timeout", "30s")
	viper.SetDefault("mqtt.reconnect_backoff", "5s")
	viper.SetDefault("mqtt.max_reconnect", 10)
	viper.SetDefault("mqtt.qos", 1)
	viper.SetDefault("mqtt.tls_enabled", false)

	// BLE provisioning defaults
	viper.SetDefault("ble.session_timeout", "30m")
	viper.SetDefault("ble.device_name_prefix", "SmartFeeder_")
	viper.SetDefault("ble.max_active_sessions", 5)

	// WebSocket defaults
	viper.SetDefault("websocket.ping_interval", "54s")
	viper.SetDefault("websocket.write_timeout", "10s")
	viper.SetDefault("websocket.read_timeout", "60s")
	viper.SetDefault("websocket.max_message_size", 512)
	viper.SetDefault("websocket.buffer_size", 256)

	// Offline sync defaults
	viper.SetDefault("offline_sync.max_retries", 5)
	viper.SetDefault("offline_sync.retry_backoff", "30s")
	viper.SetDefault("offline_sync.cleanup_interval", "24h")
	viper.SetDefault("offline_sync.data_retention", "168h") // 7 days
	viper.SetDefault("offline_sync.high_priority_min", 4)
	viper.SetDefault("offline_sync.compression_level", 9) // Best compression

	// Cellular defaults
	viper.SetDefault("cellular.data_limit_mb", 500)
	viper.SetDefault("cellular.cost_per_mb", 0.01)
	viper.SetDefault("cellular.low_signal_threshold", 10) // CSQ value
	viper.SetDefault("cellular.report_interval", "1h")
	viper.SetDefault("cellular.alert_threshold_pct", 80.0)

	// Power management defaults
	viper.SetDefault("power.low_battery_threshold", 20.0)
	viper.SetDefault("power.critical_battery_threshold", 10.0)
	viper.SetDefault("power.solar_min_voltage", 12.0)
	viper.SetDefault("power.battery_full_voltage", 14.4)
	viper.SetDefault("power.battery_empty_voltage", 11.0)
	viper.SetDefault("power.power_check_interval", "5m")
}

// GetDSN returns the database connection string
func (c *DatabaseConfig) GetDSN() string {
	return fmt.Sprintf("host=%s port=%d user=%s password=%s dbname=%s sslmode=%s TimeZone=%s",
		c.Host, c.Port, c.User, c.Password, c.DBName, c.SSLMode, c.TimeZone)
}

// GetRedisAddr returns the Redis connection address
func (c *RedisConfig) GetRedisAddr() string {
	return fmt.Sprintf("%s:%d", c.Host, c.Port)
}
