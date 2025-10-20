#!/bin/bash
# ADB deployment script for QtScrcpy
# This script intelligently deploys either NixOS system ADB or bundled ADB

DEPLOY_PATH="$1"
BUNDLED_ADB="$2"
TARGET_ADB="$DEPLOY_PATH/adb"

# Ensure deploy directory exists
mkdir -p "$DEPLOY_PATH"

# Remove existing ADB if present
rm -f "$TARGET_ADB"

# Check if we're in a NixOS environment with QTSCRCPY_ADB_PATH set
if [ -n "$QTSCRCPY_ADB_PATH" ] && [ -f "$QTSCRCPY_ADB_PATH" ]; then
    echo "NixOS environment detected, using system ADB: $QTSCRCPY_ADB_PATH"
    ln -sf "$QTSCRCPY_ADB_PATH" "$TARGET_ADB"
    echo "Created symlink: $TARGET_ADB -> $QTSCRCPY_ADB_PATH"
else
    echo "Using bundled ADB: $BUNDLED_ADB"
    if [ -f "$BUNDLED_ADB" ]; then
        cp "$BUNDLED_ADB" "$TARGET_ADB"
        chmod +x "$TARGET_ADB"
        echo "Copied bundled ADB to: $TARGET_ADB"
    else
        echo "Warning: Bundled ADB not found at $BUNDLED_ADB"
        exit 1
    fi
fi

echo "ADB deployment completed successfully"
