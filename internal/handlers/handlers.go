package handlers

import (
	"encoding/json"
	"log"
	"net/http"
	"time"

	"0xjah.me/internal/config"
)

// Handlers holds all HTTP handlers with dependencies
type Handlers struct {
	cfg *config.Config
}

// New creates a new handlers instance
func New(cfg *config.Config) *Handlers {
	return &Handlers{cfg: cfg}
}

// Routes serves main application routes
func (h *Handlers) Routes(w http.ResponseWriter, r *http.Request) {
	// Add HTMX-specific headers
	w.Header().Set("Vary", "HX-Request")

	if r.Header.Get("HX-Request") == "true" {
		w.Header().Set("Cache-Control", "no-cache")
	}

	switch r.URL.Path {
	case "/":
		http.ServeFile(w, r, "public/index.html")
	case "/blog":
		http.ServeFile(w, r, "public/blog.html")
	case "/misc":
		http.ServeFile(w, r, "public/misc.html")
	default:
		http.NotFound(w, r)
	}
}

// Splash serves splash screen (client handles per-tab logic)
func (h *Handlers) Splash(w http.ResponseWriter, r *http.Request) {
	http.ServeFile(w, r, "public/partials/ui/splash.html")
}

// LastUpdated returns the current timestamp for the last-updated API
func (h *Handlers) LastUpdated(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json")
	w.Header().Set("Cache-Control", "no-cache")

	if err := json.NewEncoder(w).Encode(map[string]string{
		"timestamp": time.Now().Format(time.RFC3339),
	}); err != nil {
		http.Error(w, "Internal server error", http.StatusInternalServerError)
		log.Printf("Error encoding JSON: %v", err)
		return
	}
}

// HealthCheck provides a simple health check endpoint
func (h *Handlers) HealthCheck(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)

	response := map[string]interface{}{
		"status":      "healthy",
		"timestamp":   time.Now().Format(time.RFC3339),
		"environment": h.cfg.Environment,
	}

	json.NewEncoder(w).Encode(response)
}
