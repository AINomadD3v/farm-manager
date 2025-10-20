# Farm Manager Scalability - Integration Guide

## Quick Start

This guide helps you integrate the new scalability features into your build system.

## Files Created

### New Files (must be added to build):

1. **Connection Pool:**
   - `/home/phone-node/tools/farm-manager/QtScrcpy/QtScrcpyCore/src/device/deviceconnectionpool.h`
   - `/home/phone-node/tools/farm-manager/QtScrcpy/QtScrcpyCore/src/device/deviceconnectionpool.cpp`

2. **Configuration:**
   - `/home/phone-node/tools/farm-manager/QtScrcpy/ui/farmviewerconfig.h`

3. **Documentation:**
   - `/home/phone-node/tools/farm-manager/FARM_MANAGER_SCALABILITY.md`
   - `/home/phone-node/tools/farm-manager/INTEGRATION_GUIDE.md` (this file)

### Modified Files:

1. `/home/phone-node/tools/farm-manager/QtScrcpy/ui/farmviewer.h`
2. `/home/phone-node/tools/farm-manager/QtScrcpy/ui/farmviewer.cpp`

## Build System Integration

### For CMake:

Add to your `CMakeLists.txt`:

```cmake
# In your QtScrcpyCore sources
set(QTSCRCPYCORE_SOURCES
    # ... existing sources ...
    QtScrcpy/QtScrcpyCore/src/device/deviceconnectionpool.cpp
)

set(QTSCRCPYCORE_HEADERS
    # ... existing headers ...
    QtScrcpy/QtScrcpyCore/src/device/deviceconnectionpool.h
)

# In your UI sources
set(UI_HEADERS
    # ... existing headers ...
    QtScrcpy/ui/farmviewerconfig.h
)
```

### For qmake (.pro file):

Add to your `.pro` file:

```qmake
# Headers
HEADERS += \
    # ... existing headers ...
    QtScrcpy/QtScrcpyCore/src/device/deviceconnectionpool.h \
    QtScrcpy/ui/farmviewerconfig.h

# Sources
SOURCES += \
    # ... existing sources ...
    QtScrcpy/QtScrcpyCore/src/device/deviceconnectionpool.cpp
```

## Compilation

### Build the project:

```bash
# If using CMake
cd /home/phone-node/tools/farm-manager
mkdir -p build
cd build
cmake ..
make -j$(nproc)

# If using qmake
cd /home/phone-node/tools/farm-manager
qmake
make -j$(nproc)
```

## Testing

### Test with 5 devices (Ultra Quality):

```bash
# Connect 5 devices via ADB
adb devices

# Run farm manager
./farm-manager

# Expected:
# - 2x3 or 3x2 grid
# - 1080p @ 60fps
# - 300x600 tile size
# - Status: "Devices: 5 | Quality: Ultra (1080p @ 60fps)"
```

### Test with 20 devices (High Quality):

```bash
# Expected:
# - 4x5 or 5x4 grid
# - 720p @ 30fps
# - 220x450 tile size
# - Status: "Devices: 20 | Quality: High (720p @ 30fps)"
```

### Test with 50+ devices (Medium/Low Quality):

```bash
# Expected for 50:
# - 8x7 or 7x8 grid
# - 540p @ 20fps
# - 160x320 tile size

# Expected for 100:
# - 10x10 grid
# - 480p @ 15fps
# - 120x240 tile size
```

### Test window resize:

1. Start with few devices
2. Resize window - grid should recalculate
3. Add more devices - tiles should shrink
4. Remove devices - tiles should grow

## Verification Checklist

- [ ] Project compiles without errors
- [ ] No linking errors for DeviceConnectionPool
- [ ] FarmViewer shows devices in grid
- [ ] Grid adapts when window resized
- [ ] Status bar shows quality tier
- [ ] Connection pool statistics accessible
- [ ] Memory usage reasonable
- [ ] No crashes with many devices

## Common Build Issues

### Issue: "deviceconnectionpool.h: No such file or directory"

**Solution:** Ensure file path is correct in build system:
```cpp
#include "../QtScrcpyCore/src/device/deviceconnectionpool.h"
```

### Issue: Undefined reference to DeviceConnectionPool methods

**Solution:** Add `deviceconnectionpool.cpp` to SOURCES in build file.

### Issue: Multiple definition of DeviceConnectionPool::s_instance

**Solution:** Ensure .cpp file is only compiled once (not in multiple modules).

### Issue: Qt MOC errors

**Solution:** Ensure Q_OBJECT macro is present in header and run moc:
```bash
# For qmake
qmake
make clean
make

# For CMake
cmake --build . --clean-first
```

## Runtime Configuration

### Adjust for your hardware:

Edit `QtScrcpy/ui/farmviewerconfig.h`:

```cpp
// For slower machines
constexpr int MAX_CONNECTIONS = 50;  // Reduce from 200
constexpr int QUALITY_LOW_FPS = 10;  // Reduce from 15

// For powerful machines
constexpr int MAX_CONNECTIONS = 300;  // Increase from 200
constexpr int QUALITY_ULTRA_FPS = 120; // Increase from 60
```

### Enable debug logging:

```cpp
constexpr bool ENABLE_DETAILED_LOGGING = true;
constexpr bool LOG_PERFORMANCE_METRICS = true;
```

## Advanced Configuration

### Customize quality tiers:

In `deviceconnectionpool.cpp`, modify `getOptimalStreamSettings()`:

```cpp
StreamQualityProfile DeviceConnectionPool::getOptimalStreamSettings(int totalDeviceCount)
{
    // Custom tier for your use case
    if (totalDeviceCount <= 10) {
        return StreamQualityProfile(1440, 10000000, 60, "Custom Ultra");
    }
    // ... rest of tiers
}
```

### Customize grid layout:

In `farmviewer.cpp`, modify `calculateColumns()`:

```cpp
int FarmViewer::calculateColumns(int deviceCount, const QSize& windowSize) const
{
    // Custom logic for your screen size
    if (windowSize.width() > 2560) {  // 4K display
        return qMin(12, deviceCount);
    }
    // ... existing logic
}
```

## Performance Tuning

### Monitor performance:

```cpp
// Add to your code
qDebug() << "FPS:" << videoForm->getCurrentFPS();
qDebug() << "Memory:" << pool.estimateMemoryUsage() / 1024 / 1024 << "MB";
qDebug() << "Active:" << pool.getActiveConnectionCount();
```

### Profile with tools:

```bash
# CPU profiling
valgrind --tool=callgrind ./farm-manager

# Memory profiling
valgrind --tool=massif ./farm-manager

# QML profiling (if using QML)
QML_IMPORT_TRACE=1 ./farm-manager
```

## Deployment

### For production deployment:

1. **Disable debug logging:**
   ```cpp
   constexpr bool ENABLE_DETAILED_LOGGING = false;
   ```

2. **Optimize build:**
   ```bash
   cmake -DCMAKE_BUILD_TYPE=Release ..
   # or
   qmake CONFIG+=release
   ```

3. **Set appropriate limits:**
   ```cpp
   constexpr int MAX_CONNECTIONS = 150;  // Based on your hardware
   ```

4. **Test under load:**
   ```bash
   # Connect maximum expected devices
   # Monitor for 1+ hour
   # Check memory leaks
   # Verify cleanup works
   ```

## Troubleshooting

### Get connection pool status:

```cpp
auto& pool = qsc::DeviceConnectionPool::instance();
qDebug() << "Pool status:";
qDebug() << "  Total:" << pool.getTotalConnectionCount();
qDebug() << "  Active:" << pool.getActiveConnectionCount();
qDebug() << "  Idle:" << pool.getIdleConnectionCount();
qDebug() << "  Usage:" << pool.getTotalUsageCount();
```

### Force cleanup:

```cpp
// Manual cleanup of idle connections
qsc::DeviceConnectionPool::instance().cleanup();
```

### Reset pool:

```cpp
// Remove all connections
for (const QString& serial : pool.getActiveSerials()) {
    pool.removeConnection(serial);
}
```

## Support

For issues:

1. Check console logs for errors
2. Verify all files are in build system
3. Ensure Qt version compatibility (Qt 5.12+)
4. Check system resources (RAM, CPU)
5. Review FARM_MANAGER_SCALABILITY.md

## Next Steps

After successful integration:

1. Test with your actual device farm
2. Tune configuration for your hardware
3. Monitor performance under load
4. Consider additional optimizations
5. Implement pagination for 200+ devices (future)

---

**Integration Status:** Ready for testing
**Build System:** CMake or qmake
**Qt Version:** 5.12+ or Qt 6.x
**Platform:** Linux (recommended), Windows, macOS
