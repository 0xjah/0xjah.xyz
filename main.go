package main

import (
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"os"
	"strings"
	"time"
)

// securityHeaders adds security headers to all responses
func securityHeaders(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("X-Content-Type-Options", "nosniff")
		w.Header().Set("X-Frame-Options", "DENY")
		w.Header().Set("X-XSS-Protection", "1; mode=block")
		w.Header().Set("Referrer-Policy", "strict-origin-when-cross-origin")
		w.Header().Set("Content-Security-Policy", "default-src 'self' https:; script-src 'self' 'unsafe-inline' https://unpkg.com; style-src 'self' 'unsafe-inline' https://cdnjs.cloudflare.com https://fonts.googleapis.com; font-src 'self' https://fonts.gstatic.com https://cdnjs.cloudflare.com; img-src 'self' https: data:;")
		next.ServeHTTP(w, r)
	})
}

// corsMiddleware adds CORS headers to responses
func corsMiddleware(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Access-Control-Allow-Origin", "*")
		w.Header().Set("Access-Control-Allow-Methods", "GET, OPTIONS")
		w.Header().Set("Access-Control-Allow-Headers", "Accept, Content-Type, Content-Length, Accept-Encoding, Authorization, HX-Request, HX-Trigger")

		if r.Method == "OPTIONS" {
			w.WriteHeader(http.StatusOK)
			return
		}

		next.ServeHTTP(w, r)
	})
}

// cacheControl adds cache control headers for static assets
func cacheControl(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		path := r.URL.Path
		var maxAge int
		switch {
		case strings.HasSuffix(path, ".css"):
			w.Header().Set("Content-Type", "text/css")
			maxAge = 31536000 // 1 year for CSS
		case strings.HasSuffix(path, ".js"):
			w.Header().Set("Content-Type", "application/javascript")
			maxAge = 31536000 // 1 year for JS
		case strings.HasSuffix(path, ".html"):
			w.Header().Set("Content-Type", "text/html; charset=utf-8")
			maxAge = 0 // No cache for HTML
		case strings.HasSuffix(path, ".png"), strings.HasSuffix(path, ".jpg"), strings.HasSuffix(path, ".gif"), strings.HasSuffix(path, ".webp"):
			maxAge = 31536000 // 1 year for images
		default:
			maxAge = 86400 // 1 day for other files
		}

		if maxAge > 0 {
			w.Header().Set("Cache-Control", fmt.Sprintf("public, max-age=%d, stale-while-revalidate=86400", maxAge))
		} else {
			w.Header().Set("Cache-Control", "no-cache, must-revalidate")
		}
		next.ServeHTTP(w, r)
	})
}

func main() {
	port := os.Getenv("PORT")
	if port == "" {
		port = "3000"
	}

	mux := http.NewServeMux()

	// API endpoints
	mux.HandleFunc("/api/last-updated", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		w.Header().Set("Cache-Control", "no-cache")
		if err := json.NewEncoder(w).Encode(map[string]string{
			"timestamp": time.Now().Format(time.RFC3339),
		}); err != nil {
			http.Error(w, "Internal server error", http.StatusInternalServerError)
			log.Printf("Error encoding JSON: %v", err)
			return
		}
	})

	// Serve static files with caching
	staticHandler := cacheControl(http.StripPrefix("/static/", http.FileServer(http.Dir("public/static"))))
	mux.Handle("/static/", staticHandler)

	// Serve partial files with proper headers
	partialsHandler := http.StripPrefix("/partials/", http.FileServer(http.Dir("public/partials")))
	mux.Handle("/partials/", partialsHandler)

	// Main route handler with HTMX support
	mux.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		// Add HTMX-specific headers for better integration
		w.Header().Set("Vary", "HX-Request")

		// Handle partial requests differently
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
	})

	log.Printf("Server started at http://localhost:%s", port)

	// Wrap entire mux with middleware chain
	handler := corsMiddleware(securityHeaders(mux))

	srv := &http.Server{
		Addr:         ":" + port,
		Handler:      handler,
		ReadTimeout:  15 * time.Second,
		WriteTimeout: 15 * time.Second,
		IdleTimeout:  60 * time.Second,
	}

	log.Fatal(srv.ListenAndServe())
}
