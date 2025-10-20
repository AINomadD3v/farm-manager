#!/bin/bash
# QtScrcpy Phone Farm Manager - Enable Auto-Start on Boot

set -e

echo "=================================================="
echo "QtScrcpy Phone Farm Manager - Enable Auto-Start"
echo "=================================================="
echo ""

GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

SYSTEMD_DIR="${HOME}/.config/systemd/user"
SERVICE_FILE="qtscrcpy-farm.service"

# Check if application is installed
if [ ! -f "${HOME}/.local/bin/qtscrcpy-farm" ]; then
    echo -e "${YELLOW}Error: QtScrcpy Phone Farm Manager is not installed${NC}"
    echo "Please run ./install.sh first"
    exit 1
fi

echo -e "${BLUE}Step 1: Creating systemd user directory...${NC}"
mkdir -p "${SYSTEMD_DIR}"
echo -e "${GREEN}✓ Directory created${NC}"
echo ""

echo -e "${BLUE}Step 2: Installing systemd service...${NC}"
cp "${SERVICE_FILE}" "${SYSTEMD_DIR}/"
echo -e "${GREEN}✓ Service file installed${NC}"
echo ""

echo -e "${BLUE}Step 3: Reloading systemd daemon...${NC}"
systemctl --user daemon-reload
echo -e "${GREEN}✓ Daemon reloaded${NC}"
echo ""

echo -e "${BLUE}Step 4: Enabling auto-start...${NC}"
systemctl --user enable qtscrcpy-farm.service
echo -e "${GREEN}✓ Auto-start enabled${NC}"
echo ""

echo -e "${YELLOW}Would you like to start the service now? (y/N)${NC}"
read -p "> " -n 1 -r
echo ""

if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo -e "${BLUE}Starting service...${NC}"
    systemctl --user start qtscrcpy-farm.service
    echo -e "${GREEN}✓ Service started${NC}"
    echo ""
fi

echo -e "${GREEN}=================================================="
echo "Auto-Start Configuration Complete!"
echo "==================================================${NC}"
echo ""
echo "Service status:"
systemctl --user status qtscrcpy-farm.service --no-pager || true
echo ""
echo "Useful commands:"
echo -e "  Start service:   ${BLUE}systemctl --user start qtscrcpy-farm${NC}"
echo -e "  Stop service:    ${BLUE}systemctl --user stop qtscrcpy-farm${NC}"
echo -e "  Restart service: ${BLUE}systemctl --user restart qtscrcpy-farm${NC}"
echo -e "  View logs:       ${BLUE}journalctl --user -u qtscrcpy-farm -f${NC}"
echo -e "  Disable auto-start: ${BLUE}systemctl --user disable qtscrcpy-farm${NC}"
echo ""
