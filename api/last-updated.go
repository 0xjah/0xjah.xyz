package handler

import (
	"encoding/json"
	"fmt"
	"net/http"
	"os"
	"time"
)

type GitHubRepo struct {
	PushedAt string `json:"pushed_at"`
}

var githubToken = os.Getenv("GITHUB_TOKEN")

func Handler(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Access-Control-Allow-Origin", "*")
	w.Header().Set("Content-Type", "text/plain")

	url := "https://api.github.com/repos/0xjah/0xjah.me"
	req, _ := http.NewRequest("GET", url, nil)

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

	var repo GitHubRepo
	if err := json.NewDecoder(resp.Body).Decode(&repo); err != nil {
		http.Error(w, "Unavailable", http.StatusInternalServerError)
		return
	}

	t, err := time.Parse(time.RFC3339, repo.PushedAt)
	if err != nil {
		http.Error(w, "Unavailable", http.StatusInternalServerError)
		return
	}

	fmt.Fprintf(w, "%02d.%d", t.Month(), t.Year())
}
