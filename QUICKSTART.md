# QtScrcpy Phone Farm Manager - Quick Start Guide

Get up and running in 3 simple steps!

## Prerequisites

✅ All Android devices connected via ADB (USB or TCP/IP)
✅ QtScrcpy built successfully in `output/x64/RelWithDebInfo/`

## Installation (One-Time Setup)

### Option 1: User Installation (Recommended)

```bash
# From the farm-manager directory
./install.sh
```

This installs the application to `~/.local/` (no sudo required).

### Option 2: Run from Build Directory

```bash
cd output/x64/RelWithDebInfo
./QtScrcpy
```

## Running the Application

### After Installation

**Command Line:**
```bash
qtscrcpy-farm
```

**Application Launcher:**
Search for "QtScrcpy Phone Farm Manager" in your system's application menu.

### Auto-Start on Boot (Optional)

```bash
./enable-autostart.sh
```

The app will now start automatically when you log in.

## Basic Usage

1. **Launch the application** using one of the methods above
2. **Click on any device tile** to connect and start streaming
3. **Scroll** to view all devices in the grid
4. **Hover** over a device to highlight it (blue border)

### Managing Devices

- **Connect**: Click on a disconnected device (shows "Ready to Connect")
- **Disconnect**: Click on a streaming device to stop the stream
- **Reconnect**: If a device shows "Connection Failed", click to retry

## Quick Commands

```bash
# Check installation
ls -l ~/.local/bin/qtscrcpy-farm

# Run the app
qtscrcpy-farm

# Check device connections
adb devices

# View service status (if auto-start enabled)
systemctl --user status qtscrcpy-farm

# View logs
journalctl --user -u qtscrcpy-farm -f
```

## Common Issues

### "Command not found: qtscrcpy-farm"

Add `~/.local/bin` to your PATH:
```bash
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

### No devices showing up

Check ADB connection:
```bash
adb devices
```

Restart ADB if needed:
```bash
adb kill-server && adb start-server
```

### Performance issues

The app automatically adjusts quality based on device count:
- **1-5 devices**: 720p @ 30fps
- **6-20 devices**: 540p @ 20fps
- **21-50 devices**: 480p @ 15fps
- **51-100 devices**: 360p @ 15fps
- **100+ devices**: 270p @ 10fps

## Advanced

See [DEPLOYMENT.md](DEPLOYMENT.md) for:
- Systemd service management
- Performance tuning
- Troubleshooting
- Uninstallation
- Custom configurations

## Updates

To update to the latest version:

```bash
# Rebuild
cd QtScrcpy/build
make -j8

# Reinstall
cd ../..
./install.sh

# Restart service (if using auto-start)
systemctl --user restart qtscrcpy-farm
```

## Support

- **Documentation**: [DEPLOYMENT.md](DEPLOYMENT.md)
- **GitHub**: https://github.com/AINomadD3v/farm-manager
- **Issues**: Create an issue with logs and system info

---

**Tip**: The app handles up to 200 concurrent device streams with intelligent batching and quality adjustment!
