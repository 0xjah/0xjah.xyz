package main

import (
	"context"
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"os"
	"os/signal"
	"path"
	"strings"
	"syscall"
	"time"
)

type GitHubRepo struct {
	PushedAt string `json:"pushed_at"`
}

var githubToken = os.Getenv("GITHUB_TOKEN")

type captureResponseWriter struct {
	http.ResponseWriter
	status      int
	wroteHeader bool
}

func (w *captureResponseWriter) WriteHeader(status int) {
	if w.wroteHeader {
		return
	}
	w.status = status
	w.ResponseWriter.WriteHeader(status)
	w.wroteHeader = true
}

func (w *captureResponseWriter) Write(b []byte) (int, error) {
	if !w.wroteHeader {
		w.WriteHeader(http.StatusOK)
	}
	return w.ResponseWriter.Write(b)
}

type notFoundWrapper struct {
	handler http.Handler
}

func (w *notFoundWrapper) ServeHTTP(rw http.ResponseWriter, r *http.Request) {
	crw := &captureResponseWriter{ResponseWriter: rw}
	w.handler.ServeHTTP(crw, r)

	if crw.status == 0 {
		crw.status = http.StatusOK
	}

	if crw.status == http.StatusNotFound && !crw.wroteHeader {
		rw.Header().Set("Content-Type", "text/html; charset=utf-8")
		rw.WriteHeader(http.StatusNotFound)
		fmt.Fprint(rw, `<html><head><title>404 Not Found</title></head><body><h1>404 - Page Not Found</h1><p>The page you requested does not exist.</p></body></html>`)
	}
}

func main() {
	port := os.Getenv("PORT")
	if port == "" {
		port = "8080"
	}

	mux := http.NewServeMux()

	mux.Handle("/static/", securityHeaders(http.StripPrefix("/static/", http.FileServer(http.Dir("static")))))
	mux.Handle("/static/partials/", securityHeaders(http.StripPrefix("/static/partials/", http.FileServer(http.Dir("partials")))))

	mux.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/" {
			notFoundHandler(w, r)
			return
		}
		http.ServeFile(w, r, "index.html")
	})

	mux.HandleFunc("/pages/", func(w http.ResponseWriter, r *http.Request) {
		page := strings.TrimPrefix(r.URL.Path, "/pages/")
		if page == "" {
			http.Redirect(w, r, "/", http.StatusFound)
			return
		}
		page = path.Clean(page)
		if strings.Contains(page, "..") {
			http.Error(w, "Invalid page path", http.StatusBadRequest)
			return
		}
		file := fmt.Sprintf("./%s.html", page)
		if _, err := os.Stat(file); os.IsNotExist(err) {
			notFoundHandler(w, r)
			return
		}
		http.ServeFile(w, r, file)
	})

	mux.HandleFunc("/api/last-updated", lastUpdatedHandler)

	handler := loggingMiddleware(&notFoundWrapper{handler: mux})

	srv := &http.Server{
		Addr:    ":" + port,
		Handler: handler,
	}

	go func() {
		log.Printf("Server started on http://localhost:%s\n", port)
		if err := srv.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			log.Fatalf("ListenAndServe failed: %v", err)
		}
	}()

	stop := make(chan os.Signal, 1)
	signal.Notify(stop, os.Interrupt, syscall.SIGTERM)
	<-stop

	log.Println("Shutting down server...")
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	if err := srv.Shutdown(ctx); err != nil {
		log.Fatalf("Server Shutdown Failed: %v", err)
	}

	log.Println("Server stopped gracefully")
}

func lastUpdatedHandler(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Access-Control-Allow-Origin", "*")
	w.Header().Set("Content-Type", "text/plain")

	username := "0xjah"
	repo := "0xjah.xyz"
	url := fmt.Sprintf("https://api.github.com/repos/%s/%s", username, repo)

	req, err := http.NewRequest("GET", url, nil)
	if err != nil {
		http.Error(w, "Unavailable", http.StatusServiceUnavailable)
		return
	}

	if githubToken != "" {
		req.Header.Set("Authorization", "Bearer "+githubToken)
	}

	client := &http.Client{Timeout: 10 * time.Second}
	resp, err := client.Do(req)
	if err != nil || resp.StatusCode != 200 {
		http.Error(w, "Unavailable", http.StatusServiceUnavailable)
		return
	}
	defer resp.Body.Close()

	var repoData GitHubRepo
	if err := json.NewDecoder(resp.Body).Decode(&repoData); err != nil {
		http.Error(w, "Unavailable", http.StatusInternalServerError)
		return
	}

	t, err := time.Parse(time.RFC3339, repoData.PushedAt)
	if err != nil {
		http.Error(w, "Unavailable", http.StatusInternalServerError)
		return
	}

	fmt.Fprintf(w, "%02d.%d", t.Month(), t.Year())
}

func notFoundHandler(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	w.WriteHeader(http.StatusNotFound)
	fmt.Fprint(w, `<html><head><title>404 Not Found</title></head><body><h1>404 - Page Not Found</h1><p>The page you requested does not exist.</p></body></html>`)
}

func loggingMiddleware(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		start := time.Now()
		next.ServeHTTP(w, r)
		log.Printf("%s %s %v", r.Method, r.URL.Path, time.Since(start))
	})
}

func securityHeaders(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		headers := w.Header()
		headers.Set("X-Content-Type-Options", "nosniff")
		headers.Set("X-Frame-Options", "DENY")
		headers.Set("Referrer-Policy", "no-referrer-when-downgrade")
		headers.Set("Content-Security-Policy", "default-src 'self';")
		next.ServeHTTP(w, r)
	})
}
