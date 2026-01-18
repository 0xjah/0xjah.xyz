# 0xjah.xyz C Server

Minimal, fast C web server with markdown blog support.

## Dependencies

```bash
# macOS
brew install curl openssl md4c

# Ubuntu/Debian
sudo apt install libcurl4-openssl-dev libssl-dev libmd4c-dev
```

## Build

```bash
gcc -o server main.c \
    -lcurl -lpthread -lssl -lcrypto -lmd4c-html \
    -O3 -Wall -Wextra

# macOS with Homebrew paths
gcc -o server main.c \
    -I/opt/homebrew/include \
    -L/opt/homebrew/lib \
    -lcurl -lpthread -lssl -lcrypto -lmd4c-html \
    -O3 -Wall -Wextra
```

## Run

```bash
# From project root
cd /path/to/0xjah.xyz
./server/server
```

## Environment Variables

Create a `.env` file in the project root:

```env
PORT=3000
GITHUB_REPO=0xjah/0xjah.xyz
CONTENT_DIR=content

# R2/S3 (optional, for gallery)
R2_ENDPOINT=r2.cloudflarestorage.com
R2_ACCESS_KEY=your_key
R2_SECRET_KEY=your_secret
R2_BUCKET=your_bucket
R2_PUBLIC_URL=your_public_domain
```

## Features

- **Static file serving** with proper MIME types
- **Markdown blog** with frontmatter support (uses md4c)
- **Gallery** with R2/S3 integration
- **GitHub status** API
- **Resume download** with proper PDF headers
- **HTMX support** for partial page updates
- **Multi-threaded** request handling

## Routes

| Route                | Description             |
| -------------------- | ----------------------- |
| `/`                  | Home page               |
| `/blog`              | Blog listing            |
| `/blog?post=name`    | Blog post (markdown)    |
| `/gallery`           | Image gallery           |
| `/misc`              | Misc page               |
| `/api/resume`        | Resume PDF download     |
| `/api/gallery`       | Gallery JSON API        |
| `/api/github-status` | GitHub last push status |
| `/health`            | Health check            |
| `/static/*`          | Static assets           |
