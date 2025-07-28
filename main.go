package main

import (
	"log"
	"net/http"
	"time"

	"0xjah.me/internal/config"
	"0xjah.me/internal/handlers"
	"0xjah.me/internal/middleware"
)

func main() {
	// Load configuration
	cfg := config.Load()

	// Create handlers
	h := handlers.New(cfg)

	// Setup routes
	mux := http.NewServeMux()

	// API endpoints
	mux.HandleFunc("/api/last-updated", h.LastUpdated)
	mux.HandleFunc("/api/github-last-updated", h.GitHubRepoLastUpdated)
	mux.HandleFunc("/api/github-last-updated-html", h.GitHubRepoLastUpdatedHTML)
	mux.HandleFunc("/health", h.HealthCheck)

	// Static files with caching
	mux.Handle("/static/", middleware.CacheControl(cfg)(http.StripPrefix("/static/", http.FileServer(http.Dir("public/static")))))

	// Partials (no caching for dynamic content)
	mux.Handle("/partials/", http.StripPrefix("/partials/", http.FileServer(http.Dir("public/partials"))))

	// Main routes (HTMX-aware)
	mux.HandleFunc("/", h.Routes)

	// Apply middleware stack
	handler := middleware.CORS(cfg)(middleware.SecurityHeaders(mux))

	srv := &http.Server{
		Addr:         cfg.Host + ":" + cfg.Port,
		Handler:      handler,
		ReadTimeout:  15 * time.Second,
		WriteTimeout: 15 * time.Second,
		IdleTimeout:  60 * time.Second,
	}

	log.Printf("🚀 Server starting on http://%s:%s", cfg.Host, cfg.Port)
	log.Printf("📝 Environment: %s", cfg.Environment)

	if err := srv.ListenAndServe(); err != nil {
		log.Fatal("❌ Server failed to start:", err)
	}
}
