package main

import (
	"context"
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"os"
	"os/signal"
	"path/filepath"
	"strings"
	"syscall"
	"time"
)

// Improved logging middleware
func loggingMiddleware(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		start := time.Now()

		// Wrap ResponseWriter to capture status code
		wrapped := &responseWriter{ResponseWriter: w, statusCode: http.StatusOK}

		next.ServeHTTP(wrapped, r)

		log.Printf("%s %s %d %v %s",
			r.Method,
			r.URL.Path,
			wrapped.statusCode,
			time.Since(start),
			r.UserAgent(),
		)
	})
}

// ResponseWriter wrapper to capture status codes
type responseWriter struct {
	http.ResponseWriter
	statusCode int
}

func (rw *responseWriter) WriteHeader(code int) {
	rw.statusCode = code
	rw.ResponseWriter.WriteHeader(code)
}

// Enhanced security headers with proper CSP
func securityHeaders(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("X-Content-Type-Options", "nosniff")
		w.Header().Set("X-Frame-Options", "DENY")
		w.Header().Set("X-XSS-Protection", "1; mode=block")
		w.Header().Set("Referrer-Policy", "strict-origin-when-cross-origin")
		w.Header().Set("Strict-Transport-Security", "max-age=31536000; includeSubDomains")

		// More restrictive CSP with proper nonce support
		csp := "default-src 'self'; " +
			"script-src 'self' 'unsafe-inline' https://unpkg.com; " +
			"style-src 'self' 'unsafe-inline' https://cdnjs.cloudflare.com https://fonts.googleapis.com; " +
			"font-src 'self' https://fonts.gstatic.com https://cdnjs.cloudflare.com; " +
			"img-src 'self' https: data: github.com; " +
			"connect-src 'self' https:; " +
			"frame-ancestors 'none'; " +
			"base-uri 'self';"

		w.Header().Set("Content-Security-Policy", csp)
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

// Enhanced file serving with proper error handling
func safeServeFile(w http.ResponseWriter, r *http.Request, filename string) {
	// Check if file exists and is readable
	info, err := os.Stat(filename)
	if os.IsNotExist(err) {
		http.NotFound(w, r)
		return
	}
	if err != nil {
		log.Printf("Error accessing file %s: %v", filename, err)
		http.Error(w, "Internal server error", http.StatusInternalServerError)
		return
	}

	// Prevent directory traversal
	if info.IsDir() {
		http.NotFound(w, r)
		return
	}

	http.ServeFile(w, r, filename)
}

// Improved cache control with ETag support
func cacheControl(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		path := r.URL.Path
		var maxAge int

		switch {
		case strings.HasSuffix(path, ".css"):
			w.Header().Set("Content-Type", "text/css; charset=utf-8")
			maxAge = 31536000 // 1 year for CSS
		case strings.HasSuffix(path, ".js"):
			w.Header().Set("Content-Type", "application/javascript; charset=utf-8")
			maxAge = 31536000 // 1 year for JS
		case strings.HasSuffix(path, ".html"):
			w.Header().Set("Content-Type", "text/html; charset=utf-8")
			maxAge = 0 // No cache for HTML
		case strings.HasSuffix(path, ".png"):
			w.Header().Set("Content-Type", "image/png")
			maxAge = 31536000 // 1 year for images
		case strings.HasSuffix(path, ".jpg"), strings.HasSuffix(path, ".jpeg"):
			w.Header().Set("Content-Type", "image/jpeg")
			maxAge = 31536000
		case strings.HasSuffix(path, ".gif"):
			w.Header().Set("Content-Type", "image/gif")
			maxAge = 31536000
		case strings.HasSuffix(path, ".webp"):
			w.Header().Set("Content-Type", "image/webp")
			maxAge = 31536000
		case strings.HasSuffix(path, ".svg"):
			w.Header().Set("Content-Type", "image/svg+xml")
			maxAge = 31536000
		case strings.HasSuffix(path, ".ico"):
			w.Header().Set("Content-Type", "image/x-icon")
			maxAge = 31536000
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

	// Simple splash screen endpoint
	mux.HandleFunc("/splash", func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodGet {
			http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
			return
		}

		w.Header().Set("Content-Type", "text/html; charset=utf-8")
		w.Header().Set("Cache-Control", "no-cache, must-revalidate")

		safeServeFile(w, r, "public/partials/ui/splash.html")
	})

	// Enhanced API endpoints with better error handling
	mux.HandleFunc("/api/last-updated", func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodGet {
			http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
			return
		}

		w.Header().Set("Content-Type", "application/json; charset=utf-8")
		w.Header().Set("Cache-Control", "no-cache, must-revalidate")

		response := map[string]string{
			"timestamp": time.Now().Format(time.RFC3339),
		}

		if err := json.NewEncoder(w).Encode(response); err != nil {
			log.Printf("Error encoding JSON response: %v", err)
			http.Error(w, "Internal server error", http.StatusInternalServerError)
			return
		}
	})

	// Health check endpoint
	mux.HandleFunc("/health", func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodGet {
			http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
			return
		}

		w.Header().Set("Content-Type", "application/json; charset=utf-8")
		w.WriteHeader(http.StatusOK)
		json.NewEncoder(w).Encode(map[string]string{
			"status":    "healthy",
			"timestamp": time.Now().Format(time.RFC3339),
		})
	})

	// Serve static files with enhanced caching and security
	staticHandler := cacheControl(http.StripPrefix("/static/", http.FileServer(http.Dir("public/static"))))
	mux.Handle("/static/", staticHandler)

	// Serve partial files with proper headers and security
	mux.Handle("/partials/", http.StripPrefix("/partials/", http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodGet {
			http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
			return
		}

		// Prevent directory traversal
		cleanPath := filepath.Clean(r.URL.Path)
		if strings.Contains(cleanPath, "..") {
			http.NotFound(w, r)
			return
		}

		w.Header().Set("Content-Type", "text/html; charset=utf-8")
		w.Header().Set("Cache-Control", "no-cache, must-revalidate")

		safeServeFile(w, r, filepath.Join("public/partials", cleanPath))
	})))

	// Enhanced main route handler with better HTMX support and error handling
	mux.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodGet {
			http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
			return
		}

		// Enhanced HTMX integration headers
		w.Header().Set("Vary", "HX-Request")

		// Handle HTMX partial requests with optimized caching
		if r.Header.Get("HX-Request") == "true" {
			w.Header().Set("Cache-Control", "no-cache, must-revalidate")
		}

		// Route handling with enhanced security
		switch r.URL.Path {
		case "/":
			w.Header().Set("Content-Type", "text/html; charset=utf-8")
			safeServeFile(w, r, "public/index.html")
		case "/blog":
			w.Header().Set("Content-Type", "text/html; charset=utf-8")
			safeServeFile(w, r, "public/blog.html")
		case "/misc":
			w.Header().Set("Content-Type", "text/html; charset=utf-8")
			safeServeFile(w, r, "public/misc.html")
		default:
			http.NotFound(w, r)
		}
	})

	// Build middleware chain with proper ordering
	handler := loggingMiddleware(corsMiddleware(securityHeaders(mux)))

	// Enhanced server configuration with production-ready settings
	srv := &http.Server{
		Addr:           ":" + port,
		Handler:        handler,
		ReadTimeout:    15 * time.Second,
		WriteTimeout:   15 * time.Second,
		IdleTimeout:    60 * time.Second,
		MaxHeaderBytes: 1 << 20, // 1MB
	}

	// Graceful shutdown setup
	stop := make(chan os.Signal, 1)
	signal.Notify(stop, os.Interrupt, syscall.SIGTERM)

	go func() {
		log.Printf("🚀 Server starting on http://localhost:%s", port)
		log.Printf("📊 Health check available at http://localhost:%s/health", port)

		if err := srv.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			log.Fatalf("💥 Server failed to start: %v", err)
		}
	}()

	// Wait for interrupt signal for graceful shutdown
	<-stop
	log.Println("🛑 Shutting down server gracefully...")

	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()

	if err := srv.Shutdown(ctx); err != nil {
		log.Printf("⚠️ Error during server shutdown: %v", err)
	} else {
		log.Println("✅ Server stopped gracefully")
	}
}
