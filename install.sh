#!/bin/bash
# Sigma Language Installer

set -e

echo "🚀 Installing Sigma Programming Language..."

# Detect OS
OS="$(uname -s)"
case "${OS}" in
    Linux*)     MACHINE=Linux;;
    Darwin*)    MACHINE=Mac;;
    *)          MACHINE="UNKNOWN:${OS}"
esac

if [ "$MACHINE" == "UNKNOWN:${OS}" ]; then
    echo "❌ Unsupported OS: ${OS}"
    exit 1
fi

echo "📦 Detected: $MACHINE"

# Clone repo
INSTALL_DIR="/tmp/sigma-install"
rm -rf $INSTALL_DIR
git clone https://github.com/justphemi/Sigma.git $INSTALL_DIR
cd $INSTALL_DIR

# Build
echo "🔨 Building Sigma compiler..."
chmod +x build.sh
./build.sh

echo ""
echo "✅ Sigma installed successfully!"
echo ""
echo "📝 Test it:"
echo "   echo 'yap(\"Hello Sigma!\")' > test.sgm"
echo "   sig test.sgm"
echo ""
echo "🎨 Install VS Code extension for file icons:"
echo "   Download: https://github.com/justphemi/Sigma/releases"
echo "   code --install-extension sigma-icons-0.0.1.vsix"
echo ""
echo "📚 Documentation: https://github.com/justphemi/Sigma"