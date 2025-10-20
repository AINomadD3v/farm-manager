# Testing Guide - Sequential Connection Fix

## What to Look For in Logs

### 1. Memory Parsing Fix (Should See)
```
PerformanceMonitor: MemTotal = 16455634944 bytes (16102672 KB)
PerformanceMonitor: MemAvailable = 12014477312 bytes (11732888 KB)
PerformanceMonitor: Final - Total: 15693 MB Available: 11454 MB Percent: 72.9%
```

**NOT** the broken output:
```
FarmViewer: Memory check - Available: 0 MB ( "0.0" %)
```

### 2. ADB Callback Trigger (Should See)
```
FarmViewer: ADB CALLBACK TRIGGERED! Result: 0
FarmViewer: ADB success, checking arguments...
FarmViewer: ADB arguments: ("devices")
FarmViewer: Getting device list from ADB output...
FarmViewer: Found 78 devices: ("serial1", "serial2", ...)
FarmViewer: Calling processDetectedDevices()...
```

### 3. Device Queue Processing (Should See)
```
========================================
FarmViewer: Processing detected devices: ("serial1", "serial2", ...)
Device count: 78
========================================
```

```
========================================
FarmViewer: Adding 78 devices to connection queue
New devices: ("serial1", "serial2", ...)
========================================
FarmViewer: Calling processConnectionQueue()...
```

### 4. Sequential Connection (Should See)
```
FarmViewer: Processing connection queue - Queue size: 78 Active: 0 / 1
FarmViewer: Starting connection to serial1 (Active: 1 / 1 Queue: 77)
```

**Wait for device 1 to complete...**

```
FarmViewer: Connection successful for serial1
FarmViewer: Connection complete (Active: 0 Queue: 77)
FarmViewer: Scheduling next connection in 500 ms
```

**500ms later...**

```
FarmViewer: Connection throttle timer fired, processing next device...
FarmViewer: Processing connection queue - Queue size: 77 Active: 0 / 1
FarmViewer: Starting connection to serial2 (Active: 1 / 1 Queue: 76)
```

### 5. Resource Monitoring Disabled (Should See)
```
FarmViewer: Resource monitoring DISABLED (sequential mode)
```

**Should NOT see:**
```
FarmViewer: Memory check - Available: 0 MB
FarmViewer: MEMORY PRESSURE DETECTED
FarmViewer: Resource limit reached, pausing connections...
```

### 6. Simplified Resource Logging (Should See)
```
=== Resource Usage [After connection] ===
  Active Devices: 5
  Pool Connections: 5
  Quality Tier: Default
=======================================
```

**Should NOT see:**
```
Available Memory: 0 MB / Total: 0 MB
```

## Expected Connection Behavior

### Sequential Connection Pattern
1. Connect device 1 → **Active: 1/1, Queue: 77**
2. Wait for device 1 to complete
3. Wait 500ms throttle delay
4. Connect device 2 → **Active: 1/1, Queue: 76**
5. Wait for device 2 to complete
6. Wait 500ms throttle delay
7. ...continue until all 78 devices connected

### Key Characteristics
- **Max Active Connections**: Always 1
- **Delay Between Connections**: Fixed 500ms
- **No Memory Checks**: Resource monitoring disabled
- **No Parallel Connections**: Simple sequential queue

## How to Run the Test

```bash
cd /home/phone-node/tools/farm-manager/output/x64/Release
./QtScrcpy --farm-mode 2>&1 | tee farm-test.log
```

Then search the log for the patterns above.

## Success Criteria

✓ Memory parsing shows > 0% available
✓ ADB callback is triggered and logs device list
✓ processDetectedDevices() is called
✓ Devices are added to connection queue
✓ Connections proceed sequentially (Active: 1/1)
✓ No memory pressure warnings
✓ No "0 MB / 0%" in logs

## Failure Indicators

✗ Logs show "Available: 0 MB (0.0%)"
✗ ADB callback never triggered
✗ No "Processing detected devices" log
✗ Multiple active connections (Active: 2+ / 1)
✗ Memory pressure pausing connections
✗ Devices not being queued

## Debug Commands

### Check if binary was rebuilt
```bash
ls -lh /home/phone-node/tools/farm-manager/output/x64/Release/QtScrcpy
stat /home/phone-node/tools/farm-manager/output/x64/Release/QtScrcpy
```

### Verify memory info is readable
```bash
cat /proc/meminfo | head -5
```

### Check ADB devices
```bash
adb devices | wc -l
```

### Monitor in real-time
```bash
./QtScrcpy --farm-mode 2>&1 | grep -E "(CALLBACK|Processing detected|Adding.*queue|Active:|Memory check)"
```

## Troubleshooting

### If ADB callback still not triggered
- Check signal/slot connection in constructor (lines 73-95)
- Verify AdbProcess is completing successfully
- Check that `isRuning()` returns false before execute

### If memory still shows 0%
- Verify the regex fix was compiled in
- Check binary timestamp matches build time
- Manually test /proc/meminfo parsing

### If connections are parallel (not sequential)
- Verify m_maxParallelConnections = 1 (line 34)
- Check processConnectionQueue() limits (line 1051)
- Verify getMaxParallelForDeviceCount() returns 1 (line 1039)

## Files Modified
1. `/home/phone-node/tools/farm-manager/QtScrcpy/ui/performancemonitor.cpp`
2. `/home/phone-node/tools/farm-manager/QtScrcpy/ui/farmviewer.cpp`

## Build Verification
```bash
cd /home/phone-node/tools/farm-manager/QtScrcpy/build
cmake --build . -j8
echo "Build exit code: $?"
```

Exit code should be 0 for success.
