#!/bin/bash
# QtScrcpy Phone Farm Manager - Installation Script
# Installs the application to ~/.local for user-level deployment

set -e

echo "=================================================="
echo "QtScrcpy Phone Farm Manager - Installation Script"
echo "=================================================="
echo ""

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Detect installation directory
INSTALL_DIR="${HOME}/.local/share/qtscrcpy-farm"
BIN_DIR="${HOME}/.local/bin"
DESKTOP_DIR="${HOME}/.local/share/applications"

# Source directory (where the built binaries are)
SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/output/x64/RelWithDebInfo"

echo -e "${BLUE}Installation Configuration:${NC}"
echo "  Source: ${SOURCE_DIR}"
echo "  Install Directory: ${INSTALL_DIR}"
echo "  Binary Link: ${BIN_DIR}/qtscrcpy-farm"
echo "  Desktop Entry: ${DESKTOP_DIR}/qtscrcpy-farm.desktop"
echo ""

# Check if source directory exists
if [ ! -d "${SOURCE_DIR}" ]; then
    echo -e "${YELLOW}Error: Build directory not found at ${SOURCE_DIR}${NC}"
    echo "Please run 'cd QtScrcpy/build && make -j8' first"
    exit 1
fi

# Check if QtScrcpy binary exists
if [ ! -f "${SOURCE_DIR}/QtScrcpy" ]; then
    echo -e "${YELLOW}Error: QtScrcpy binary not found${NC}"
    echo "Please build the project first"
    exit 1
fi

echo -e "${BLUE}Step 1: Creating installation directories...${NC}"
mkdir -p "${INSTALL_DIR}"
mkdir -p "${BIN_DIR}"
mkdir -p "${DESKTOP_DIR}"
echo -e "${GREEN}✓ Directories created${NC}"
echo ""

echo -e "${BLUE}Step 2: Copying application files...${NC}"
# Copy main executable
cp "${SOURCE_DIR}/QtScrcpy" "${INSTALL_DIR}/QtScrcpy"
chmod +x "${INSTALL_DIR}/QtScrcpy"

# Copy required files
cp "${SOURCE_DIR}/scrcpy-server" "${INSTALL_DIR}/"
if [ -f "${SOURCE_DIR}/sndcpy.apk" ]; then
    cp "${SOURCE_DIR}/sndcpy.apk" "${INSTALL_DIR}/"
fi
if [ -f "${SOURCE_DIR}/sndcpy.sh" ]; then
    cp "${SOURCE_DIR}/sndcpy.sh" "${INSTALL_DIR}/"
    chmod +x "${INSTALL_DIR}/sndcpy.sh"
fi

# Create symlink to adb (NixOS-aware)
if [ -L "${SOURCE_DIR}/adb" ]; then
    # adb is a symlink, copy the link
    cp -P "${SOURCE_DIR}/adb" "${INSTALL_DIR}/"
elif [ -f "${SOURCE_DIR}/adb" ]; then
    # adb is a regular file
    cp "${SOURCE_DIR}/adb" "${INSTALL_DIR}/"
    chmod +x "${INSTALL_DIR}/adb"
else
    # Use system adb
    echo -e "${YELLOW}  Warning: adb not found in build, will use system adb${NC}"
fi

echo -e "${GREEN}✓ Application files copied${NC}"
echo ""

echo -e "${BLUE}Step 3: Creating launcher script...${NC}"
cat > "${BIN_DIR}/qtscrcpy-farm" << 'EOF'
#!/bin/bash
# QtScrcpy Phone Farm Manager Launcher
# This script sets up the environment and launches the application

# Get the installation directory
INSTALL_DIR="${HOME}/.local/share/qtscrcpy-farm"

# Change to installation directory so relative paths work
cd "${INSTALL_DIR}"

# Launch the application
exec ./QtScrcpy "$@"
EOF

chmod +x "${BIN_DIR}/qtscrcpy-farm"
echo -e "${GREEN}✓ Launcher script created at ${BIN_DIR}/qtscrcpy-farm${NC}"
echo ""

echo -e "${BLUE}Step 4: Creating desktop entry...${NC}"
cat > "${DESKTOP_DIR}/qtscrcpy-farm.desktop" << EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=QtScrcpy Phone Farm Manager
Comment=Manage and monitor multiple Android devices simultaneously
Exec=${BIN_DIR}/qtscrcpy-farm
Icon=phone
Terminal=false
Categories=Utility;System;Network;
Keywords=android;adb;scrcpy;farm;devices;
StartupNotify=true
EOF

chmod +x "${DESKTOP_DIR}/qtscrcpy-farm.desktop"
echo -e "${GREEN}✓ Desktop entry created${NC}"
echo ""

echo -e "${BLUE}Step 5: Updating desktop database...${NC}"
if command -v update-desktop-database &> /dev/null; then
    update-desktop-database "${DESKTOP_DIR}" 2>/dev/null || true
    echo -e "${GREEN}✓ Desktop database updated${NC}"
else
    echo -e "${YELLOW}  Warning: update-desktop-database not found, skipping${NC}"
fi
echo ""

echo -e "${GREEN}=================================================="
echo "Installation Complete!"
echo "==================================================${NC}"
echo ""
echo "You can now run QtScrcpy Phone Farm Manager in several ways:"
echo ""
echo "  1. From command line:"
echo -e "     ${BLUE}qtscrcpy-farm${NC}"
echo ""
echo "  2. From application launcher:"
echo "     Search for 'QtScrcpy Phone Farm Manager' in your applications menu"
echo ""
echo "  3. Add to PATH (if ~/.local/bin is not in PATH):"
echo -e "     ${BLUE}echo 'export PATH=\"\$HOME/.local/bin:\$PATH\"' >> ~/.bashrc${NC}"
echo -e "     ${BLUE}source ~/.bashrc${NC}"
echo ""
echo "Configuration files:"
echo "  Installation: ${INSTALL_DIR}"
echo "  Launcher: ${BIN_DIR}/qtscrcpy-farm"
echo "  Desktop: ${DESKTOP_DIR}/qtscrcpy-farm.desktop"
echo ""
echo -e "${YELLOW}Note: Make sure all your Android devices are connected via ADB${NC}"
echo ""
