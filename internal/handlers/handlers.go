package handlers

import (
	"encoding/json"
	"fmt"
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

	isHTMXRequest := r.Header.Get("HX-Request") == "true"

	switch r.URL.Path {
	case "/":
		if isHTMXRequest {
			// Return only the content for HTMX requests
			http.ServeFile(w, r, "public/partials/pages/index_content.html")
		} else {
			http.ServeFile(w, r, "public/index.html")
		}
	case "/blog":
		if isHTMXRequest {
			// Return only the content for HTMX requests
			http.ServeFile(w, r, "public/partials/pages/blog_content.html")
		} else {
			http.ServeFile(w, r, "public/blog.html")
		}
	case "/misc":
		if isHTMXRequest {
			// Return only the content for HTMX requests
			http.ServeFile(w, r, "public/partials/pages/misc_content.html")
		} else {
			http.ServeFile(w, r, "public/misc.html")
		}
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

// GitHubRepoLastUpdatedHTML returns the last update time from GitHub repository as HTML
func (h *Handlers) GitHubRepoLastUpdatedHTML(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "text/html")
	w.Header().Set("Cache-Control", "max-age=300") // Cache for 5 minutes

	// GitHub API endpoint for repository information
	apiURL := fmt.Sprintf("https://api.github.com/repos/%s", h.cfg.GitHubRepo)

	// Create HTTP client with timeout
	client := &http.Client{
		Timeout: 10 * time.Second,
	}

	// Create request
	req, err := http.NewRequest("GET", apiURL, nil)
	if err != nil {
		w.Write([]byte("error fetching repo data"))
		log.Printf("Error creating GitHub API request: %v", err)
		return
	}

	// Add GitHub token if available
	if h.cfg.GitHubToken != "" {
		req.Header.Set("Authorization", "token "+h.cfg.GitHubToken)
	}
	req.Header.Set("Accept", "application/vnd.github.v3+json")

	// Make request
	resp, err := client.Do(req)
	if err != nil {
		w.Write([]byte("error fetching repo data"))
		log.Printf("Error fetching GitHub repository data: %v", err)
		return
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		w.Write([]byte("error fetching repo data"))
		log.Printf("GitHub API returned status: %d", resp.StatusCode)
		return
	}

	// Parse GitHub API response
	var repoData struct {
		UpdatedAt string `json:"updated_at"`
		PushedAt  string `json:"pushed_at"`
	}

	if err := json.NewDecoder(resp.Body).Decode(&repoData); err != nil {
		w.Write([]byte("error parsing repo data"))
		log.Printf("Error parsing GitHub API response: %v", err)
		return
	}

	// Use pushed_at as it represents the last push to the repository
	lastUpdate := repoData.PushedAt
	if lastUpdate == "" {
		lastUpdate = repoData.UpdatedAt
	}

	// Parse and format the timestamp
	parsedTime, err := time.Parse(time.RFC3339, lastUpdate)
	if err != nil {
		w.Write([]byte("error parsing timestamp"))
		log.Printf("Error parsing GitHub timestamp: %v", err)
		return
	}

	// Return formatted HTML
	relativeTime := getRelativeTime(parsedTime)
	formattedTime := parsedTime.Format("2006-01-02 15:04:05 MST")

	html := fmt.Sprintf(`<span title="%s">%s</span>`, formattedTime, relativeTime)
	w.Write([]byte(html))
}

// GitHubRepoLastUpdated returns the last update time from GitHub repository
func (h *Handlers) GitHubRepoLastUpdated(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json")
	w.Header().Set("Cache-Control", "max-age=300") // Cache for 5 minutes

	// GitHub API endpoint for repository information
	apiURL := fmt.Sprintf("https://api.github.com/repos/%s", h.cfg.GitHubRepo)

	// Create HTTP client with timeout
	client := &http.Client{
		Timeout: 10 * time.Second,
	}

	// Create request
	req, err := http.NewRequest("GET", apiURL, nil)
	if err != nil {
		http.Error(w, "Failed to create request", http.StatusInternalServerError)
		log.Printf("Error creating GitHub API request: %v", err)
		return
	}

	// Add GitHub token if available
	if h.cfg.GitHubToken != "" {
		req.Header.Set("Authorization", "token "+h.cfg.GitHubToken)
	}
	req.Header.Set("Accept", "application/vnd.github.v3+json")

	// Make request
	resp, err := client.Do(req)
	if err != nil {
		http.Error(w, "Failed to fetch repository data", http.StatusInternalServerError)
		log.Printf("Error fetching GitHub repository data: %v", err)
		return
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		http.Error(w, "GitHub API request failed", http.StatusInternalServerError)
		log.Printf("GitHub API returned status: %d", resp.StatusCode)
		return
	}

	// Parse GitHub API response
	var repoData struct {
		UpdatedAt string `json:"updated_at"`
		PushedAt  string `json:"pushed_at"`
	}

	if err := json.NewDecoder(resp.Body).Decode(&repoData); err != nil {
		http.Error(w, "Failed to parse response", http.StatusInternalServerError)
		log.Printf("Error parsing GitHub API response: %v", err)
		return
	}

	// Use pushed_at as it represents the last push to the repository
	lastUpdate := repoData.PushedAt
	if lastUpdate == "" {
		lastUpdate = repoData.UpdatedAt
	}

	// Parse and format the timestamp
	parsedTime, err := time.Parse(time.RFC3339, lastUpdate)
	if err != nil {
		http.Error(w, "Failed to parse timestamp", http.StatusInternalServerError)
		log.Printf("Error parsing GitHub timestamp: %v", err)
		return
	}

	// Return formatted response
	response := map[string]string{
		"timestamp":     parsedTime.Format("2006-01-02 15:04:05 MST"),
		"iso_timestamp": parsedTime.Format(time.RFC3339),
		"relative_time": getRelativeTime(parsedTime),
	}

	if err := json.NewEncoder(w).Encode(response); err != nil {
		http.Error(w, "Internal server error", http.StatusInternalServerError)
		log.Printf("Error encoding JSON: %v", err)
		return
	}
}

// getRelativeTime returns a human-readable relative time string
func getRelativeTime(t time.Time) string {
	now := time.Now()
	diff := now.Sub(t)

	switch {
	case diff < time.Minute:
		return "just now"
	case diff < time.Hour:
		minutes := int(diff.Minutes())
		if minutes == 1 {
			return "1 minute ago"
		}
		return fmt.Sprintf("%d minutes ago", minutes)
	case diff < 24*time.Hour:
		hours := int(diff.Hours())
		if hours == 1 {
			return "1 hour ago"
		}
		return fmt.Sprintf("%d hours ago", hours)
	case diff < 7*24*time.Hour:
		days := int(diff.Hours() / 24)
		if days == 1 {
			return "1 day ago"
		}
		return fmt.Sprintf("%d days ago", days)
	case diff < 30*24*time.Hour:
		weeks := int(diff.Hours() / (24 * 7))
		if weeks == 1 {
			return "1 week ago"
		}
		return fmt.Sprintf("%d weeks ago", weeks)
	default:
		months := int(diff.Hours() / (24 * 30))
		if months == 1 {
			return "1 month ago"
		}
		return fmt.Sprintf("%d months ago", months)
	}
}
