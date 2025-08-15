package handlers

import (
	"encoding/json"
	"fmt"
	"io"
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

// Routes serves main application routes - only full pages
func (h *Handlers) Routes(w http.ResponseWriter, r *http.Request) {
	switch r.URL.Path {
	case "/":
		http.ServeFile(w, r, "public/index.html")
	case "/blog":
		postParam := r.URL.Query().Get("post")
		if postParam != "" {
			// Serve the specific blog post partial
			postFile := fmt.Sprintf("public/partials/blog/%s.html", postParam)
			http.ServeFile(w, r, postFile)
		} else {
			http.ServeFile(w, r, "public/blog.html")
		}
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
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.Header().Set("Cache-Control", "no-cache, must-revalidate")

	if err := json.NewEncoder(w).Encode(map[string]string{
		"timestamp": time.Now().Format(time.RFC3339),
	}); err != nil {
		http.Error(w, "Internal server error", http.StatusInternalServerError)
		log.Printf("Error encoding JSON: %v", err)
	}
}

// HealthCheck provides a simple health check endpoint
func (h *Handlers) HealthCheck(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.Header().Set("Cache-Control", "no-cache")
	w.WriteHeader(http.StatusOK)

	response := map[string]interface{}{
		"status":      "healthy",
		"timestamp":   time.Now().Format(time.RFC3339),
		"environment": h.cfg.Environment,
	}

	if err := json.NewEncoder(w).Encode(response); err != nil {
		log.Printf("Error encoding JSON: %v", err)
	}
}

// GitHubRepoLastUpdatedHTML returns the last update time from GitHub repository as HTML
func (h *Handlers) GitHubRepoLastUpdatedHTML(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	w.Header().Set("Cache-Control", "public, max-age=300, stale-while-revalidate=600") // Optimized caching

	// Create HTTP client with optimized settings
	client := &http.Client{
		Timeout: 8 * time.Second, // Reduced timeout for better responsiveness
	}

	// GitHub API endpoint for repository information
	apiURL := fmt.Sprintf("https://api.github.com/repos/%s", h.cfg.GitHubRepo)

	// Create request
	req, err := http.NewRequest("GET", apiURL, nil)
	if err != nil {
		w.Write([]byte("error fetching repo data"))
		log.Printf("Error creating GitHub API request: %v", err)
		return
	}

	// Add optimized headers
	if h.cfg.GitHubToken != "" {
		req.Header.Set("Authorization", "token "+h.cfg.GitHubToken)
	}
	req.Header.Set("Accept", "application/vnd.github.v3+json")
	req.Header.Set("User-Agent", "0xjah-website/1.0")

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
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.Header().Set("Cache-Control", "public, max-age=300, stale-while-revalidate=600") // Optimized caching

	// Create HTTP client with optimized settings
	client := &http.Client{
		Timeout: 8 * time.Second, // Reduced timeout
	}

	// GitHub API endpoint for repository information
	apiURL := fmt.Sprintf("https://api.github.com/repos/%s", h.cfg.GitHubRepo)

	// Create request
	req, err := http.NewRequest("GET", apiURL, nil)
	if err != nil {
		http.Error(w, `{"error":"Failed to create request"}`, http.StatusInternalServerError)
		log.Printf("Error creating GitHub API request: %v", err)
		return
	}

	// Add optimized headers
	if h.cfg.GitHubToken != "" {
		req.Header.Set("Authorization", "token "+h.cfg.GitHubToken)
	}
	req.Header.Set("Accept", "application/vnd.github.v3+json")
	req.Header.Set("User-Agent", "0xjah-website/1.0")

	// Make request
	resp, err := client.Do(req)
	if err != nil {
		http.Error(w, `{"error":"Failed to fetch repository data"}`, http.StatusInternalServerError)
		log.Printf("Error fetching GitHub repository data: %v", err)
		return
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		http.Error(w, `{"error":"GitHub API request failed"}`, http.StatusInternalServerError)
		log.Printf("GitHub API returned status: %d", resp.StatusCode)
		return
	}

	// Parse GitHub API response
	var repoData struct {
		UpdatedAt string `json:"updated_at"`
		PushedAt  string `json:"pushed_at"`
	}

	if err := json.NewDecoder(resp.Body).Decode(&repoData); err != nil {
		http.Error(w, `{"error":"Failed to parse response"}`, http.StatusInternalServerError)
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
		http.Error(w, `{"error":"Failed to parse timestamp"}`, http.StatusInternalServerError)
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

// DiscordStatus fetches Discord status from Lanyard API and returns as HTML
func (h *Handlers) DiscordStatus(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "text/html")

	if r.Method != http.MethodGet {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	// Lanyard API endpoint
	apiURL := fmt.Sprintf("https://api.lanyard.rest/v1/users/%s", h.cfg.DiscordID)

	// Create HTTP client with timeout
	client := &http.Client{Timeout: 5 * time.Second}

	req, err := http.NewRequest("GET", apiURL, nil)
	if err != nil {
		log.Printf("Error creating Discord API request: %v", err)
		w.WriteHeader(http.StatusInternalServerError)
		fmt.Fprint(w, `<div class="status-header"><h2>Status</h2><p class="quote error">Status unavailable</p></div>`)
		return
	}

	req.Header.Set("User-Agent", "0xjah.xyz/1.0")

	resp, err := client.Do(req)
	if err != nil {
		log.Printf("Error fetching Discord status: %v", err)
		w.WriteHeader(http.StatusInternalServerError)
		fmt.Fprint(w, `<div class="status-header"><h2>Status</h2><p class="quote error">Status unavailable</p></div>`)
		return
	}
	defer resp.Body.Close()

	// Read the response body
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		log.Printf("Error reading Discord API response: %v", err)
		w.WriteHeader(http.StatusInternalServerError)
		fmt.Fprint(w, `<div class="status-header"><h2>Status</h2><p class="quote error">Status unavailable</p></div>`)
		return
	}

	if resp.StatusCode != http.StatusOK {
		log.Printf("Discord API returned status: %d", resp.StatusCode)

		// Try to parse the error response for better debugging
		var errorResp struct {
			Success bool `json:"success"`
			Error   struct {
				Code    string `json:"code"`
				Message string `json:"message"`
			} `json:"error"`
		}

		if err := json.Unmarshal(body, &errorResp); err == nil {
			log.Printf("Discord API error: %s - %s", errorResp.Error.Code, errorResp.Error.Message)

			// If user is not monitored, show a fallback status
			if errorResp.Error.Code == "user_not_monitored" {
				html := fmt.Sprintf(`<div class="status-header" hx-classes="add fade-in:200ms">
					<h2>Status</h2>
					<p class="quote" 
					   hx-on:click="htmx.trigger(this.parentElement, 'hx:refresh'); htmx.trigger('#repo-timestamp', 'refresh')"
					   style="cursor: pointer"
					   title="Click to refresh">
						%s
					</p>
					<small style="opacity: 0.6; font-size: 0.75em; display: block; margin-top: 4px">
						Last updated:
						<span id="repo-timestamp"
							  hx-get="/api/github-last-updated-html"
							  hx-trigger="load, every 5m, refresh"
							  hx-swap="innerHTML"
							  title="Last GitHub repository update">
							loading...
						</span>
					</small>
				</div>`, h.cfg.FallbackStatus)
				fmt.Fprint(w, html)
				return
			}
		}

		w.WriteHeader(http.StatusInternalServerError)
		fmt.Fprint(w, `<div class="status-header"><h2>Status</h2><p class="quote error">Status unavailable</p></div>`)
		return
	}

	// Parse Lanyard API response
	var lanyardResp struct {
		Success bool `json:"success"`
		Data    struct {
			DiscordUser struct {
				Username string `json:"username"`
			} `json:"discord_user"`
			DiscordStatus string `json:"discord_status"`
			Activities    []struct {
				Name    string `json:"name"`
				Type    int    `json:"type"`
				Details string `json:"details,omitempty"`
				State   string `json:"state,omitempty"`
			} `json:"activities"`
			Listening struct {
				Song   string `json:"song"`
				Artist string `json:"artist"`
				Album  string `json:"album"`
			} `json:"spotify,omitempty"`
		} `json:"data"`
	}

	if err := json.Unmarshal(body, &lanyardResp); err != nil {
		log.Printf("Error parsing Discord status response: %v", err)
		w.WriteHeader(http.StatusInternalServerError)
		fmt.Fprint(w, `<div class="status-header"><h2>Status</h2><p class="quote error">Status unavailable</p></div>`)
		return
	}

	if !lanyardResp.Success {
		log.Println("Discord API response was not successful")
		w.WriteHeader(http.StatusInternalServerError)
		fmt.Fprint(w, `<div class="status-header"><h2>Status</h2><p class="quote error">Status unavailable</p></div>`)
		return
	}

	// Determine status info
	status := lanyardResp.Data.DiscordStatus
	if status == "" {
		status = "offline"
	}

	// Check for Spotify activity
	var activity string
	var listening string

	if lanyardResp.Data.Listening.Song != "" {
		listening = fmt.Sprintf("listening to %s by %s", lanyardResp.Data.Listening.Song, lanyardResp.Data.Listening.Artist)
	} else {
		// Check other activities
		for _, act := range lanyardResp.Data.Activities {
			if act.Type == 0 { // Playing
				activity = fmt.Sprintf("playing %s", act.Name)
				break
			} else if act.Type == 2 { // Listening (Spotify should be caught above)
				activity = fmt.Sprintf("listening to %s", act.Name)
				break
			} else if act.Type == 3 { // Watching
				activity = fmt.Sprintf("watching %s", act.Name)
				break
			}
		}
	}

	// Build status text
	statusText := status
	if listening != "" {
		statusText = fmt.Sprintf("%s • %s", status, listening)
	} else if activity != "" {
		statusText = fmt.Sprintf("%s • %s", status, activity)
	}

	// Return HTML response with Discord status
	html := fmt.Sprintf(`<div class="status-header" hx-classes="add fade-in:200ms">
		<h2>Status</h2>
		<p class="quote" 
		   hx-on:click="htmx.trigger(this.parentElement, 'hx:refresh'); htmx.trigger('#repo-timestamp', 'refresh')"
		   style="cursor: pointer"
		   title="Click to refresh">
			%s
		</p>
		<small style="opacity: 0.6; font-size: 0.75em; display: block; margin-top: 4px">
			Last updated:
			<span id="repo-timestamp"
				  hx-get="/api/github-last-updated-html"
				  hx-trigger="load, every 5m, refresh"
				  hx-swap="innerHTML"
				  title="Last GitHub repository update">
				loading...
			</span>
		</small>
	</div>`, statusText)

	fmt.Fprint(w, html)
}
