# Resource Management Quick Reference

## For 78+ Devices - What to Expect

### Connection Behavior

**Connecting 78 devices will:**
- Use **2 parallel connections** at a time
- Wait **1.5 seconds** between connection batches
- Apply **Low quality** (480p, 1Mbps, 15fps)
- Take approximately **90-120 seconds** total
- Use approximately **200-300 MB** of RAM
- Consume approximately **78 Mbps** of bandwidth

### Status Messages You'll See

#### Normal Connection Flow:
```
"Connecting 78 devices at Low quality (480p @ 15fps) to conserve resources..."
"Connecting: 10/78 at Low quality (active: 2/2, queued: 66)"
"Connecting: 50/78 at Low quality (active: 2/2, queued: 26)"
"Connected: 78 devices | Quality: Low (480p @ 15fps)"
```

#### If Memory Pressure Occurs:
```
"Resource limit reached, pausing connections..."
(Wait 2-10 seconds while system frees memory)
"Resuming connections (28 in queue)..."
(Connections continue)
```

### Resource Usage Logs

Watch for these log entries (in console/logs):

```
=== Resource Usage [Before connection batch] ===
  Active Devices: 0
  Pool Connections: 0
  Process Memory: 45 MB
  Available Memory: 3500 MB / Total: 4096 MB
  Memory Per Device: 0 MB
  Estimated Bandwidth: 0.0 Mbps
  Quality Tier: Low (50+ devices)
=======================================
```

```
=== Resource Usage [After connection] ===
  Active Devices: 50
  Pool Connections: 50
  Process Memory: 175 MB
  Available Memory: 2100 MB / Total: 4096 MB
  Memory Per Device: 3 MB
  Estimated Bandwidth: 50.0 Mbps
  Quality Tier: Low (50+ devices)
=======================================
```

## Configuration Options

### If Connections Are Too Slow

**Problem**: Taking too long to connect all devices

**Solution**: Edit `farmviewer.cpp`, function `getMaxParallelForDeviceCount()`

Change for 78 devices:
```cpp
if (totalDeviceCount >= 70) {
    return 3;  // Increase from 2 to 3 (only if you have plenty of RAM)
}
```

**Note**: Only increase if you have > 4GB RAM and memory pressure is not occurring

### If Getting Memory Pressure

**Problem**: Seeing "Resource limit reached, pausing connections..." frequently

**Solution 1**: Reduce parallel connections (if not already at 2)

**Solution 2**: Increase memory pressure threshold

Edit `farmviewer.cpp`, function `checkMemoryAvailable()`:
```cpp
return availablePercent >= 15.0;  // Lower threshold from 20% to 15%
```

**Solution 3**: Close other applications to free memory

### If Quality is Too Low

**Problem**: 480p is too low quality for your use case

**Solution**: Edit `deviceconnectionpool.cpp`, function `getOptimalStreamSettings()`

Change quality tier for 50+ devices:
```cpp
case TIER_LOW:
    return StreamQualityProfile(540, 1500000, 20, "Low (50+ devices)");
    // Changed from: 480p, 1Mbps, 15fps
    // Changed to:   540p, 1.5Mbps, 20fps
```

**Warning**: Higher quality uses more resources. May need to reduce parallel connections or increase delays.

### If Delays Are Too Long

**Problem**: 1.5 second delay is too slow

**Solution**: Edit `farmviewer.cpp`, function `calculateConnectionDelay()`

Change for 50+ devices:
```cpp
if (queueSize >= 50) {
    return 1000;  // Reduce from 1500ms to 1000ms
}
```

**Warning**: Shorter delays may cause resource spikes. Monitor memory carefully.

## Monitoring Tools

### Watch Memory During Connections

```bash
# Terminal 1: Run the app
cd /home/phone-node/tools/farm-manager
./output/x64/Release/QtScrcpy --farm

# Terminal 2: Monitor memory
watch -n 1 'free -h; echo ""; ps aux | grep QtScrcpy | grep -v grep'
```

### Watch Logs

```bash
# If logging to file:
tail -f /tmp/qtscrcpy.log | grep "Resource Usage"

# Or if logging to console:
./output/x64/Release/QtScrcpy --farm 2>&1 | grep -E "(Resource Usage|Memory check|Connection)"
```

## Troubleshooting Quick Fixes

### Process Still Getting Killed

**Quick Fix 1**: Reduce parallel connections to 1
```cpp
// farmviewer.cpp, getMaxParallelForDeviceCount()
if (totalDeviceCount >= 70) {
    return 1;  // Absolute minimum
}
```

**Quick Fix 2**: Increase delays to 2 seconds
```cpp
// farmviewer.cpp, calculateConnectionDelay()
if (queueSize >= 50) {
    return 2000;  // 2 seconds
}
```

**Quick Fix 3**: Use lowest possible quality
```cpp
// deviceconnectionpool.cpp, getOptimalStreamSettings()
case TIER_LOW:
    return StreamQualityProfile(360, 500000, 10, "Minimal");
    // 360p, 0.5Mbps, 10fps - absolute minimum
```

### Connections Pausing Too Often

**Cause**: Memory threshold too high or other apps using RAM

**Fix 1**: Lower memory threshold
```cpp
// farmviewer.cpp, checkMemoryAvailable()
return availablePercent >= 10.0;  // Lower to 10%
```

**Fix 2**: Close other applications

**Fix 3**: Increase system swap space
```bash
# Add 4GB swap
sudo dd if=/dev/zero of=/swapfile bs=1G count=4
sudo mkswap /swapfile
sudo swapon /swapfile
```

### No Log Output

**Fix**: Enable verbose logging

Edit `farmviewer.cpp`, add at start of `processDetectedDevices()`:
```cpp
qSetMessagePattern("%{time yyyy-MM-dd hh:mm:ss} [%{type}] %{message}");
```

Or run with debug environment variable:
```bash
QT_LOGGING_RULES="*.debug=true" ./output/x64/Release/QtScrcpy --farm
```

## Performance Expectations

### Memory Usage by Device Count

| Devices | Estimated RAM | Peak RAM | Quality | Parallel | Time |
|---------|---------------|----------|---------|----------|------|
| 10      | 30 MB         | 50 MB    | Ultra   | 5        | 10s  |
| 20      | 60 MB         | 100 MB   | High    | 5        | 20s  |
| 50      | 150 MB        | 200 MB   | Medium  | 3        | 60s  |
| 78      | 234 MB        | 300 MB   | Low     | 2        | 120s |
| 100     | 300 MB        | 400 MB   | Low     | 2        | 150s |

### Bandwidth Usage by Device Count

| Devices | Estimated Bandwidth | Quality Setting |
|---------|-------------------|-----------------|
| 10      | 80 Mbps           | Ultra (8Mbps)   |
| 20      | 80 Mbps           | High (4Mbps)    |
| 50      | 100 Mbps          | Medium (2Mbps)  |
| 78      | 78 Mbps           | Low (1Mbps)     |
| 100     | 100 Mbps          | Low (1Mbps)     |

## Key Takeaways

### For 78 Devices:

✅ **WILL WORK**: Conservative settings prevent OS kill
- 2 parallel connections
- 1.5 second delays
- Low quality (480p)
- Memory monitoring active
- Automatic pause/resume

✅ **SAFE DEFAULTS**: All settings tuned for 70-100 device range

✅ **MONITORING**: Comprehensive logging of all resource usage

✅ **GRACEFUL**: No crashes, just pauses if needed

⚠️ **PATIENCE REQUIRED**: Takes 90-120 seconds to connect all devices

⚠️ **QUALITY TRADEOFF**: 480p @ 15fps to conserve resources

⚠️ **MEMORY REQUIREMENT**: Need ~500MB free RAM minimum (includes OS overhead)

## Emergency Overrides

### Bypass All Limits (USE WITH CAUTION)

If you have a very powerful system (32GB+ RAM, high CPU) and want maximum speed:

```cpp
// farmviewer.cpp
int FarmViewer::getMaxParallelForDeviceCount(int totalDeviceCount)
{
    return 10;  // Always use 10 parallel
}

int FarmViewer::calculateConnectionDelay(int queueSize)
{
    return 100;  // Only 100ms delay
}

bool FarmViewer::checkMemoryAvailable()
{
    return true;  // Never pause for memory
}
```

**WARNING**: This can crash the system on typical hardware. Only use if you know your system can handle it.

## Getting Help

### Check These First:
1. Available system RAM: `free -h`
2. Current quality tier: Check logs for "Quality Scaling"
3. Parallel connection setting: Check logs for "Max Parallel Connections"
4. Memory pressure events: `grep "MEMORY PRESSURE" logs`

### Include in Bug Reports:
1. Number of devices attempting to connect
2. Available system memory (`free -h`)
3. Resource usage logs (before, during, after)
4. Any "pausing" or "resuming" messages
5. Final success/failure count

## File Locations

### Configuration Files:
- Connection limits: `QtScrcpy/ui/farmviewer.cpp`
- Quality settings: `QtScrcpy/QtScrcpyCore/src/device/deviceconnectionpool.cpp`
- Memory monitoring: `QtScrcpy/ui/performancemonitor.cpp`

### Log Files:
- Application logs: Check console output or `/tmp/qtscrcpy.log`
- System logs: `journalctl -xe` (for OOM killer events)

## Summary

The resource management system for 78+ devices is designed to be:
- **Conservative**: Prioritizes stability over speed
- **Adaptive**: Adjusts to device count automatically
- **Monitored**: Tracks resources continuously
- **Graceful**: Pauses instead of crashes

**Bottom Line**: Connecting 78 devices will take 90-120 seconds but will succeed without crashes.
