package main

import (
	"context"
	"log"
	"net/http"
	"os"
	"os/signal"
	"syscall"
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

	// Setup routes with optimized multiplexer
	mux := http.NewServeMux()

	// API endpoints (high performance JSON endpoints)
	mux.HandleFunc("/api/last-updated", h.LastUpdated)
	mux.HandleFunc("/api/github-last-updated", h.GitHubRepoLastUpdated)
	mux.HandleFunc("/api/github-last-updated-html", h.GitHubRepoLastUpdatedHTML)
	mux.HandleFunc("/api/discord-status", h.DiscordStatus)
	mux.HandleFunc("/api/gallery", h.Gallery)
	mux.HandleFunc("/health", h.HealthCheck)

	// Static files with aggressive caching
	mux.Handle("/static/", middleware.CacheControl(cfg)(http.StripPrefix("/static/", http.FileServer(http.Dir("public/static")))))

	// Partials (for blog posts only, no caching for dynamic content)
	mux.Handle("/partials/", http.StripPrefix("/partials/", http.FileServer(http.Dir("public/partials"))))

	// Main routes (HTMX-aware with embedded content)
	mux.HandleFunc("/", h.Routes)

	// Apply optimized middleware stack
	handler := middleware.Compression(
		middleware.CORS(cfg)(
			middleware.SecurityHeaders(mux),
		),
	)

	// Optimized server configuration for maximum performance
	srv := &http.Server{
		Addr:    cfg.Host + ":" + cfg.Port,
		Handler: handler,

		// Optimized timeouts for high performance
		ReadTimeout:       10 * time.Second,  // Reduced from 15s
		WriteTimeout:      10 * time.Second,  // Reduced from 15s
		IdleTimeout:       120 * time.Second, // Increased from 60s for connection reuse
		ReadHeaderTimeout: 5 * time.Second,   // Added for security

		// Optimize for high concurrent connections
		MaxHeaderBytes: 1 << 16, // 64KB max header size
	}

	log.Printf("Server starting on http://%s:%s", cfg.Host, cfg.Port)
	log.Printf("Environment: %s", cfg.Environment)
	log.Printf("Optimizations: Compression enabled, embedded content, optimized timeouts")

	// Graceful shutdown setup
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, os.Interrupt, syscall.SIGTERM)

	go func() {
		if err := srv.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			log.Fatal("Server failed to start:", err)
		}
	}()

	log.Println("Server started successfully")

	// Wait for shutdown signal
	<-sigChan
	log.Println("Shutting down server...")

	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()

	if err := srv.Shutdown(ctx); err != nil {
		log.Printf("Server shutdown error: %v", err)
	} else {
		log.Println("Server shutdown complete")
	}
}
