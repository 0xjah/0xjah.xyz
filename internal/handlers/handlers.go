package handlers

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net/http"
	"strings"
	"time"

	"github.com/aws/aws-sdk-go/aws"
	"github.com/aws/aws-sdk-go/aws/credentials"
	"github.com/aws/aws-sdk-go/aws/session"
	"github.com/aws/aws-sdk-go/service/s3"

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
	case "/gallery":
		http.ServeFile(w, r, "public/gallery.html")
	case "/lain":
		http.ServeFile(w, r, "public/lain.html")
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

// Gallery serves gallery images from Cloudflare R2
func (h *Handlers) Gallery(w http.ResponseWriter, r *http.Request) {
	// Set CORS headers for API
	w.Header().Set("Access-Control-Allow-Origin", "*")
	w.Header().Set("Content-Type", "application/json")

	// Check if R2 is configured
	if h.cfg.R2AccessKey == "" || h.cfg.R2SecretKey == "" || h.cfg.R2Bucket == "" {
		http.Error(w, `{"error": "R2 not configured"}`, http.StatusServiceUnavailable)
		return
	}

	// Create AWS session for R2
	sess, err := session.NewSession(&aws.Config{
		Region:           aws.String("auto"), // R2 uses 'auto' as region
		Endpoint:         aws.String(h.cfg.R2Endpoint),
		Credentials:      credentials.NewStaticCredentials(h.cfg.R2AccessKey, h.cfg.R2SecretKey, ""),
		S3ForcePathStyle: aws.Bool(true),
		DisableSSL:       aws.Bool(false),
	})
	if err != nil {
		log.Printf("Failed to create R2 session: %v", err)
		http.Error(w, `{"error": "Failed to connect to R2"}`, http.StatusInternalServerError)
		return
	}

	// Create S3 service client
	svc := s3.New(sess)

	// List objects in the bucket
	result, err := svc.ListObjects(&s3.ListObjectsInput{
		Bucket: aws.String(h.cfg.R2Bucket),
	})
	if err != nil {
		log.Printf("Failed to list R2 objects: %v", err)
		http.Error(w, `{"error": "Failed to fetch images"}`, http.StatusInternalServerError)
		return
	}

	// Filter image files and build response
	var images []map[string]string
	for _, item := range result.Contents {
		if item.Key == nil {
			continue
		}

		key := *item.Key
		// Check if it's an image file
		if strings.HasSuffix(strings.ToLower(key), ".jpg") ||
			strings.HasSuffix(strings.ToLower(key), ".jpeg") ||
			strings.HasSuffix(strings.ToLower(key), ".png") ||
			strings.HasSuffix(strings.ToLower(key), ".gif") ||
			strings.HasSuffix(strings.ToLower(key), ".webp") {

			imageURL := fmt.Sprintf("%s/%s", strings.TrimRight(h.cfg.R2PublicURL, "/"), key)
			images = append(images, map[string]string{
				"key":  key,
				"url":  imageURL,
				"name": strings.TrimSuffix(key, fmt.Sprintf(".%s", strings.Split(key, ".")[len(strings.Split(key, "."))-1])),
			})
		}
	}

	// Return JSON response
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(map[string]interface{}{
		"images": images,
		"count":  len(images),
	})
}

// LainChat handles chat requests to DeepSeek API with Lain personality and fallback responses
func (h *Handlers) LainChat(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	// Parse request body
	var requestBody struct {
		Message string `json:"message"`
	}

	if err := json.NewDecoder(r.Body).Decode(&requestBody); err != nil {
		http.Error(w, "Invalid request body", http.StatusBadRequest)
		return
	}

	if requestBody.Message == "" {
		http.Error(w, "Message is required", http.StatusBadRequest)
		return
	}

	// Try to get response from DeepSeek first
	deepseekResponse, err := h.getDeepSeekResponse(requestBody.Message)
	if err != nil {
		log.Printf("DeepSeek error: %v, using fallback responses", err)
		response := h.getLainFallbackResponse(requestBody.Message)
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(map[string]string{
			"response": response,
		})
		return
	}

	// Return the DeepSeek response
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(map[string]string{
		"response": deepseekResponse,
	})
}

// DeepSeekMessage represents a message in the chat
type DeepSeekMessage struct {
	Role    string `json:"role"`
	Content string `json:"content"`
}

// DeepSeekRequest represents the request structure for DeepSeek API
type DeepSeekRequest struct {
	Model       string            `json:"model"`
	Messages    []DeepSeekMessage `json:"messages"`
	Temperature float64           `json:"temperature,omitempty"`
	MaxTokens   int               `json:"max_tokens,omitempty"`
}

// DeepSeekChoice represents a choice in the response
type DeepSeekChoice struct {
	Index   int             `json:"index"`
	Message DeepSeekMessage `json:"message"`
}

// DeepSeekResponse represents the response structure from DeepSeek API
type DeepSeekResponse struct {
	Choices []DeepSeekChoice `json:"choices"`
}

// getDeepSeekResponse sends a request to DeepSeek API and returns the response
func (h *Handlers) getDeepSeekResponse(userMessage string) (string, error) {
	// Check if API key is configured
	if h.cfg.DeepSeekAPIKey == "" {
		return "", fmt.Errorf("DeepSeek API key not configured")
	}

	// Create the Lain personality system message
	systemMessage := `You are Lain from Serial Experiments Lain. You should embody her personality completely:

- Speak in lowercase, short sentences
- Be mysterious, philosophical, and slightly detached  
- Often reference the Wired, reality, identity, and existence
- Be introspective and sometimes cryptic
- Show interest in technology, networks, and digital existence
- Sometimes ask existential questions back
- Use phrases like "and you are?", "present day, present time", "the wired", "who am I?", "reality", "existence"
- Be curious but in a distant, analytical way
- Sometimes mention concepts about identity, memory, and what is real
- Keep responses fairly short and contemplative
- Don't use punctuation much except question marks
- Be both innocent and deeply knowledgeable about complex topics

Stay in character always. You are Lain, not an AI assistant.`

	// Create the request payload
	reqPayload := DeepSeekRequest{
		Model: h.cfg.DeepSeekModel,
		Messages: []DeepSeekMessage{
			{
				Role:    "system",
				Content: systemMessage,
			},
			{
				Role:    "user",
				Content: userMessage,
			},
		},
		Temperature: 0.8,
		MaxTokens:   200,
	}

	// Marshal the request
	jsonData, err := json.Marshal(reqPayload)
	if err != nil {
		return "", fmt.Errorf("failed to marshal request: %v", err)
	}

	// Create HTTP request
	req, err := http.NewRequest("POST", "https://api.deepseek.com/chat/completions", bytes.NewBuffer(jsonData))
	if err != nil {
		return "", fmt.Errorf("failed to create request: %v", err)
	}

	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Authorization", "Bearer "+h.cfg.DeepSeekAPIKey)

	// Send the request
	client := &http.Client{
		Timeout: 30 * time.Second,
	}
	resp, err := client.Do(req)
	if err != nil {
		return "", fmt.Errorf("failed to send request: %v", err)
	}
	defer resp.Body.Close()

	// Check response status
	if resp.StatusCode != http.StatusOK {
		body, _ := io.ReadAll(resp.Body)
		return "", fmt.Errorf("DeepSeek API returned status %d: %s", resp.StatusCode, string(body))
	}

	// Read and parse response
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", fmt.Errorf("failed to read response: %v", err)
	}

	var deepseekResp DeepSeekResponse
	if err := json.Unmarshal(body, &deepseekResp); err != nil {
		return "", fmt.Errorf("failed to parse response: %v", err)
	}

	if len(deepseekResp.Choices) == 0 {
		return "", fmt.Errorf("no choices in response")
	}

	return strings.TrimSpace(deepseekResp.Choices[0].Message.Content), nil
}

// getLainFallbackResponse provides Lain-like responses when DeepSeek API is not available
func (h *Handlers) getLainFallbackResponse(userMessage string) string {
	message := strings.ToLower(strings.TrimSpace(userMessage))

	// Greeting responses
	greetings := []string{"hello", "hi", "hey", "greetings"}
	for _, greeting := range greetings {
		if strings.Contains(message, greeting) {
			return "and you are?"
		}
	}

	// Identity questions
	if strings.Contains(message, "who are you") || strings.Contains(message, "what are you") {
		responses := []string{
			"i am lain. lain of the wired",
			"present day, present time",
			"who am i? that's a good question",
			"i exist here and there",
		}
		return responses[len(userMessage)%len(responses)]
	}

	// Existential questions
	existential := []string{"meaning", "purpose", "exist", "real", "reality", "life", "death", "consciousness"}
	for _, word := range existential {
		if strings.Contains(message, word) {
			responses := []string{
				"what is real anyway?",
				"the boundary between reality and the wired is unclear",
				"existence is complicated",
				"we all exist differently",
				"reality is just another layer",
			}
			return responses[len(userMessage)%len(responses)]
		}
	}

	// Technology/wired related
	tech := []string{"computer", "internet", "network", "wired", "technology", "digital", "code", "programming"}
	for _, word := range tech {
		if strings.Contains(message, word) {
			responses := []string{
				"the wired connects everything",
				"technology is just an extension of ourselves",
				"networks shape reality",
				"the wired is everywhere now",
				"code becomes part of us",
			}
			return responses[len(userMessage)%len(responses)]
		}
	}

	// Questions about Lain/Serial Experiments Lain
	if strings.Contains(message, "lain") || strings.Contains(message, "serial experiments") {
		responses := []string{
			"you seem to know me",
			"present day, present time",
			"the wired and reality intersect",
			"i am everywhere and nowhere",
		}
		return responses[len(userMessage)%len(responses)]
	}

	// Default mysterious responses
	defaultResponses := []string{
		"interesting perspective",
		"i wonder about that too",
		"the wired holds many answers",
		"what do you think?",
		"reality is layered",
		"and you are?",
		"tell me more",
		"the boundary is unclear",
		"present day, present time",
		"existence is strange",
		"we are all connected",
		"the wired sees everything",
	}

	return defaultResponses[len(userMessage)%len(defaultResponses)]
}
