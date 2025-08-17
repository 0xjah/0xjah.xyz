# 0xjah.me

Hello there!

This is my portfolio website's source code :3

## Features

- **Gallery**: Auto-grid gallery that fetches images from Cloudflare R2 bucket
- **Blog**: Personal blog with markdown support
- **Responsive Design**: Works on all devices
- **HTMX Integration**: Fast, modern web interactions

## Gallery Setup

To enable the gallery feature, you need to configure your Cloudflare R2 bucket:

1. Copy `.env.example` to `.env`
2. Fill in your R2 credentials:
   ```bash
   R2_ENDPOINT=https://<account-id>.r2.cloudflarestorage.com
   R2_ACCESS_KEY=your_r2_access_key
   R2_SECRET_KEY=your_r2_secret_key
   R2_BUCKET=your_bucket_name
   R2_PUBLIC_URL=https://your-custom-domain.com
   ```

The gallery will automatically display all image files (jpg, jpeg, png, gif, webp) from your R2 bucket in a responsive grid layout with fullscreen modal support.