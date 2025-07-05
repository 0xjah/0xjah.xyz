package main

import (
	"embed"
	"io/fs"
	"log"
	"net/http"
	"os"
	"0xjah/backend"
)

//go:embed templates/* static/*
var embeddedFS embed.FS

func main() {
	port := ":8080"
	if envPort := os.Getenv("PORT"); envPort != "" {
		port = ":" + envPort
	}

	templateFS, err := fs.Sub(embeddedFS, "templates")
	if err != nil {
		log.Fatalf("failed to load templates: %v", err)
	}
	staticFS, err := fs.Sub(embeddedFS, "static")
	if err != nil {
		log.Fatalf("failed to load static: %v", err)
	}

	mux := http.NewServeMux()
	backend.RegisterRoutes(mux, templateFS, staticFS)

	server := &http.Server{
		Addr:    port,
		Handler: mux,
	}

	log.Printf("Server running on http://localhost%s", port)
	log.Fatal(server.ListenAndServe())
}
