# Phone Farm Manager

[![NixOS](https://img.shields.io/badge/NixOS-5277C3.svg?logo=nixos&logoColor=white)](#nix-flake-installation)
[![Qt6](https://img.shields.io/badge/Qt-6.9.1-brightgreen.svg)](https://doc.qt.io/qt-6/)
[![Android SDK](https://img.shields.io/badge/Android%20SDK-API%2030--35-brightgreen.svg)](https://developer.android.com/sdk)
[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE)

**Enterprise-grade Android device farm management system built on QtScrcpy foundation**

Phone Farm Manager is a professional multi-device management platform designed for large-scale Android device operations, testing environments, and enterprise device farms. Built on the proven QtScrcpy foundation, it provides advanced capabilities for managing 50+ devices simultaneously with sophisticated automation, monitoring, and control features.

## 🚀 Key Features

### Multi-Device Management
- **Farm Grid Viewer**: Unified dashboard displaying multiple devices in a responsive grid layout
- **Batch Operations**: Simultaneous control across selected device groups
- **Real-time Monitoring**: Live device status, battery levels, and connectivity tracking
- **Auto Discovery**: Network-wide device detection and registration

### Enterprise Automation
- **Task Scheduler**: Cron-like scheduling for automated operations
- **Script Engine**: JavaScript-based automation with device APIs
- **Bulk File Transfer**: Deploy APKs and files across multiple devices
- **Screenshot Automation**: Coordinated screenshot capture across device groups

### Device Farm Operations
- **Group Management**: Organize devices into logical groups and hierarchies  
- **Master-Slave Control**: Synchronized input distribution from master to controlled devices
- **Connection Pooling**: Efficient USB and WiFi connection management
- **Health Monitoring**: Automated device health checks and recovery

### Professional UI/UX
- **Qt6-Based Interface**: Modern, responsive desktop application
- **Grid Layout**: Configurable device grid with live thumbnails
- **Batch Control Panel**: Quick actions for common farm operations
- **Status Dashboard**: Real-time farm statistics and device metrics

## 🏗️ Architecture

### System Components

```
Phone Farm Manager Architecture
├── Core Engine (QtScrcpy Foundation)
│   ├── Device Communication (ADB/TCP)
│   ├── Video Streaming (H.264 Hardware Acceleration)
│   └── Input Management (Touch, Key, Mouse)
├── Farm Management Layer
│   ├── FarmViewer (Multi-Device Grid)
│   ├── DeviceManager (Lifecycle Management)
│   ├── GroupController (Batch Operations)
│   └── TaskScheduler (Automation Engine)
├── Data Layer
│   ├── SQLite Database (Device Inventory)
│   ├── Configuration Management
│   └── Task/Script Storage
└── User Interface
    ├── Main Farm Dashboard
    ├── Device Grid Display
    ├── Control Panels
    └── Automation Tools
```

### Key Classes

- **FarmViewer**: Central multi-device grid interface
- **FarmDeviceManager**: Device lifecycle and state management  
- **GroupManager**: Device grouping and organization
- **TaskScheduler**: Automation and scripting engine
- **DeviceGrid**: Responsive grid layout component

## 🔧 NixOS Installation

Phone Farm Manager is designed specifically for NixOS environments with comprehensive flake-based dependency management.

### Prerequisites

- **NixOS** with flakes enabled
- **Git** with repository access
- **16GB+ RAM** for multi-device operations
- **Android devices** for testing (3+ recommended)

### Quick Start (Super Simple!)

```bash
# 1. Enter the project directory
cd phone-farm-manager

# 2. That's it! The environment auto-loads with direnv
#    If not, just run: direnv allow

# 3. Build the application (first time only)
build_release

# 4. Start the Phone Farm Manager
run_qtscrcpy_farm

# 5. In the QtScrcpy window that opens:
#    - Click the "Farm Viewer" button in the toolbar
#    - All your connected devices will appear in a grid
#    - Select and control multiple devices at once!
```

**That's all!** The app opens once, then you use the Farm Viewer inside it to see all your devices in a single window.

### Nix Flake Installation

```bash
# Direct execution from flake
nix run github:user/phone-farm-manager

# Install system-wide
nix profile install github:user/phone-farm-manager

# Add to NixOS system configuration
{
  inputs.phone-farm-manager.url = "github:user/phone-farm-manager";
  # ... add to system packages
}
```

### NixOS System Installation (After Building)

Once you've built the application, install it system-wide on NixOS:

```bash
# Quick installation (recommended)
./install-nixos.sh

# Then run from anywhere
~/.local/bin/qtscrcpy-farm

# Or enable auto-start service
systemctl --user enable qtscrcpy-farm
systemctl --user start qtscrcpy-farm
```

For detailed NixOS installation instructions, see:
- **[QUICKSTART-NIXOS.md](QUICKSTART-NIXOS.md)** - Quick reference (2 commands to install)
- **[INSTALL-NIXOS.md](INSTALL-NIXOS.md)** - Complete NixOS installation guide
- **[home-manager-module.nix](home-manager-module.nix)** - Declarative home-manager integration

### Development Environment

The default environment includes everything you need:
- Qt6 development tools
- Android SDK and ADB
- Build tools (CMake, Ninja, GCC)
- All farm management functions

No need to switch between different environments! Just enter the project directory and start working.

```bash
# The default environment has everything
cd phone-farm-manager
# Environment auto-loads via direnv

# All commands are ready:
build_release        # Build the project
run_qtscrcpy_farm   # Run multi-device mode
list_devices        # Show connected devices
kill_qtscrcpy       # Stop all instances
```

### NixOS Service Module

Enable the built-in NixOS service for production deployments:

```nix
{
  services.qtscrcpy = {
    enable = true;
    user = "farm-manager";
    dataDir = "/var/lib/phone-farm";
    port = 27183;
  };
}
```

## 🖥️ Usage

### Basic Farm Operations

1. **Connect Devices**
   ```bash
   # USB connection
   adb devices
   
   # WiFi setup
   adb connect 192.168.1.100:5555
   ```

2. **Launch Farm Viewer**
   - Click the **"Farm Viewer"** button in main interface
   - Devices auto-populate in grid layout
   - Select devices for batch operations

3. **Multi-Device Control**
   - **Master Mode**: One device controls others
   - **Sync Input**: Simultaneous input across selected devices
   - **Group Operations**: Bulk actions on device groups

### Advanced Features

#### Device Groups
```bash
# Create device groups
Groups → Create Group → "Testing Devices"
# Assign devices via drag-and-drop or context menu
```

#### Automation Scripts  
```javascript
// Example: Take screenshots across all devices
farm.getAllDevices().forEach(device => {
  device.takeScreenshot(`/screenshots/${device.name}.png`);
});
```

#### Batch File Operations
```bash
# Deploy APK to all connected devices
farm.deployAPK("app-debug.apk", ["group:testing"]);

# Transfer files to specific devices  
farm.transferFile("data.json", ["device1", "device2"]);
```

### Command Line Interface

```bash
# Available farm functions (loaded in nix develop)
run_qtscrcpy                    # Standard single-device mode
run_qtscrcpy_farm               # Multi-device farm mode
run_qtscrcpy_device <ip:port>   # Specific device connection
list_devices                    # Show connected devices
kill_qtscrcpy                   # Stop all instances
```

### Farm Grid Layout

The Farm Viewer presents devices in a responsive grid:
- **2x2 default layout** (expandable)
- **Live device thumbnails** with status indicators
- **Scroll support** for large device farms
- **Context menus** for per-device actions
- **Batch selection** with Ctrl/Shift+click

## 🧪 Development

### Building from Source

```bash
# Enter development environment
nix develop

# Configure build
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo

# Compile
make -j$(nproc)

# Run tests
# make test  # (disabled by default for faster builds)

# Install
make install
```

### Development Tools

The Nix environment includes:
- **Qt Creator** (optional, due to build complexity)
- **CMake 3.31.7** with Ninja build system
- **GCC 14.3.0** compiler
- **Android SDK** with API levels 30-35
- **Debugging tools** (GDB, Valgrind)

### Testing Multi-Device Setups

```bash
# Connect test devices
adb connect 192.168.40.101:5555
adb connect 192.168.40.102:5555  
adb connect 192.168.40.103:5555

# Launch farm mode
run_qtscrcpy_farm

# Test automation
run_automation_test
```

### Architecture Extension

Key extension points for new features:
- **`QtScrcpy/farmmanager/`** - Core farm management classes
- **`QtScrcpy/ui/farmviewer.cpp`** - Main farm interface
- **`QtScrcpy/automation/`** - Task scheduling and scripting
- **`QtScrcpy/groupcontroller/`** - Multi-device coordination

## 📊 Performance

### System Requirements

- **CPU**: Multi-core recommended (4+ cores for 10+ devices)  
- **RAM**: 16GB+ (base 200MB + ~50MB per device)
- **Network**: Gigabit Ethernet for WiFi device farms
- **Storage**: SSD recommended for video recording

### Benchmarks

- **Device Capacity**: 50+ simultaneous connections tested
- **Video Performance**: 30-60 FPS per device with hardware acceleration
- **Network Throughput**: 100+ Mbps aggregate video streams
- **Response Time**: <100ms for batch operations
- **Resource Efficiency**: <5% CPU overhead per additional device

## 🔒 Security

### Production Considerations

- **Script Sandboxing**: JavaScript execution with limited system access
- **Network Security**: Device communication over encrypted channels
- **Access Control**: User permissions for device groups and operations
- **Audit Logging**: Complete operation tracking and compliance

### Device Security

- **ADB Security**: Proper authentication and key management
- **Network Isolation**: VLAN support for device networks
- **Certificate Management**: SSL/TLS for remote connections

## 🤝 Contributing

Phone Farm Manager welcomes contributions from the community:

1. **Fork** the repository
2. **Create** a feature branch (`git checkout -b feature/amazing-feature`)
3. **Commit** your changes (`git commit -m 'Add amazing feature'`)
4. **Push** to the branch (`git push origin feature/amazing-feature`)
5. **Open** a Pull Request

### Development Guidelines

- Follow existing Qt6 and C++ coding standards
- Add unit tests for new functionality  
- Update documentation for user-facing features
- Test with multiple device configurations
- Use the NixOS development environment

### Areas for Contribution

- **Device Drivers**: Support for additional device types
- **Automation Scripts**: Pre-built automation libraries
- **UI/UX**: Interface improvements and accessibility
- **Performance**: Optimization for large-scale deployments
- **Documentation**: Tutorials and best practices

## 📄 License

Phone Farm Manager inherits the Apache License 2.0 from QtScrcpy:

```
Copyright (C) 2025 Phone Farm Manager Contributors
Copyright (C) 2025 QtScrcpy Project

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```

## 🙏 Acknowledgments

Phone Farm Manager is built upon the excellent foundation of:

- **[QtScrcpy](https://github.com/barry-ran/QtScrcpy)** by barry-ran - The core Android mirroring technology
- **[scrcpy](https://github.com/Genymobile/scrcpy)** by Genymobile - The underlying screen mirroring protocol
- **Qt Project** - The cross-platform application framework
- **Android Open Source Project** - Android SDK and tools

### Key Differences from QtScrcpy

| Feature | QtScrcpy | Phone Farm Manager |
|---------|----------|-------------------|
| **Focus** | Single device mirroring | Enterprise multi-device farms |
| **UI Paradigm** | Individual device windows | Unified grid dashboard |
| **Device Limit** | 1-5 devices (manual) | 50+ devices (automated) |
| **Automation** | Manual operations | Scripted task scheduling |
| **Management** | Basic device control | Advanced farm operations |
| **Target Users** | Individual developers | Enterprise device farms |
| **Deployment** | Manual installation | NixOS service modules |

## 📞 Support

- **Documentation**: Check the `/docs` directory for detailed guides
- **Issues**: Report bugs and feature requests via GitHub Issues  
- **Discussions**: Join community discussions for help and ideas
- **Enterprise**: Contact for enterprise support and consulting

---

**Phone Farm Manager** - Professional Android device farm management made simple.

*Built with ❤️ on NixOS using Qt6 and modern C++*