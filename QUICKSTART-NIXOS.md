# QtScrcpy Phone Farm Manager - NixOS Quick Start

## Installation (2 commands)

```bash
# 1. Make installer executable
chmod +x install-nixos.sh

# 2. Run installation
./install-nixos.sh
```

## Usage

### Run from command line

```bash
# Use absolute path (if ~/.local/bin not in PATH)
~/.local/bin/qtscrcpy-farm

# Or if ~/.local/bin is in PATH
qtscrcpy-farm
```

### Run from GNOME

1. Press `Super` key (Windows key)
2. Search for "QtScrcpy"
3. Click on "QtScrcpy Phone Farm Manager"

## Enable Auto-Start on Boot

```bash
# Enable auto-start
systemctl --user enable qtscrcpy-farm

# Start now
systemctl --user start qtscrcpy-farm

# Check status
systemctl --user status qtscrcpy-farm
```

## Useful Commands

```bash
# Start service
systemctl --user start qtscrcpy-farm

# Stop service
systemctl --user stop qtscrcpy-farm

# Restart service
systemctl --user restart qtscrcpy-farm

# View logs in real-time
journalctl --user -u qtscrcpy-farm -f

# Check ADB devices
adb devices
```

## Troubleshooting

### "Command not found"

If `qtscrcpy-farm` command is not found, use absolute path:

```bash
~/.local/bin/qtscrcpy-farm
```

Or add to PATH in new terminal:

```bash
export PATH="$HOME/.local/bin:$PATH"
```

For permanent PATH (home-manager will do this automatically on next rebuild):

```bash
# Check if already in PATH
echo $PATH | grep -q "$HOME/.local/bin" && echo "Already in PATH" || echo "Not in PATH"
```

### No devices found

```bash
# Check ADB
adb devices

# If no devices, reconnect USB or restart ADB
adb kill-server
adb start-server
adb devices
```

### Application won't start

```bash
# Test manually
cd ~/.local/share/qtscrcpy-farm
./QtScrcpy

# Check dependencies
ldd ./QtScrcpy | grep "not found"
# Should show nothing (all libraries found)
```

## Uninstallation

```bash
# Stop service
systemctl --user stop qtscrcpy-farm
systemctl --user disable qtscrcpy-farm

# Remove installation
rm -rf ~/.local/share/qtscrcpy-farm
rm ~/.local/bin/qtscrcpy-farm
rm ~/.local/share/applications/qtscrcpy-farm.desktop
rm ~/.config/systemd/user/qtscrcpy-farm.service

# Reload systemd
systemctl --user daemon-reload
```

## Next Steps

See [INSTALL-NIXOS.md](INSTALL-NIXOS.md) for:
- Home Manager integration (declarative installation)
- Advanced configuration
- Troubleshooting details
- Running in Xvfb (headless mode)

## Installation Locations

- **Application**: `~/.local/share/qtscrcpy-farm/`
- **Launcher**: `~/.local/bin/qtscrcpy-farm`
- **Desktop Entry**: `~/.local/share/applications/qtscrcpy-farm.desktop`
- **Systemd Service**: `~/.config/systemd/user/qtscrcpy-farm.service`
