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
	GitHubToken    string
	GitHubRepo     string
	DiscordID      string
	FallbackStatus string
	MaxConnections int
	EnableGzip     bool
	R2Endpoint     string
	R2AccessKey    string
	R2SecretKey    string
	R2Bucket       string
	R2PublicURL    string
	OllamaHost     string
	OllamaModel    string
}

// Load loads configuration from environment variables
func Load() *Config {
	cfg := &Config{
		Port:           getEnv("PORT", "3000"),
		Host:           getEnv("HOST", "0.0.0.0"),
		Environment:    getEnv("ENV", "development"),
		LogLevel:       getEnv("LOG_LEVEL", "info"),
		StaticCacheTTL: getEnvAsInt("STATIC_CACHE_TTL", 31536000), // 1 year
		HTMLCacheTTL:   getEnvAsInt("HTML_CACHE_TTL", 3600),       // 1 hour in production
		CORSOrigins:    getEnv("CORS_ORIGINS", "*"),
		GitHubToken:    getEnv("GITHUB_TOKEN", ""),
		GitHubRepo:     getEnv("GITHUB_REPO", "0xjah/0xjah.xyz"),
		DiscordID:      getEnv("DISCORD_ID", "763769303681335316"),
		FallbackStatus: getEnv("FALLBACK_STATUS", "online • coding"),
		MaxConnections: getEnvAsInt("MAX_CONNECTIONS", 1000),
		EnableGzip:     getEnvAsBool("ENABLE_GZIP", true),
		R2Endpoint:     getEnv("R2_ENDPOINT", ""),
		R2AccessKey:    getEnv("R2_ACCESS_KEY", ""),
		R2SecretKey:    getEnv("R2_SECRET_KEY", ""),
		R2Bucket:       getEnv("R2_BUCKET", ""),
		R2PublicURL:    getEnv("R2_PUBLIC_URL", ""),
		OllamaHost:     getEnv("OLLAMA_HOST", "http://localhost:11434"),
		OllamaModel:    getEnv("OLLAMA_MODEL", "llama3.2:1b"),
	}

	// Optimize cache settings for production
	if cfg.IsProduction() {
		cfg.HTMLCacheTTL = getEnvAsInt("HTML_CACHE_TTL", 3600) // 1 hour cache for HTML in production
	} else {
		cfg.HTMLCacheTTL = 0 // No cache in development
	}

	log.Printf("Server configuration loaded - Port: %s, Environment: %s, Gzip: %t", cfg.Port, cfg.Environment, cfg.EnableGzip)
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

// getEnvAsBool gets an environment variable as boolean with a fallback value
func getEnvAsBool(key string, fallback bool) bool {
	if value := os.Getenv(key); value != "" {
		if boolVal, err := strconv.ParseBool(value); err == nil {
			return boolVal
		}
	}
	return fallback
}
