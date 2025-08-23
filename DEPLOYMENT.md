# Deployment Guide for Render

## Prerequisites

1. **Render Account**: Sign up at [render.com](https://render.com)
2. **GitHub Repository**: Your code should be in a GitHub repository
3. **DeepSeek API Key**: Get your free API key from [DeepSeek](https://platform.deepseek.com/)
4. **Render Plan**: Free tier works fine now! No special requirements.

## Important Notes

✅ **Resource Requirements**: 
- Much lighter than previous Ollama setup
- Free tier is sufficient
- No local AI model storage needed

## Deployment Steps

### Option 1: Using render.yaml (Recommended)

1. **Push your code** to GitHub with all the files:
   - `Dockerfile`
   - `render.yaml`
   - Your Go application code

2. **Connect to Render**:
   - Go to [Render Dashboard](https://dashboard.render.com)
   - Click "New" → "Blueprint"
   - Connect your GitHub repository
   - Render will automatically detect `render.yaml`

3. **Configure Environment Variables**:
   ```
   PORT=3000
   HOST=0.0.0.0
   ENV=production
   DEEPSEEK_API_KEY=your_deepseek_api_key_here
   DEEPSEEK_MODEL=deepseek-chat
   ```
   
   **Important**: Set `DEEPSEEK_API_KEY` as a secret in Render dashboard for security.

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
   DEEPSEEK_API_KEY=your_deepseek_api_key_here
   DEEPSEEK_MODEL=deepseek-chat
   ```

4. **Choose Plan**: Free tier works perfectly fine now!

## Deployment Process

1. **Build Time**: ~2-5 minutes (much faster without Ollama)
2. **First Start**: ~30 seconds
3. **Subsequent Starts**: ~10-15 seconds

## Health Checks

The application includes:
- Health check endpoint at `/health`
- Automatic fallback if DeepSeek API fails
- Graceful error handling

## Monitoring

- Check logs in Render Dashboard
- Monitor API usage on DeepSeek platform
- Much lower resource usage overall

## Fallback Strategy

If DeepSeek API fails or quota exceeded:
- Application continues running
- Chatbot uses pre-programmed Lain responses
- No service downtime

## Cost Estimation

- **Free Tier**: $0/month (now sufficient!)
- **Starter Plan**: $7/month (if you need premium features)

## Troubleshooting

### Common Issues:

1. **API Key Invalid**: Check your DeepSeek API key
2. **Rate Limiting**: Free tier has generous limits, upgrade if needed
3. **Network Issues**: Check DeepSeek API status

### Logs to Check:
```bash
# In Render dashboard, check for:
"Server configuration loaded"
"Starting Go application..."
"Server started on port 3000"
```

## Benefits of DeepSeek Migration

1. **No Local Resources**: No model storage or high memory usage
2. **Free Tier Compatible**: Can deploy on Render's free tier
3. **Faster Deployment**: No model download time
4. **Better Performance**: Cloud API is typically faster
5. **Always Updated**: Latest AI capabilities without manual updates

This gives you a working Lain chatbot with modern AI capabilities at no cost!
