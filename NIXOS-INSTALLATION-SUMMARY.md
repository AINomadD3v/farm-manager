# NixOS Installation - Complete Summary

## What Was Done

A complete NixOS-compatible installation solution was created for QtScrcpy Phone Farm Manager.

## Key Findings

### 1. Binary Analysis
- **Binary Location**: `/home/phone-node/tools/farm-manager/output/x64/RelWithDebInfo/QtScrcpy`
- **Binary Type**: ELF 64-bit LSB pie executable
- **Dynamic Linker**: `/nix/store/qhw0sp183mqd04x5jp75981kwya64npv-glibc-2.40-66/lib/ld-linux-x86-64.so.2`
- **Status**: ✅ Properly linked with NixOS libraries

The binary was built within the NixOS direnv environment and is already correctly linked with all libraries from `/nix/store`. No patching or additional library setup is required.

### 2. NixOS Environment
- **Version**: NixOS 25.11.20251002.7df7ff7 (Xantusia)
- **Home Manager**: Enabled and configured at `/etc/nixos/home.nix`
- **Shell**: Zsh (configured via home-manager)
- **Desktop**: GNOME 3 with GDM
- **ADB**: Properly configured with `programs.adb.enable = true`
- **User Groups**: `adbusers` group membership configured

### 3. Library Dependencies
All dependencies are satisfied from `/nix/store`:
- Qt6 libraries (6.9.2): Multimedia, Widgets, GUI, Network, Core
- FFmpeg libraries (7.1.1): avcodec, avformat, avutil, swscale
- Graphics libraries: OpenGL, GLX, Vulkan, EGL
- System libraries: glibc, gcc-lib, fontconfig, harfbuzz

No missing dependencies - the application is ready to run.

## Files Created

### 1. Installation Script
**File**: `install-nixos.sh`
- Bash script for quick installation to `~/.local`
- Verifies NixOS binary compatibility
- Creates launcher, desktop entry, and systemd service
- Handles ADB symlink correctly for NixOS
- Provides detailed output and verification

### 2. Home Manager Module
**File**: `home-manager-module.nix`
- Declarative Nix expression for home-manager integration
- Uses `stdenv.mkDerivation` to package the built binary
- Creates wrapper with proper environment
- Includes desktop entry via `xdg.desktopEntries`
- Includes optional systemd user service

### 3. Documentation
**Files**:
- `INSTALL-NIXOS.md` - Comprehensive installation guide (8.4KB)
- `QUICKSTART-NIXOS.md` - Quick reference guide (2.6KB)
- `NIXOS-INSTALLATION-SUMMARY.md` - This document

## Installation Methods

### Method 1: Quick Script (Recommended for Users)
```bash
chmod +x install-nixos.sh
./install-nixos.sh
```

**Installs to**:
- Application: `~/.local/share/qtscrcpy-farm/`
- Launcher: `~/.local/bin/qtscrcpy-farm`
- Desktop Entry: `~/.local/share/applications/qtscrcpy-farm.desktop`
- Systemd Service: `~/.config/systemd/user/qtscrcpy-farm.service`

**Advantages**:
- Fast and simple (2 commands)
- No configuration changes needed
- Works immediately
- Easy to test and iterate

### Method 2: Home Manager Integration (Recommended for Declarative Setup)
```nix
# Add to /etc/nixos/home.nix
imports = [ /home/phone-node/tools/farm-manager/home-manager-module.nix ];
```

```bash
# Rebuild
sudo nixos-rebuild switch --flake /home/phone-node/nix-config#nixos
```

**Advantages**:
- Fully declarative (infrastructure as code)
- Survives system rebuilds
- Integrated with NixOS configuration
- Clean uninstallation
- Reproducible across systems

### Method 3: Manual Installation
Step-by-step manual installation for full control. See `INSTALL-NIXOS.md` for details.

## Usage

### Run Application
```bash
# Method 1: Absolute path
~/.local/bin/qtscrcpy-farm

# Method 2: Command (if ~/.local/bin in PATH)
qtscrcpy-farm

# Method 3: GNOME launcher
# Press Super key → Search "QtScrcpy"
```

### Enable Auto-Start
```bash
# Enable service
systemctl --user enable qtscrcpy-farm

# Start immediately
systemctl --user start qtscrcpy-farm

# Check status
systemctl --user status qtscrcpy-farm

# View logs
journalctl --user -u qtscrcpy-farm -f
```

## Testing Results

### Binary Execution Test
```bash
ldd output/x64/RelWithDebInfo/QtScrcpy
# Result: All libraries found, no "not found" messages
```

### Launch Test
```bash
~/.local/bin/qtscrcpy-farm
# Result: Application launches successfully
# Output: Fontconfig warning (harmless) → Application GUI appears
```

### Installation Test
```bash
./install-nixos.sh
# Result: ✅ All steps completed successfully
# - Directories created
# - Files copied
# - Launcher created
# - Desktop entry created
# - Systemd service created
```

## Why This Approach Works for NixOS

### 1. Respects NixOS Philosophy
- Uses `/nix/store` libraries (binary already correctly linked)
- Doesn't try to modify system directories
- Works within user space (`~/.local`)
- Declarative option available (home-manager)

### 2. Handles NixOS Specifics
- Dynamic linker path: Already set correctly in binary
- Library paths: All in `/nix/store`, no LD_LIBRARY_PATH needed
- ADB integration: Uses NixOS android-tools package
- Systemd user services: Properly configured for NixOS

### 3. Developer-Friendly
- No need to rebuild for each change during development
- Can run directly from build directory
- Installation script for testing deployment
- Easy to iterate and modify

### 4. Production-Ready
- Systemd service for auto-start
- Desktop integration for GNOME
- Proper logging via journald
- Clean uninstallation process

## Comparison: Standard Linux vs NixOS

| Aspect | Standard Linux | NixOS Solution |
|--------|----------------|----------------|
| Libraries | `/usr/lib`, `/lib` | `/nix/store/...` (already linked) |
| Installation | Can modify system dirs | User space only (`~/.local`) |
| Dynamic Linker | `/lib64/ld-linux.so` | `/nix/store/.../ld-linux.so` |
| Package Manager | apt/dnf install | Declarative nix config |
| LD_LIBRARY_PATH | Often needed | Not needed (hardcoded paths) |
| System Integration | System packages | home-manager or ~/.local |

## Advantages of This Solution

### For Development
1. **Fast iteration**: Rebuild and test without reinstalling
2. **No system pollution**: Everything in user space
3. **Easy rollback**: Just delete ~/.local/share/qtscrcpy-farm
4. **Keep using direnv**: Development environment unchanged

### For Deployment
1. **Reproducible**: Same installation on any NixOS system
2. **Declarative option**: Home-manager integration available
3. **Systemd integration**: Auto-start and logging
4. **Desktop integration**: GNOME launcher entry

### For Users
1. **Simple**: 2 commands to install
2. **Standard paths**: Uses ~/.local (XDG standard)
3. **Familiar tools**: systemctl, journalctl
4. **Easy uninstall**: Simple rm commands or nix config change

## Files in Repository

```
/home/phone-node/tools/farm-manager/
├── install-nixos.sh              # Installation script (executable)
├── home-manager-module.nix       # Home-manager module
├── INSTALL-NIXOS.md             # Complete installation guide
├── QUICKSTART-NIXOS.md          # Quick reference
├── NIXOS-INSTALLATION-SUMMARY.md # This file
├── README.md                     # Updated with NixOS section
└── output/x64/RelWithDebInfo/
    ├── QtScrcpy                  # Main binary (NixOS-linked)
    ├── scrcpy-server            # Android server
    ├── adb -> /nix/store/.../adb # Symlink to NixOS adb
    ├── sndcpy.apk               # Audio APK
    └── sndcpy.sh                # Audio script
```

## Next Steps

### For Immediate Use
1. Run `./install-nixos.sh` to install
2. Launch with `~/.local/bin/qtscrcpy-farm`
3. Enable auto-start if desired

### For Integration
1. Add home-manager module to `/etc/nixos/home.nix`
2. Rebuild NixOS configuration
3. Application will be available after rebuild

### For Updates
After rebuilding the application:
1. **Script method**: Run `./install-nixos.sh` again (overwrites)
2. **Home-manager method**: Rebuild home-manager config

## Troubleshooting Reference

### Command Not Found
```bash
# Use absolute path
~/.local/bin/qtscrcpy-farm

# Or add to PATH (temporary)
export PATH="$HOME/.local/bin:$PATH"
```

### No Devices Found
```bash
# Check ADB
adb devices

# Restart ADB if needed
adb kill-server
adb start-server
```

### Application Won't Start
```bash
# Check dependencies
ldd ~/.local/share/qtscrcpy-farm/QtScrcpy | grep "not found"
# Should show nothing

# Run manually for errors
cd ~/.local/share/qtscrcpy-farm
./QtScrcpy
```

### Service Issues
```bash
# Reload systemd
systemctl --user daemon-reload

# Check service status
systemctl --user status qtscrcpy-farm

# View logs
journalctl --user -u qtscrcpy-farm -n 50
```

## Summary

✅ **Complete NixOS installation solution created**
✅ **Binary properly linked with NixOS libraries**
✅ **Three installation methods provided**
✅ **Comprehensive documentation written**
✅ **Installation tested and verified**
✅ **Application launches successfully**

The QtScrcpy Phone Farm Manager is now ready for deployment on NixOS with proper integration, documentation, and support for both imperative and declarative installation approaches.
