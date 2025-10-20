# Farm Manager Scalability Implementation

## Overview

This document describes the production-ready scalability features implemented for managing 100+ Android devices simultaneously in the QtScrcpy Farm Manager.

## Features Implemented

### 1. Dynamic Grid Layout System

**Location:** `QtScrcpy/ui/farmviewer.cpp` and `farmviewer.h`

The grid layout automatically adapts based on:
- Number of connected devices
- Window size
- Available screen space

**Key Methods:**
- `calculateOptimalGrid(int deviceCount, QSize windowSize)` - Calculates optimal grid configuration
- `calculateColumns(int deviceCount, QSize windowSize)` - Determines column count
- `getOptimalTileSize(int deviceCount, QSize windowSize)` - Returns appropriate tile dimensions
- `resizeEvent(QResizeEvent* event)` - Handles window resize events

**Grid Scaling:**
| Device Count | Grid Layout | Tile Size |
|--------------|-------------|-----------|
| 1-4          | 2x2         | 300x600   |
| 5-9          | 3x3         | 300x600   |
| 10-20        | 4x5         | 220x450   |
| 21-50        | 6x8         | 160x320   |
| 50+          | 10x10+      | 120x240   |

### 2. Adaptive Stream Quality

**Location:** `QtScrcpy/QtScrcpyCore/src/device/deviceconnectionpool.h/cpp`

Stream quality automatically adjusts based on total device count to maintain system performance.

**Quality Tiers:**

| Tier  | Device Count | Resolution | Bitrate | FPS | Description |
|-------|--------------|------------|---------|-----|-------------|
| ULTRA | 1-5          | 1080p      | 8 Mbps  | 60  | Maximum quality |
| HIGH  | 6-20         | 720p       | 4 Mbps  | 30  | Good quality |
| MEDIUM| 21-50        | 540p       | 2 Mbps  | 20  | Balanced |
| LOW   | 50+          | 480p       | 1 Mbps  | 15  | Performance optimized |

**Key Components:**
```cpp
struct StreamQualityProfile {
    quint16 maxSize;        // Resolution (max dimension)
    quint32 bitRate;        // Bits per second
    quint32 maxFps;         // Frames per second
    QString description;    // Human-readable description
};
```

**Methods:**
- `getOptimalStreamSettings(int totalDeviceCount)` - Returns appropriate quality profile
- `getQualityTier(int deviceCount)` - Determines quality tier
- `applyQualityProfile(DeviceParams& params, StreamQualityProfile& profile)` - Applies settings

### 3. Device Connection Pool

**Location:** `QtScrcpy/QtScrcpyCore/src/device/deviceconnectionpool.h/cpp`

Thread-safe singleton connection pool for efficient device management.

**Features:**
- Connection reuse (avoids recreating connections for same device)
- Idle timeout management (5 minutes default)
- Max connection limit (200 connections default)
- LRU (Least Recently Used) eviction
- Thread-safe operations with QMutex
- Memory usage monitoring

**Key Methods:**
```cpp
// Connection management
QSharedPointer<Device> acquireConnection(const DeviceParams& params);
void releaseConnection(const QString& serial);
void removeConnection(const QString& serial);
void cleanup();

// Statistics
int getActiveConnectionCount() const;
int getTotalConnectionCount() const;
int getIdleConnectionCount() const;
```

**Configuration Constants:**
```cpp
static const int MAX_CONNECTIONS = 200;
static const int IDLE_TIMEOUT_MS = 5 * 60 * 1000;  // 5 minutes
static const int CLEANUP_INTERVAL_MS = 60 * 1000;   // 1 minute
```

### 4. Widget Size Optimization

**Dynamic Sizing:**
- Widget sizes adapt to device count
- Maintain 9:16 aspect ratio (portrait orientation)
- Font sizes scale with tile size
- Minimum/maximum size constraints prevent UI issues

**Implementation:**
```cpp
// Tile sizes scale with device count
if (deviceCount <= 5) {
    width = 300; height = 600;
} else if (deviceCount <= 20) {
    width = 220; height = 450;
} else if (deviceCount <= 50) {
    width = 160; height = 320;
} else {
    width = 120; height = 240;  // 100+ devices
}
```

### 5. Resource Management

**Memory Monitoring:**
- Estimated memory usage per connection: ~3MB
- Warning threshold: 500MB
- Automatic cleanup of idle connections

**Port Management:**
- Port range: 27183-30000
- Automatic wrap-around to prevent exhaustion
- Unique port per device connection

**Connection Limits:**
- Hard limit: 200 simultaneous connections
- LRU eviction when limit reached
- Configurable via `setMaxConnections(int max)`

## Configuration

All tunable parameters are centralized in `farmviewerconfig.h`:

```cpp
// Grid configuration
constexpr int MAX_COLS_TIER_5 = 10;  // For 50+ devices
constexpr TileSize TILE_SIZE_50_PLUS = {120, 240};

// Quality configuration
constexpr int QUALITY_LOW_RESOLUTION = 480;
constexpr int QUALITY_LOW_BITRATE = 1000000;
constexpr int QUALITY_LOW_FPS = 15;

// Connection pool
constexpr int MAX_CONNECTIONS = 200;
constexpr int IDLE_TIMEOUT_MS = 5 * 60 * 1000;

// Performance
constexpr int MAX_VISIBLE_DEVICES = 100;
```

## Usage

### Connecting Multiple Devices

The system automatically:
1. Detects all connected devices via ADB
2. Calculates optimal quality based on device count
3. Assigns unique ports
4. Creates grid layout
5. Manages connections via pool

```cpp
// Automatic detection and connection
FarmViewer::instance().showFarmViewer();
FarmViewer::instance().autoDetectAndConnectDevices();
```

### Manual Quality Adjustment

```cpp
// Get current quality profile
auto profile = FarmViewer::instance().getOptimalStreamSettings(deviceCount);

// Apply to all devices
FarmViewer::instance().applyQualityToAllDevices();
```

### Grid Recalculation

```cpp
// Manually recalculate grid
FarmViewer::instance().calculateOptimalGrid(deviceCount, windowSize);
```

## Performance Optimization Tips

### For 50-100 Devices:
- Use 540p @ 20fps (Medium tier)
- Enable hardware acceleration
- Increase system memory
- Use SSD for better I/O

### For 100+ Devices:
- Use 480p @ 15fps (Low tier)
- Disable render expired frames
- Consider multiple instances across machines
- Monitor memory usage
- Use connection pooling effectively

### Network Optimization:
- Use ADB over USB (faster than wireless)
- Ensure sufficient USB bandwidth (USB 3.0+)
- Use powered USB hubs
- Batch device connections (10 at a time)

### System Requirements:
- **CPU:** Multi-core processor (8+ cores recommended for 100+ devices)
- **RAM:** 16GB minimum, 32GB+ recommended for 100+ devices
- **Network:** USB 3.0 hubs with individual power
- **OS:** Linux (better USB handling than Windows)

## Monitoring

### Connection Pool Statistics:
```cpp
auto& pool = qsc::DeviceConnectionPool::instance();
qDebug() << "Total connections:" << pool.getTotalConnectionCount();
qDebug() << "Active connections:" << pool.getActiveConnectionCount();
qDebug() << "Idle connections:" << pool.getIdleConnectionCount();
```

### Status Bar:
The status bar displays:
- Device count
- Current quality tier
- Resolution
- FPS

Example: `Devices: 45 | Quality: Medium (540p @ 20fps)`

## Troubleshooting

### Issue: UI becomes sluggish with many devices

**Solution:**
1. Lower quality tier manually
2. Reduce tile sizes in `farmviewerconfig.h`
3. Disable render expired frames
4. Increase cleanup frequency

### Issue: Connection pool limit reached

**Solution:**
1. Increase `MAX_CONNECTIONS` in config
2. Reduce `IDLE_TIMEOUT_MS` for faster cleanup
3. Manually cleanup idle connections
4. Check for connection leaks

### Issue: Memory usage too high

**Solution:**
1. Reduce video buffer sizes
2. Lower resolution/bitrate
3. Increase cleanup frequency
4. Reduce `MAX_CONNECTIONS`

### Issue: Port exhaustion

**Solution:**
1. Increase `MAX_PORT` in config
2. Ensure port cleanup on disconnect
3. Check for stuck connections

## File Structure

```
QtScrcpy/
├── ui/
│   ├── farmviewer.h                 # Main farm viewer header
│   ├── farmviewer.cpp               # Main farm viewer implementation
│   └── farmviewerconfig.h           # Configuration constants
│
└── QtScrcpyCore/
    └── src/
        └── device/
            ├── deviceconnectionpool.h    # Connection pool header
            └── deviceconnectionpool.cpp  # Connection pool implementation
```

## Integration Notes

### Build System:
Ensure the new files are added to your build system (CMake/qmake):

```cmake
# Add to CMakeLists.txt
set(SOURCES
    # ... existing sources ...
    QtScrcpy/QtScrcpyCore/src/device/deviceconnectionpool.cpp
)

set(HEADERS
    # ... existing headers ...
    QtScrcpy/ui/farmviewerconfig.h
    QtScrcpy/QtScrcpyCore/src/device/deviceconnectionpool.h
)
```

### Dependencies:
- Qt5/Qt6 Core, Widgets, Network
- Existing QtScrcpy components (Device, IDeviceManage, etc.)
- C++11 or later (for constexpr)

## Future Enhancements

Potential improvements for even better scalability:

1. **Virtual Scrolling:** Only render visible devices
2. **Device Clustering:** Group devices by status/activity
3. **Pagination:** Display devices in pages
4. **Multi-threading:** Parallel device connection/disconnection
5. **Compression:** Use H.265/HEVC for better quality at lower bitrates
6. **Dynamic FPS:** Adjust FPS based on CPU load
7. **Cloud Integration:** Distribute load across multiple servers
8. **WebRTC Support:** Browser-based viewing for scalability

## License

Same as QtScrcpy project.

## Support

For issues related to scalability:
1. Check configuration in `farmviewerconfig.h`
2. Review debug logs
3. Monitor connection pool statistics
4. Verify system resources (CPU, RAM, USB bandwidth)

---

**Version:** 1.0
**Last Updated:** 2025-10-19
**Author:** Claude Code Implementation
