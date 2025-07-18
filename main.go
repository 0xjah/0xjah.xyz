package main

import (
	"log"
	"net/http"
	"os"
)

func main() {
	port := os.Getenv("PORT")
	if port == "" {
		port = "8080"
	}

	mux := http.NewServeMux()

	mux.Handle("/static/", http.StripPrefix("/static/", http.FileServer(http.Dir("public/static"))))

	mux.HandleFunc("/partials/ui/buttons.html", func(w http.ResponseWriter, r *http.Request) {
		http.ServeFile(w, r, "public/partials/ui/buttons.html")
	})

	mux.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
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
	log.Fatal(http.ListenAndServe(":"+port, mux))
}
