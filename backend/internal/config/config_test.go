package config

import (
	"testing"
)

func TestParseDatabaseURL(t *testing.T) {
	tests := []struct {
		name     string
		url      string
		expected DatabaseConfig
	}{
		{
			name: "standard postgresql URL",
			url:  "postgresql://user:password@host.railway.app:5432/dbname",
			expected: DatabaseConfig{
				Host:     "host.railway.app",
				Port:     5432,
				User:     "user",
				Password: "password",
				DBName:   "dbname",
				SSLMode:  "require",
			},
		},
		{
			name: "postgres URL with sslmode",
			url:  "postgres://myuser:mypass@localhost:5432/mydb?sslmode=disable",
			expected: DatabaseConfig{
				Host:     "localhost",
				Port:     5432,
				User:     "myuser",
				Password: "mypass",
				DBName:   "mydb",
				SSLMode:  "disable",
			},
		},
		{
			name: "Railway format URL",
			url:  "postgresql://postgres:abc123@containers-us-west-123.railway.app:6543/railway",
			expected: DatabaseConfig{
				Host:     "containers-us-west-123.railway.app",
				Port:     6543,
				User:     "postgres",
				Password: "abc123",
				DBName:   "railway",
				SSLMode:  "require",
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			var cfg DatabaseConfig
			err := parseDatabaseURL(&cfg, tt.url)
			if err != nil {
				t.Fatalf("parseDatabaseURL() error = %v", err)
			}

			if cfg.Host != tt.expected.Host {
				t.Errorf("Host = %v, want %v", cfg.Host, tt.expected.Host)
			}
			if cfg.Port != tt.expected.Port {
				t.Errorf("Port = %v, want %v", cfg.Port, tt.expected.Port)
			}
			if cfg.User != tt.expected.User {
				t.Errorf("User = %v, want %v", cfg.User, tt.expected.User)
			}
			if cfg.Password != tt.expected.Password {
				t.Errorf("Password = %v, want %v", cfg.Password, tt.expected.Password)
			}
			if cfg.DBName != tt.expected.DBName {
				t.Errorf("DBName = %v, want %v", cfg.DBName, tt.expected.DBName)
			}
		})
	}
}

func TestParseRedisURL(t *testing.T) {
	tests := []struct {
		name     string
		url      string
		expected RedisConfig
	}{
		{
			name: "simple redis URL",
			url:  "redis://localhost:6379",
			expected: RedisConfig{
				Host: "localhost",
				Port: 6379,
			},
		},
		{
			name: "redis URL with password",
			url:  "redis://:mypassword@redis.railway.app:6379",
			expected: RedisConfig{
				Host:     "redis.railway.app",
				Port:     6379,
				Password: "mypassword",
			},
		},
		{
			name: "redis URL with db",
			url:  "redis://localhost:6379/2",
			expected: RedisConfig{
				Host: "localhost",
				Port: 6379,
				DB:   2,
			},
		},
		{
			name: "Railway redis URL",
			url:  "redis://default:abc123@containers-us-west-456.railway.app:6379",
			expected: RedisConfig{
				Host:     "containers-us-west-456.railway.app",
				Port:     6379,
				Password: "default:abc123",
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			var cfg RedisConfig
			err := parseRedisURL(&cfg, tt.url)
			if err != nil {
				t.Fatalf("parseRedisURL() error = %v", err)
			}

			if cfg.Host != tt.expected.Host {
				t.Errorf("Host = %v, want %v", cfg.Host, tt.expected.Host)
			}
			if cfg.Port != tt.expected.Port {
				t.Errorf("Port = %v, want %v", cfg.Port, tt.expected.Port)
			}
			if cfg.DB != tt.expected.DB {
				t.Errorf("DB = %v, want %v", cfg.DB, tt.expected.DB)
			}
		})
	}
}

func TestLoad(t *testing.T) {
	// Test that Load() works with defaults
	cfg, err := Load()
	if err != nil {
		t.Fatalf("Load() error = %v", err)
	}

	if cfg.Server.Port == 0 {
		t.Error("Server.Port should have a default value")
	}

	if cfg.Database.Host == "" {
		t.Error("Database.Host should have a default value")
	}

	if cfg.Redis.Host == "" {
		t.Error("Redis.Host should have a default value")
	}
}
