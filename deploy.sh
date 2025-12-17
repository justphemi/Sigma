#!/bin/bash

echo "🚀 Deploying Sigma to GitHub..."

# Initialize git if not already
if [ ! -d .git ]; then
    git init
fi

# Copy icon to root for README
cp sigma-icons/icons/sigma.svg sigma-icon.svg

# Create .gitignore
cat > .gitignore << 'EOF'
build/
*.o
*.out
/tmp/
.DS_Store
*.vsix
node_modules/
EOF

# Stage all files
git add .

# Commit
git commit -m "Initial release of Sigma v0.1.0

Features:
- Dynamic typing (int, dec, str, bool)
- Functions with parameters
- Conditionals ($if, $el)
- Loops ($for, $while)
- Arrays and Objects
- String concatenation
- Compiles to optimized C code
- VS Code extension with file icons"

echo ""
echo "✅ Files committed!"
echo ""
echo "Next steps:"
echo "1. Create repo on GitHub: https://github.com/new"
echo "2. Run these commands:"
echo ""
echo "   git remote add origin https://github.com/justphemi/Sigma.git"
echo "   git branch -M main"
echo "   git push -u origin main"
echo ""
echo "3. Upload sigma-icons-0.0.1.vsix to GitHub Releases"
echo "4. Update README.md with your actual GitHub username"