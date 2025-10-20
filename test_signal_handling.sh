#!/bin/bash
# Test script for Unix signal handling in Phone Farm Manager
# This script tests that Ctrl+C and SIGTERM properly cleanup all device connections

set -e

echo "=========================================="
echo "Phone Farm Manager - Signal Handler Test"
echo "=========================================="
echo ""

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${YELLOW}Test 1: Building the application...${NC}"
cd /home/phone-node/tools/farm-manager
nix develop -c bash -c "qmake && make -j$(nproc)"

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Build successful${NC}"
else
    echo -e "${RED}✗ Build failed${NC}"
    exit 1
fi

echo ""
echo -e "${YELLOW}Test 2: Starting application in background...${NC}"
# Start the application in background
nix develop -c ./QtScrcpy/QtScrcpy &
APP_PID=$!
echo -e "Application started with PID: ${APP_PID}"

# Wait for application to initialize
echo "Waiting 5 seconds for application to initialize..."
sleep 5

# Check if process is still running
if ps -p $APP_PID > /dev/null; then
    echo -e "${GREEN}✓ Application is running${NC}"
else
    echo -e "${RED}✗ Application crashed during startup${NC}"
    exit 1
fi

echo ""
echo -e "${YELLOW}Test 3: Testing SIGINT (Ctrl+C) handling...${NC}"
echo "Sending SIGINT to PID $APP_PID..."
kill -SIGINT $APP_PID

# Wait for graceful shutdown (max 10 seconds)
for i in {1..10}; do
    if ! ps -p $APP_PID > /dev/null 2>&1; then
        echo -e "${GREEN}✓ Application shutdown gracefully after ${i} seconds${NC}"
        break
    fi
    echo "Waiting for shutdown... (${i}/10)"
    sleep 1
done

# Check if process is still running
if ps -p $APP_PID > /dev/null 2>&1; then
    echo -e "${RED}✗ Application did not shutdown after SIGINT${NC}"
    echo "Force killing..."
    kill -9 $APP_PID
    exit 1
fi

echo ""
echo -e "${YELLOW}Test 4: Testing SIGTERM handling...${NC}"
# Start again
nix develop -c ./QtScrcpy/QtScrcpy &
APP_PID=$!
echo "Application started with PID: $APP_PID"
sleep 5

echo "Sending SIGTERM to PID $APP_PID..."
kill -SIGTERM $APP_PID

# Wait for graceful shutdown
for i in {1..10}; do
    if ! ps -p $APP_PID > /dev/null 2>&1; then
        echo -e "${GREEN}✓ Application shutdown gracefully after ${i} seconds${NC}"
        break
    fi
    echo "Waiting for shutdown... (${i}/10)"
    sleep 1
done

if ps -p $APP_PID > /dev/null 2>&1; then
    echo -e "${RED}✗ Application did not shutdown after SIGTERM${NC}"
    kill -9 $APP_PID
    exit 1
fi

echo ""
echo -e "${GREEN}=========================================="
echo "All signal handling tests passed!"
echo "==========================================${NC}"
echo ""
echo "Manual testing instructions:"
echo "1. Start the application normally"
echo "2. Connect some devices via Farm Viewer"
echo "3. Press Ctrl+C in the terminal"
echo "4. Verify in logs that:"
echo "   - Signal was received"
echo "   - All devices were disconnected"
echo "   - Cleanup sequence completed"
echo "   - No zombie processes remain"
