#!/bin/bash
# Production deployment helper script

echo "🚀 Preparing deployment to Render..."

# Check if required files exist
echo "📋 Checking deployment files..."

required_files=("Dockerfile" "render.yaml" "go.mod" "main.go")
missing_files=()

for file in "${required_files[@]}"; do
    if [ ! -f "$file" ]; then
        missing_files+=("$file")
    else
        echo "✅ $file - found"
    fi
done

if [ ${#missing_files[@]} -ne 0 ]; then
    echo "❌ Missing required files:"
    printf '   %s\n' "${missing_files[@]}"
    exit 1
fi

echo "✅ All required files present"

# Build and test locally (optional)
if command -v docker &> /dev/null; then
    echo "🐳 Docker detected - you can test locally with:"
    echo "   docker build -t deepseek-chatbot ."
    echo "   docker run -p 3000:3000 -e DEEPSEEK_API_KEY=your_key deepseek-chatbot"
    echo ""
else
    echo "ℹ️  Docker not found - skipping local test recommendation"
fi

# Git status check
if command -v git &> /dev/null && [ -d .git ]; then
    echo "📊 Git status:"
    git status --porcelain | head -10
    
    uncommitted=$(git status --porcelain | wc -l)
    if [ $uncommitted -gt 0 ]; then
        echo "⚠️  You have $uncommitted uncommitted changes"
        echo "   Make sure to commit and push before deploying:"
        echo "   git add ."
        echo "   git commit -m 'Deploy Lain chatbot'"
        echo "   git push origin main"
    else
        echo "✅ Repository is clean"
    fi
else
    echo "ℹ️  Not a git repository - make sure your code is on GitHub"
fi

echo ""
echo "🎯 Next steps:"
echo "1. Push your code to GitHub (if not done)"
echo "2. Go to https://dashboard.render.com"
echo "3. Create new Blueprint or Web Service"
echo "4. Connect your GitHub repository"
echo "5. Choose Starter plan or higher"
echo "6. Deploy!"
echo ""
echo "📖 See DEPLOYMENT.md for detailed instructions"
echo "💡 Recommended: Standard plan ($25/month) for optimal performance"
