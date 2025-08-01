package middleware

import (
	"fmt"
	"net/http"
	"strings"

	"0xjah.me/internal/config"
)

// SecurityHeaders adds security headers to all responses
func SecurityHeaders(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("X-Content-Type-Options", "nosniff")
		w.Header().Set("X-Frame-Options", "DENY")
		w.Header().Set("X-XSS-Protection", "1; mode=block")
		w.Header().Set("Referrer-Policy", "strict-origin-when-cross-origin")
		w.Header().Set("Content-Security-Policy", "default-src 'self' https:; script-src 'self' 'unsafe-inline' https://unpkg.com; style-src 'self' 'unsafe-inline' https://cdnjs.cloudflare.com https://fonts.googleapis.com; font-src 'self' https://fonts.gstatic.com https://cdnjs.cloudflare.com; img-src 'self' https: data:;")
		next.ServeHTTP(w, r)
	})
}

// CORS adds CORS headers to responses
func CORS(cfg *config.Config) func(http.Handler) http.Handler {
	return func(next http.Handler) http.Handler {
		return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			w.Header().Set("Access-Control-Allow-Origin", cfg.CORSOrigins)
			w.Header().Set("Access-Control-Allow-Methods", "GET, OPTIONS")
			w.Header().Set("Access-Control-Allow-Headers", "Accept, Content-Type, Content-Length, Accept-Encoding, Authorization, HX-Request, HX-Trigger")

			if r.Method == "OPTIONS" {
				w.WriteHeader(http.StatusOK)
				return
			}

			next.ServeHTTP(w, r)
		})
	}
}

// CacheControl adds cache control headers for static assets
func CacheControl(cfg *config.Config) func(http.Handler) http.Handler {
	return func(next http.Handler) http.Handler {
		return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			path := r.URL.Path
			var maxAge int

			switch {
			case strings.HasSuffix(path, ".css"):
				w.Header().Set("Content-Type", "text/css")
				maxAge = cfg.StaticCacheTTL
			case strings.HasSuffix(path, ".js"):
				w.Header().Set("Content-Type", "application/javascript")
				maxAge = cfg.StaticCacheTTL
			case strings.HasSuffix(path, ".html"):
				w.Header().Set("Content-Type", "text/html; charset=utf-8")
				maxAge = cfg.HTMLCacheTTL
			case strings.HasSuffix(path, ".png"), strings.HasSuffix(path, ".jpg"), strings.HasSuffix(path, ".gif"), strings.HasSuffix(path, ".webp"):
				maxAge = cfg.StaticCacheTTL
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
}
