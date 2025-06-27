#!/bin/bash

# XFCE Status Bar Plugin Installation Script

set -e

echo "🔧 XFCE Status Bar Plugin Installation"
echo "======================================"

# Check if we're in the right directory
if [ ! -f "meson.build" ]; then
    echo "❌ Error: Please run this script from the project root directory"
    exit 1
fi

# Check dependencies
echo "📦 Checking dependencies..."

MISSING_DEPS=""

# Check for required packages
for pkg in libgtk-3-dev libxfce4panel-2.0-dev libxfce4ui-2-dev libcurl4-openssl-dev libudev-dev libjson-glib-dev meson ninja-build; do
    if ! dpkg -l | grep -q "^ii  $pkg "; then
        MISSING_DEPS="$MISSING_DEPS $pkg"
    fi
done

if [ -n "$MISSING_DEPS" ]; then
    echo "❌ Missing dependencies:$MISSING_DEPS"
    echo "📥 Installing missing dependencies..."
    sudo apt update
    sudo apt install -y $MISSING_DEPS
fi

echo "✅ All dependencies satisfied"

# Build the plugin
echo "🔨 Building plugin..."
if [ ! -d "build" ]; then
    meson setup build
fi

meson compile -C build

if [ $? -ne 0 ]; then
    echo "❌ Build failed!"
    exit 1
fi

echo "✅ Plugin built successfully"

# Install the plugin
echo "📥 Installing plugin..."
sudo meson install -C build

if [ $? -ne 0 ]; then
    echo "❌ Installation failed!"
    exit 1
fi

echo "✅ Plugin installed successfully"

# Restart XFCE panel
echo "🔄 Restarting XFCE panel..."
if pgrep -x "xfce4-panel" > /dev/null; then
    xfce4-panel -r &
    echo "✅ XFCE panel restarted"
else
    echo "⚠️  XFCE panel not running - please restart it manually"
fi

echo ""
echo "🎉 Installation Complete!"
echo ""
echo "To add the plugin to your panel:"
echo "1. Right-click on the XFCE panel"
echo "2. Go to 'Panel' → 'Add New Items'"
echo "3. Find 'Status Bar Plugin' and click 'Add'"
echo ""
echo "To configure the plugin:"
echo "1. Right-click on the plugin in the panel"
echo "2. Select 'Properties'"
echo "3. Configure your weather location and exchange API key"
echo ""
echo "📖 See STATUS_BAR_README.md for detailed configuration instructions"
