#!/bin/bash
# QtScrcpy Phone Farm Manager - Uninstallation Script

set -e

echo "=================================================="
echo "QtScrcpy Phone Farm Manager - Uninstall"
echo "=================================================="
echo ""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

INSTALL_DIR="${HOME}/.local/share/qtscrcpy-farm"
BIN_DIR="${HOME}/.local/bin"
DESKTOP_DIR="${HOME}/.local/share/applications"
SYSTEMD_DIR="${HOME}/.config/systemd/user"

echo -e "${YELLOW}This will remove QtScrcpy Phone Farm Manager from your system.${NC}"
echo ""
read -p "Are you sure you want to continue? (y/N) " -n 1 -r
echo ""

if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Uninstallation cancelled."
    exit 0
fi

echo ""
echo -e "${RED}Removing installation...${NC}"

# Stop and disable systemd service if it exists
if [ -f "${SYSTEMD_DIR}/qtscrcpy-farm.service" ]; then
    echo "Stopping systemd service..."
    systemctl --user stop qtscrcpy-farm.service 2>/dev/null || true
    systemctl --user disable qtscrcpy-farm.service 2>/dev/null || true
    rm -f "${SYSTEMD_DIR}/qtscrcpy-farm.service"
    systemctl --user daemon-reload
    echo -e "${GREEN}✓ Systemd service removed${NC}"
fi

# Remove application files
if [ -d "${INSTALL_DIR}" ]; then
    rm -rf "${INSTALL_DIR}"
    echo -e "${GREEN}✓ Application files removed${NC}"
fi

# Remove launcher script
if [ -f "${BIN_DIR}/qtscrcpy-farm" ]; then
    rm -f "${BIN_DIR}/qtscrcpy-farm"
    echo -e "${GREEN}✓ Launcher script removed${NC}"
fi

# Remove desktop entry
if [ -f "${DESKTOP_DIR}/qtscrcpy-farm.desktop" ]; then
    rm -f "${DESKTOP_DIR}/qtscrcpy-farm.desktop"
    echo -e "${GREEN}✓ Desktop entry removed${NC}"
fi

# Update desktop database
if command -v update-desktop-database &> /dev/null; then
    update-desktop-database "${DESKTOP_DIR}" 2>/dev/null || true
fi

echo ""
echo -e "${GREEN}Uninstallation complete!${NC}"
echo ""
