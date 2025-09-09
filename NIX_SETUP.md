# QtScrcpy Nix Development Environment

This document describes the Nix flake-based development environment for QtScrcpy, designed specifically for phone farm management development on NixOS.

## Overview

The flake provides:
- **Complete Qt6 development environment** with CMake integration
- **Android SDK and tools** for device management
- **Cross-platform support** (Linux primarily, with macOS compatibility)
- **Production deployment options** via NixOS modules
- **Reproducible development setup** across team members

## Quick Start

### Prerequisites
- NixOS system or Nix package manager installed
- Git for version control
- `direnv` (optional but recommended for automatic environment activation)

### Getting Started

1. **Clone and enter the project**:
   ```bash
   cd /home/aidev/dev/QtScrcpy
   ```

2. **Enter the development shell**:
   ```bash
   nix develop
   ```

3. **Build the project**:
   ```bash
   mkdir -p build && cd build
   cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo
   make -j$(nproc)
   ```

4. **Alternative: Use direnv for automatic activation**:
   ```bash
   # Install direnv if not already installed
   nix-env -iA nixpkgs.direnv
   
   # Allow direnv for this project
   direnv allow
   
   # Environment will activate automatically when entering the directory
   ```

## What's Included

### Development Tools
- **Build System**: CMake 3.31.x, Ninja, pkg-config, GNU Make
- **Compilers**: GCC 13, Clang with tools
- **Debugging**: GDB, Valgrind, LLDB, Clang-tools
- **Performance**: strace, ltrace, perf tools

### Qt6 Environment
- **Core Qt6**: qtbase, qtmultimedia, qtnetworkauth
- **Development**: Qt Creator, qttools, qtwayland
- **Graphics**: qtdeclarative, qtsvg, qtimageformats
- **Platform Support**: Wayland, X11 integration

### Android Development
- **SDK Tools**: Command-line tools, platform-tools (ADB)
- **Build Tools**: Multiple versions (33.0.2, 34.0.0, 35.0.0)
- **Platforms**: Android API levels 30, 33, 34, 35
- **NDK**: Version 27.2.12479018 for native development
- **Emulator**: Android emulator with x86_64 system images

### System Libraries
- **Graphics**: Mesa, OpenGL, X11, XCB utilities, Wayland
- **Multimedia**: FFmpeg 7.x, GStreamer with plugins, ALSA, PulseAudio
- **Database**: SQLite, PostgreSQL 16, Redis (for farm management)
- **Network**: OpenSSL, curl, libusb1
- **System**: SystemD, udev, zlib

### Optional Tools
- **Containerization**: Docker, Docker Compose
- **Development**: Various debugging and profiling tools

## Environment Variables

The development shell automatically sets up:

```bash
# Android SDK
export ANDROID_HOME="/nix/store/.../share/android-sdk"
export ANDROID_SDK_ROOT="/nix/store/.../share/android-sdk"
export ANDROID_NDK_ROOT="/nix/store/.../share/android-sdk/ndk/27.2.12479018"

# Qt6 Configuration
export QT_SELECT=6
export CMAKE_PREFIX_PATH="/nix/store/.../qtbase:$CMAKE_PREFIX_PATH"
export Qt6_DIR="/nix/store/.../qtbase/lib/cmake/Qt6"

# Graphics
export LIBGL_DRIVERS_PATH="/nix/store/.../mesa/lib/dri"
```

## Building QtScrcpy

### Development Build
```bash
# Enter development environment
nix develop

# Configure with CMake
mkdir -p build && cd build
cmake .. \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DENABLE_DETAILED_LOGS=OFF \
  -DQT_DESIRED_VERSION=6

# Build
make -j$(nproc)
```

### Package Build
```bash
# Build the complete package
nix build

# Run the built package
./result/bin/QtScrcpy
```

## Migration from Traditional Build

### Before (Traditional CMake)
```bash
# Manual dependency installation
sudo apt install qt6-base-dev cmake build-essential android-tools-adb
# Or equivalent for your distribution

mkdir build && cd build
cmake ..
make
```

### After (Nix Flake)
```bash
# Automatic dependency management
nix develop

# Same build process, but with guaranteed dependencies
mkdir build && cd build
cmake ..
make
```

### Benefits
1. **Reproducible**: Same environment across all developers and CI
2. **Isolated**: No conflicts with system packages
3. **Complete**: All dependencies specified and automatically available
4. **Version-Locked**: Specific versions ensure compatibility
5. **Cross-Platform**: Works consistently on NixOS and other Linux distributions

## Production Deployment

### NixOS Module
The flake includes a NixOS module for production deployment:

```nix
# In your NixOS configuration
{
  imports = [ ./path/to/QtScrcpy/flake.nix#nixosModules.qtscrcpy ];
  
  services.qtscrcpy = {
    enable = true;
    user = "qtscrcpy";
    dataDir = "/var/lib/qtscrcpy";
  };
}
```

### Features
- **Systemd Service**: Automatic startup and management
- **User Management**: Dedicated service user with proper permissions
- **ADB Access**: Automatic configuration for Android device access
- **Security**: Isolated service with restricted permissions
- **Networking**: Firewall configuration for device communication

## Troubleshooting

### Common Issues

1. **"Path not tracked by Git"**:
   ```bash
   git add flake.nix flake.lock .envrc
   ```

2. **Qt Creator not finding Qt Kits**:
   ```bash
   rm -rf ~/.config/QtProject*
   nix develop --command qtcreator
   ```

3. **Android devices not detected**:
   ```bash
   # On NixOS, ensure your user is in adbusers group
   sudo usermod -a -G adbusers $USER
   # Log out and back in
   ```

4. **First build is slow**:
   - This is normal; Nix downloads and caches all dependencies
   - Subsequent builds will be much faster

### Getting Help

1. Check flake status: `nix flake check`
2. Validate environment: `nix develop --command env | grep -E "(QT|ANDROID|CMAKE)"`
3. Test Android SDK: `nix develop --command adb version`
4. Verify Qt: `nix develop --command qmake -version`

## Advanced Usage

### Custom Qt6 Modules
To add more Qt6 modules, edit the `qt6Packages` list in `flake.nix`:

```nix
qt6Packages = with pkgs.qt6; [
  qtbase
  qtmultimedia
  qtnetworkauth
  qttools
  qtwayland
  qtdeclarative
  qtsvg
  qtimageformats
  # Add your modules here:
  # qtwebengine
  # qtcharts
];
```

### Android SDK Customization
Modify the `androidComposition` in `flake.nix` to change SDK components:

```nix
androidComposition = androidPkgs (sdkPkgs: with sdkPkgs; [
  cmdline-tools-latest
  platform-tools
  # Add specific build-tools versions needed
  # Add specific platform versions needed
]);
```

### Development Shell Customization
Add project-specific tools to `buildDeps` in `flake.nix`:

```nix
buildDeps = with pkgs; [
  # Existing tools...
  
  # Add your tools:
  # nodejs
  # python3
  # rust
];
```

## Architecture

### Flake Structure
```
flake.nix                 # Main flake configuration
├── inputs                # External dependencies (nixpkgs, android-nixpkgs)
├── outputs
│   ├── devShells.default # Development environment
│   ├── packages.qtscrcpy # QtScrcpy package
│   └── nixosModules      # Production deployment module
.envrc                    # direnv configuration
NIX_SETUP.md             # This documentation
```

### Key Design Decisions

1. **Qt6 Focus**: Prioritizes Qt6 over Qt5 for modern development
2. **Android Integration**: Full Android SDK with multiple API levels
3. **Development-First**: Optimized for development workflow
4. **Production-Ready**: Includes deployment options
5. **Cross-Platform**: Linux-focused with macOS compatibility

## Contributing

When modifying the Nix configuration:

1. Test changes: `nix flake check`
2. Update documentation as needed
3. Commit flake.lock changes: `git add flake.lock`
4. Test on clean environment: `nix develop --refresh`

---

**Last Updated**: September 2025  
**Nix Version**: Based on nixpkgs-unstable  
**Qt Version**: 6.9.1  
**Android SDK**: Latest (API 35)  
**CMake Version**: 3.31.x  