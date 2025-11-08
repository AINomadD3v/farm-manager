#!/usr/bin/env bash
# QtScrcpy Phone Farm Manager - NixOS Installation Script
# This script installs the application for NixOS using home-manager

set -e

echo "=========================================================="
echo "QtScrcpy Phone Farm Manager - NixOS Installation Script"
echo "=========================================================="
echo ""

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Detect installation directory
INSTALL_DIR="${HOME}/.local/share/qtscrcpy-farm"
BIN_DIR="${HOME}/.local/bin"
DESKTOP_DIR="${HOME}/.local/share/applications"
SYSTEMD_USER_DIR="${HOME}/.config/systemd/user"

# Source directory (where the built binaries are)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_DIR="${SCRIPT_DIR}/output/x64/RelWithDebInfo"

echo -e "${BLUE}NixOS Installation Configuration:${NC}"
echo "  Source: ${SOURCE_DIR}"
echo "  Install Directory: ${INSTALL_DIR}"
echo "  Binary Link: ${BIN_DIR}/qtscrcpy-farm"
echo "  Desktop Entry: ${DESKTOP_DIR}/qtscrcpy-farm.desktop"
echo "  Systemd Service: ${SYSTEMD_USER_DIR}/qtscrcpy-farm.service"
echo ""

# Check if source directory exists
if [ ! -d "${SOURCE_DIR}" ]; then
    echo -e "${RED}Error: Build directory not found at ${SOURCE_DIR}${NC}"
    echo "Please build the project first"
    exit 1
fi

# Check if QtScrcpy binary exists
if [ ! -f "${SOURCE_DIR}/QtScrcpy" ]; then
    echo -e "${RED}Error: QtScrcpy binary not found${NC}"
    echo "Please build the project first"
    exit 1
fi

# Verify binary is properly linked with NixOS libraries
echo -e "${BLUE}Verifying NixOS binary compatibility...${NC}"
if ldd "${SOURCE_DIR}/QtScrcpy" | grep -q "not found"; then
    echo -e "${RED}Error: Binary has missing dependencies:${NC}"
    ldd "${SOURCE_DIR}/QtScrcpy" | grep "not found"
    echo ""
    echo "The binary was not built properly for NixOS."
    exit 1
fi
echo -e "${GREEN}✓ Binary is properly linked with NixOS libraries${NC}"
echo ""

echo -e "${BLUE}Step 1: Creating installation directories...${NC}"
mkdir -p "${INSTALL_DIR}"
mkdir -p "${BIN_DIR}"
mkdir -p "${DESKTOP_DIR}"
mkdir -p "${SYSTEMD_USER_DIR}"
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

# Handle adb symlink (NixOS-aware)
if [ -L "${SOURCE_DIR}/adb" ]; then
    # adb is a symlink to NixOS store, copy the link
    cp -P "${SOURCE_DIR}/adb" "${INSTALL_DIR}/"
    echo -e "${GREEN}✓ Using NixOS adb from: $(readlink -f ${SOURCE_DIR}/adb)${NC}"
elif [ -f "${SOURCE_DIR}/adb" ]; then
    # adb is a regular file
    cp "${SOURCE_DIR}/adb" "${INSTALL_DIR}/"
    chmod +x "${INSTALL_DIR}/adb"
else
    # Use system adb from NixOS packages
    if command -v adb &> /dev/null; then
        ln -sf "$(command -v adb)" "${INSTALL_DIR}/adb"
        echo -e "${GREEN}✓ Using system adb: $(command -v adb)${NC}"
    else
        echo -e "${YELLOW}  Warning: adb not found, application may not work properly${NC}"
    fi
fi

echo -e "${GREEN}✓ Application files copied${NC}"
echo ""

echo -e "${BLUE}Step 3: Creating NixOS-aware launcher script...${NC}"
cat > "${BIN_DIR}/qtscrcpy-farm" << 'EOF'
#!/usr/bin/env bash
# QtScrcpy Phone Farm Manager Launcher for NixOS
# This script sets up the environment and launches the application

# Get the installation directory
INSTALL_DIR="${HOME}/.local/share/qtscrcpy-farm"

# Verify binary exists
if [ ! -f "${INSTALL_DIR}/QtScrcpy" ]; then
    echo "Error: QtScrcpy not found at ${INSTALL_DIR}/QtScrcpy"
    echo "Please run install-nixos.sh to install the application"
    exit 1
fi

# Change to installation directory so relative paths work
cd "${INSTALL_DIR}"

# On NixOS, the binary should already be linked with correct libraries
# No need to set LD_LIBRARY_PATH as everything is in /nix/store

# Force CPU-only software rendering for cross-machine compatibility
export LIBGL_ALWAYS_SOFTWARE=1
export LIBGL_DRI3_DISABLE=1
export __GLX_VENDOR_LIBRARY_NAME=mesa
export QT_OPENGL=desktop
export QT_QPA_PLATFORM=xcb
export QT_XCB_GL_INTEGRATION=xcb_glx
export MESA_GL_VERSION_OVERRIDE=3.3
export MESA_GLSL_VERSION_OVERRIDE=330

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

echo -e "${BLUE}Step 5: Creating systemd user service...${NC}"
cat > "${SYSTEMD_USER_DIR}/qtscrcpy-farm.service" << EOF
[Unit]
Description=QtScrcpy Phone Farm Manager
Documentation=https://github.com/AINomadD3v/farm-manager
After=graphical-session.target

[Service]
Type=simple
ExecStart=${BIN_DIR}/qtscrcpy-farm
Restart=on-failure
RestartSec=10

# Environment for NixOS/GNOME
Environment=DISPLAY=:0
Environment=QT_QPA_PLATFORM=xcb
Environment=XDG_RUNTIME_DIR=/run/user/%U

# Logging
StandardOutput=journal
StandardError=journal

# Security (optional hardening)
PrivateTmp=true
NoNewPrivileges=true

[Install]
WantedBy=default.target
EOF

echo -e "${GREEN}✓ Systemd service created${NC}"
echo ""

echo -e "${BLUE}Step 6: Updating desktop database...${NC}"
if command -v update-desktop-database &> /dev/null; then
    update-desktop-database "${DESKTOP_DIR}" 2>/dev/null || true
    echo -e "${GREEN}✓ Desktop database updated${NC}"
else
    echo -e "${YELLOW}  Info: update-desktop-database not found, skipping${NC}"
fi
echo ""

# Reload systemd user daemon
echo -e "${BLUE}Step 7: Reloading systemd user daemon...${NC}"
if command -v systemctl &> /dev/null; then
    systemctl --user daemon-reload 2>/dev/null || true
    echo -e "${GREEN}✓ Systemd daemon reloaded${NC}"
else
    echo -e "${YELLOW}  Info: systemctl not found in PATH${NC}"
fi
echo ""

echo -e "${GREEN}=========================================================="
echo "Installation Complete!"
echo "==========================================================${NC}"
echo ""
echo "You can now run QtScrcpy Phone Farm Manager in several ways:"
echo ""
echo "  1. From command line:"
echo -e "     ${BLUE}qtscrcpy-farm${NC}"
echo ""
echo "  2. From GNOME application launcher:"
echo "     Search for 'QtScrcpy Phone Farm Manager' in Activities"
echo ""
echo "  3. As a systemd user service (auto-start):"
echo -e "     ${BLUE}systemctl --user enable qtscrcpy-farm${NC}"
echo -e "     ${BLUE}systemctl --user start qtscrcpy-farm${NC}"
echo ""
echo "Configuration files:"
echo "  Installation: ${INSTALL_DIR}"
echo "  Launcher: ${BIN_DIR}/qtscrcpy-farm"
echo "  Desktop: ${DESKTOP_DIR}/qtscrcpy-farm.desktop"
echo "  Service: ${SYSTEMD_USER_DIR}/qtscrcpy-farm.service"
echo ""
echo -e "${BLUE}Systemd service commands:${NC}"
echo -e "  Enable auto-start:  ${BLUE}systemctl --user enable qtscrcpy-farm${NC}"
echo -e "  Start service:      ${BLUE}systemctl --user start qtscrcpy-farm${NC}"
echo -e "  Stop service:       ${BLUE}systemctl --user stop qtscrcpy-farm${NC}"
echo -e "  Service status:     ${BLUE}systemctl --user status qtscrcpy-farm${NC}"
echo -e "  View logs:          ${BLUE}journalctl --user -u qtscrcpy-farm -f${NC}"
echo ""
echo -e "${YELLOW}Note: Make sure your Android devices are connected via ADB${NC}"
echo -e "${YELLOW}Check ADB: ${BLUE}adb devices${NC}"
echo ""
