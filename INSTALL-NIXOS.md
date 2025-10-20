# QtScrcpy Phone Farm Manager - NixOS Installation Guide

This guide provides detailed instructions for installing QtScrcpy Phone Farm Manager on NixOS.

## Prerequisites

- NixOS 25.05 or later
- Built QtScrcpy binary at `output/x64/RelWithDebInfo/QtScrcpy`
- Android devices connected via ADB
- GNOME desktop environment (or any graphical environment)

## Why NixOS Requires Special Installation

NixOS is different from traditional Linux distributions:

1. **No `/usr/lib` or `/lib`**: All libraries are in `/nix/store` with cryptographic hashes
2. **Immutable system**: Cannot directly modify system directories
3. **Declarative configuration**: Prefers declarative package management over imperative installation
4. **Dynamic linker**: Uses NixOS-specific paths for the dynamic linker

The good news: **Your QtScrcpy binary is already properly built for NixOS!** It's linked with libraries from `/nix/store`, which means it will work correctly.

## Installation Methods

Choose one of the following methods based on your preference:

### Method 1: Quick Script Installation (Recommended)

This method installs QtScrcpy to `~/.local` with proper integration.

```bash
# Make the installation script executable
chmod +x install-nixos.sh

# Run the installation
./install-nixos.sh
```

This will:
- Install the application to `~/.local/share/qtscrcpy-farm`
- Create a launcher at `~/.local/bin/qtscrcpy-farm`
- Add a desktop entry for GNOME application launcher
- Set up a systemd user service for auto-start

**Usage after installation:**
```bash
# Run from command line
qtscrcpy-farm

# Or find it in GNOME Activities (search for "QtScrcpy")
```

### Method 2: Home Manager Integration (Declarative)

This method uses Home Manager for a fully declarative installation.

**Step 1: Import the module in your home.nix**

Edit `/etc/nixos/home.nix` and add:

```nix
{
  imports = [
    /home/phone-node/tools/farm-manager/home-manager-module.nix
  ];
}
```

**Step 2: Rebuild your home configuration**

```bash
# If using system-level home-manager with flakes
sudo nixos-rebuild switch --flake /home/phone-node/nix-config#nixos

# Or if using standalone home-manager
home-manager switch
```

**Step 3: Run the application**

```bash
# From command line
qtscrcpy-farm

# Or from GNOME Activities
```

**Advantages of Home Manager method:**
- Fully declarative (configuration as code)
- Automatically integrated with your system
- Easy to enable/disable
- Survives system rebuilds
- Clean uninstallation

**Note about rebuilding:** After each build, you'll need to rebuild home-manager when the binary changes.

### Method 3: Manual Installation

If you prefer full control, follow these steps:

```bash
# 1. Create directories
mkdir -p ~/.local/share/qtscrcpy-farm
mkdir -p ~/.local/bin

# 2. Copy application files
cp output/x64/RelWithDebInfo/QtScrcpy ~/.local/share/qtscrcpy-farm/
cp output/x64/RelWithDebInfo/scrcpy-server ~/.local/share/qtscrcpy-farm/
cp output/x64/RelWithDebInfo/sndcpy.* ~/.local/share/qtscrcpy-farm/ 2>/dev/null || true

# 3. Copy or link adb
cp -P output/x64/RelWithDebInfo/adb ~/.local/share/qtscrcpy-farm/

# 4. Make executable
chmod +x ~/.local/share/qtscrcpy-farm/QtScrcpy

# 5. Create launcher script
cat > ~/.local/bin/qtscrcpy-farm << 'EOF'
#!/usr/bin/env bash
cd "${HOME}/.local/share/qtscrcpy-farm"
exec ./QtScrcpy "$@"
EOF

chmod +x ~/.local/bin/qtscrcpy-farm

# 6. Run it
qtscrcpy-farm
```

## Enable Auto-Start on Boot

### Using systemd user service

After installation with Method 1 or 2:

```bash
# Enable auto-start
systemctl --user enable qtscrcpy-farm

# Start immediately
systemctl --user start qtscrcpy-farm

# Check status
systemctl --user status qtscrcpy-farm

# View logs
journalctl --user -u qtscrcpy-farm -f
```

### Using GNOME Startup Applications

1. Open GNOME Settings → Applications → Startup Applications
2. Click "Add"
3. Enter:
   - Name: `QtScrcpy Phone Farm Manager`
   - Command: `/home/phone-node/.local/bin/qtscrcpy-farm`
   - Comment: `Android device farm manager`

## Verification

After installation, verify everything works:

```bash
# 1. Check if launcher exists and is executable
which qtscrcpy-farm
ls -lh ~/.local/bin/qtscrcpy-farm

# 2. Verify installation directory
ls -lh ~/.local/share/qtscrcpy-farm/

# 3. Check ADB connectivity
adb devices

# 4. Run the application
qtscrcpy-farm
```

## Troubleshooting

### Application doesn't start

```bash
# Check for missing dependencies
ldd ~/.local/share/qtscrcpy-farm/QtScrcpy

# Verify all libraries are found (no "not found" messages)
```

### ADB devices not detected

```bash
# Check ADB server status
adb devices

# Restart ADB server
adb kill-server
adb start-server

# Check udev rules (should be automatic on NixOS with programs.adb.enable)
groups | grep adbusers
```

### Application crashes on startup

```bash
# Run with verbose output
cd ~/.local/share/qtscrcpy-farm
./QtScrcpy --verbose

# Check systemd logs if running as service
journalctl --user -u qtscrcpy-farm -n 50
```

### Desktop entry doesn't appear

```bash
# Update desktop database
update-desktop-database ~/.local/share/applications

# Or restart GNOME Shell (Alt+F2, type 'r', Enter)
```

## Uninstallation

### Method 1 & 3 (Script/Manual installation)

```bash
# Stop service if running
systemctl --user stop qtscrcpy-farm
systemctl --user disable qtscrcpy-farm

# Remove files
rm -rf ~/.local/share/qtscrcpy-farm
rm ~/.local/bin/qtscrcpy-farm
rm ~/.local/share/applications/qtscrcpy-farm.desktop
rm ~/.config/systemd/user/qtscrcpy-farm.service

# Reload systemd
systemctl --user daemon-reload

# Update desktop database
update-desktop-database ~/.local/share/applications
```

### Method 2 (Home Manager)

Edit `/etc/nixos/home.nix` and remove the import line, then rebuild:

```bash
sudo nixos-rebuild switch --flake /home/phone-node/nix-config#nixos
```

## NixOS-Specific Notes

### Library Paths

The built binary already has the correct NixOS library paths hardcoded. You can verify this:

```bash
# Check interpreter (dynamic linker)
file output/x64/RelWithDebInfo/QtScrcpy

# Output should show: interpreter /nix/store/.../glibc-.../lib/ld-linux-x86-64.so.2

# Check linked libraries
ldd output/x64/RelWithDebInfo/QtScrcpy | head -20

# All libraries should point to /nix/store/...
```

### Why Not Use a Nix Package?

You could create a full Nix package, but since:
1. The binary is already built with NixOS libraries
2. You're actively developing and rebuilding
3. The installation is for a single user

The `~/.local` installation approach is more practical and flexible.

### PATH Configuration

NixOS home-manager automatically adds `~/.local/bin` to PATH. If it's not in your PATH:

```bash
# Check if ~/.local/bin is in PATH
echo $PATH | grep -q "$HOME/.local/bin" && echo "Yes" || echo "No"

# If not, add to home.nix:
home.sessionPath = [ "$HOME/.local/bin" ];

# Then rebuild
```

### ADB Setup

Your `/etc/nixos/configuration.nix` already has:

```nix
programs.adb.enable = true;
users.users.phone-node.extraGroups = [ "adbusers" ];
```

This ensures proper ADB permissions on NixOS.

## Integration with Existing Services

Your system already has these services running:
- `xvfb.service` - X virtual frame buffer on :99
- `office-node.service` - Office node service
- `ig-automation-backend.service` - Instagram automation backend

The QtScrcpy Farm Manager will use `:0` (your real display) by default, not conflicting with Xvfb on `:99`.

## Advanced: Running in Xvfb (Headless)

If you want to run QtScrcpy headlessly using the existing Xvfb:

```bash
# Edit the systemd service to use :99
systemctl --user edit qtscrcpy-farm

# Add:
[Service]
Environment=DISPLAY=:99
```

## Support

For issues specific to NixOS installation, check:
- Binary dependencies: `ldd ~/.local/share/qtscrcpy-farm/QtScrcpy`
- Service logs: `journalctl --user -u qtscrcpy-farm`
- ADB status: `adb devices`
- Desktop entry: `cat ~/.local/share/applications/qtscrcpy-farm.desktop`

## Next Steps

After successful installation:

1. **Configure device farm**: Connect your Android devices via USB/Network
2. **Test ADB connectivity**: Run `adb devices` to see all devices
3. **Launch application**: Run `qtscrcpy-farm` or open from GNOME
4. **Enable auto-start**: Use systemd service for automatic startup
5. **Monitor logs**: Use `journalctl --user -u qtscrcpy-farm -f` for real-time logs
