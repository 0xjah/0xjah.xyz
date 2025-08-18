# Deployment Guide for Render

## Prerequisites

1. **Render Account**: Sign up at [render.com](https://render.com)
2. **GitHub Repository**: Your code should be in a GitHub repository
3. **Render Plan**: You need at least the **Starter Plan** ($7/month) for Docker support and sufficient resources for Ollama

## Important Notes

⚠️ **Resource Requirements**: 
- Ollama + Llama 3.2 1B model requires at least 1.5GB RAM
- Recommended: **Standard Plan** (2GB RAM, $25/month) or higher
- Free tier will NOT work with this setup

## Deployment Steps

### Option 1: Using render.yaml (Recommended)

1. **Push your code** to GitHub with all the files:
   - `Dockerfile`
   - `start.sh`
   - `render.yaml`
   - Your Go application code

2. **Connect to Render**:
   - Go to [Render Dashboard](https://dashboard.render.com)
   - Click "New" → "Blueprint"
   - Connect your GitHub repository
   - Render will automatically detect `render.yaml`

3. **Configure Environment Variables** (if needed):
   ```
   PORT=3000
   HOST=0.0.0.0
   ENV=production
   OLLAMA_HOST=http://localhost:11434
   OLLAMA_MODEL=llama3.2:1b
   ```

### Option 2: Manual Web Service Creation

1. **Create Web Service**:
   - Go to Render Dashboard
   - Click "New" → "Web Service"
   - Connect your GitHub repository

2. **Configure Build Settings**:
   - **Environment**: Docker
   - **Dockerfile Path**: `./Dockerfile`
   - **Build Command**: (leave empty, Docker handles this)
   - **Start Command**: (leave empty, Docker handles this)

3. **Set Environment Variables**:
   ```
   PORT=3000
   HOST=0.0.0.0
   ENV=production
   OLLAMA_HOST=http://localhost:11434
   OLLAMA_MODEL=llama3.2:1b
   ```

4. **Choose Plan**: Select **Starter** or higher (Standard recommended)

## Deployment Process

1. **Build Time**: ~10-15 minutes (includes Ollama installation)
2. **First Start**: ~5-10 minutes (includes model download ~1.3GB)
3. **Subsequent Starts**: ~2-3 minutes

## Health Checks

The application includes:
- Health check endpoint at `/health`
- Automatic fallback if Ollama fails
- Graceful error handling

## Monitoring

- Check logs in Render Dashboard
- Monitor memory usage (Ollama uses ~800MB-1GB)
- Watch for model download completion in startup logs

## Fallback Strategy

If Ollama fails to start or model download fails:
- Application continues running
- Chatbot uses pre-programmed Lain responses
- No service downtime

## Cost Estimation

- **Starter Plan**: $7/month (might be too limited)
- **Standard Plan**: $25/month (recommended)
- **Pro Plan**: $85/month (best performance)

## Troubleshooting

### Common Issues:

1. **Out of Memory**: Upgrade to Standard plan or higher
2. **Model Download Timeout**: Will retry automatically, fallback responses work
3. **Slow Response**: Expected on Starter plan, upgrade for better performance

### Logs to Check:
```bash
# In Render dashboard, check for:
"Starting Ollama server..."
"Ollama server is ready!"
"Model pulled successfully"
"Starting Go application..."
```

## Alternative: Lightweight Deployment

If Ollama is too resource-intensive, you can:

1. **Remove Ollama** from Dockerfile
2. **Set OLLAMA_HOST=""** to disable AI features  
3. **Use fallback responses only** (still provides Lain personality)
4. **Deploy on Free tier** if desired

This gives you a working chatbot with authentic Lain responses without AI costs.
