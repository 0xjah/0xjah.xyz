package config

import (
	"log"
	"os"
	"strconv"
)

// Config holds all application configuration
type Config struct {
	Port           string
	Host           string
	Environment    string
	LogLevel       string
	StaticCacheTTL int
	HTMLCacheTTL   int
	CORSOrigins    string
}

// Load loads configuration from environment variables
func Load() *Config {
	cfg := &Config{
		Port:           getEnv("PORT", "3000"),
		Host:           getEnv("HOST", "0.0.0.0"),
		Environment:    getEnv("ENV", "development"),
		LogLevel:       getEnv("LOG_LEVEL", "info"),
		StaticCacheTTL: getEnvAsInt("STATIC_CACHE_TTL", 31536000), // 1 year
		HTMLCacheTTL:   getEnvAsInt("HTML_CACHE_TTL", 0),          // no cache
		CORSOrigins:    getEnv("CORS_ORIGINS", "*"),
	}

	log.Printf("Server configuration loaded - Port: %s, Environment: %s", cfg.Port, cfg.Environment)
	return cfg
}

// IsDevelopment returns true if running in development mode
func (c *Config) IsDevelopment() bool {
	return c.Environment == "development"
}

// IsProduction returns true if running in production mode
func (c *Config) IsProduction() bool {
	return c.Environment == "production"
}

// getEnv gets an environment variable with a fallback value
func getEnv(key, fallback string) string {
	if value := os.Getenv(key); value != "" {
		return value
	}
	return fallback
}

// getEnvAsInt gets an environment variable as integer with a fallback value
func getEnvAsInt(key string, fallback int) int {
	if value := os.Getenv(key); value != "" {
		if intVal, err := strconv.Atoi(value); err == nil {
			return intVal
		}
	}
	return fallback
}
