# 0xjah.me

Personal portfolio website built with Go and HTMX.

## Quick Start

### Local Development
```bash
# Clone and run
git clone <repo>
cd 0xjah.xyz
go run main.go
```

Visit: http://localhost:3000

### Docker
```bash
# Build and run
docker build -t 0xjah-site .
docker run -p 3000:3000 0xjah-site

# Or use docker-compose
docker-compose up
```

## Project Structure
```
├── main.go              # Server
├── public/              # Static files
│   ├── index.html       # Home page  
│   ├── blog.html        # Blog page
│   ├── misc.html        # Misc page
│   ├── partials/        # HTMX partials
│   └── static/          # CSS, images, etc
├── Dockerfile           # Container config
├── docker-compose.yml   # Multi-container setup
└── .env.example         # Environment template
```

## Environment Variables
```bash
PORT=3000               # Server port
ENV=development         # Environment mode
```

## Features
- ✅ Per-tab splash screen
- ✅ HTMX navigation  
- ✅ Responsive design
- ✅ Docker support
- ✅ Health check endpoint

## API Endpoints
- `GET /` - Home page
- `GET /blog` - Blog page  
- `GET /misc` - Misc page
- `GET /health` - Health check
- `GET /api/last-updated` - Last updated timestamp

Built with ❤️ using Go + HTMX
