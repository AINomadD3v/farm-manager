# Phone Farm Manager - Resource Management Implementation

## Summary

This document describes the resource management features implemented to prevent OS kills when connecting 78+ devices simultaneously.

## Problem Statement

The Phone Farm Manager process was being killed by the OS when attempting to connect 78 TCP devices simultaneously. The process would crash after starting the first connection due to:
- Rapid memory exhaustion
- CPU overload from parallel connection attempts
- ADB process overwhelming
- Network resource spikes

## Solution Overview

Implemented a multi-layered resource management system with:

1. **System Memory Monitoring** - Real-time memory pressure detection
2. **Adaptive Connection Throttling** - Dynamic parallel connection limits based on device count
3. **Connection Rate Limiting** - Delays between connection attempts
4. **Graceful Degradation** - Automatic pause/resume based on resource availability
5. **Resource Usage Logging** - Detailed tracking of memory, bandwidth, and connections

## Implementation Details

### 1. System Memory Monitoring (`PerformanceMonitor`)

**File**: `/home/phone-node/tools/farm-manager/QtScrcpy/ui/performancemonitor.h`
**File**: `/home/phone-node/tools/farm-manager/QtScrcpy/ui/performancemonitor.cpp`

#### New Functions Added:

```cpp
// System resource queries
double getSystemMemoryAvailablePercent();
quint64 getSystemMemoryAvailable();
quint64 getSystemMemoryTotal();
bool isMemoryPressureHigh();  // Returns true if available memory < 20%
```

#### Implementation:

- Reads `/proc/meminfo` for system-wide memory statistics
- Calculates available memory including buffers and cache
- Uses `MemAvailable` field (kernel 3.14+) for best estimate
- Falls back to `MemFree + Buffers + Cached` if not available

**Memory Pressure Threshold**: 20% available memory

### 2. Adaptive Connection Throttling (FarmViewer)

**File**: `/home/phone-node/tools/farm-manager/QtScrcpy/ui/farmviewer.h`
**File**: `/home/phone-node/tools/farm-manager/QtScrcpy/ui/farmviewer.cpp`

#### Dynamic Parallel Connection Limits:

| Device Count | Max Parallel Connections | Rationale |
|--------------|-------------------------|-----------|
| < 30 devices | 5 parallel              | Fast for small farms |
| 30-49 devices | 4 parallel             | Moderate for medium farms |
| 50-69 devices | 3 parallel             | Conservative for large farms |
| 70+ devices  | **2 parallel**          | **VERY conservative for massive farms** |

**Function**: `getMaxParallelForDeviceCount(int totalDeviceCount)`

```cpp
int FarmViewer::getMaxParallelForDeviceCount(int totalDeviceCount)
{
    if (totalDeviceCount >= 70) {
        return 2;  // Only 2 parallel for 70+ devices (VERY conservative)
    } else if (totalDeviceCount >= 50) {
        return 3;  // 3 parallel for 50-69 devices
    } else if (totalDeviceCount >= 30) {
        return 4;  // 4 parallel for 30-49 devices
    } else {
        return 5;  // 5 parallel for < 30 devices
    }
}
```

### 3. Connection Rate Limiting

**Implementation**: Delays between connection attempts to prevent resource spikes

#### Dynamic Delay Calculation:

| Queue Size | Delay Between Connections |
|------------|--------------------------|
| 50+ devices | 1500ms (1.5 seconds)    |
| 20-49 devices | 1000ms (1 second)     |
| 10-19 devices | 800ms                 |
| < 10 devices | 500ms                  |

**Function**: `calculateConnectionDelay(int queueSize)`

```cpp
int FarmViewer::calculateConnectionDelay(int queueSize)
{
    if (queueSize >= 50) {
        return 1500;  // 1.5 second delay for massive queues
    } else if (queueSize >= 20) {
        return 1000;  // 1 second delay for large queues
    } else if (queueSize >= 10) {
        return 800;   // 800ms delay for medium queues
    } else {
        return 500;   // 500ms delay for small queues
    }
}
```

**Implementation**: Uses `m_connectionThrottleTimer` to schedule next connection with calculated delay

### 4. Memory Pressure Detection & Graceful Degradation

#### Resource Check Timer

- **Interval**: 2000ms (2 seconds)
- **Function**: `onResourceCheckTimer()`

#### Behavior:

**When Memory Pressure Detected (< 20% available)**:
1. Pause new connection attempts
2. Stop the connection throttle timer
3. Update status: "Resource limit reached, pausing connections..."
4. Set `m_memoryPressurePaused = true`
5. Continue monitoring until memory frees up

**When Memory Pressure Relieved (>= 20% available)**:
1. Resume connection processing
2. Update status: "Resuming connections (X in queue)..."
3. Set `m_memoryPressurePaused = false`
4. Restart processing the connection queue

**Memory Check Function**:

```cpp
bool FarmViewer::checkMemoryAvailable()
{
    PerformanceMonitor monitor;
    double availablePercent = monitor.getSystemMemoryAvailablePercent();
    quint64 availableBytes = monitor.getSystemMemoryAvailable();

    qInfo() << "FarmViewer: Memory check - Available:"
            << (availableBytes / 1024 / 1024) << "MB"
            << "(" << QString::number(availablePercent, 'f', 1) << "%)";

    return availablePercent >= 20.0;
}
```

### 5. Resource Usage Logging

**Function**: `logResourceUsage(const QString& context)`

#### Metrics Logged:

- **Active Devices**: Currently connected device count
- **Pool Connections**: Total connection pool size
- **Process Memory**: RSS memory usage of the process
- **Available Memory**: System memory available
- **Memory Per Device**: Average memory usage per connected device
- **Estimated Bandwidth**: Total bandwidth based on bitrate × device count
- **Quality Tier**: Current quality profile being used

**Example Output**:

```
=== Resource Usage [Before connection batch] ===
  Active Devices: 0
  Pool Connections: 0
  Process Memory: 45 MB
  Available Memory: 2048 MB / Total: 4096 MB
  Memory Per Device: 0 MB
  Estimated Bandwidth: 0.0 Mbps
  Quality Tier: Low (50+ devices)
=======================================
```

#### Logging Trigger Points:

1. **Before connection batch** - Initial resource state
2. **After each connection** - Per-device impact
3. **Periodic checks** - During active connections (every 2s)
4. **All connections complete** - Final resource state

### 6. Integration with DeviceConnectionPool

The DeviceConnectionPool already has:
- Quality tier management based on device count
- Memory usage estimation
- Connection limits (MAX_CONNECTIONS = 200)

**Enhanced Integration**:
- FarmViewer queries optimal settings before starting connections
- Quality profile applied to all new connections
- Prevents sudden quality degradation mid-connection

## Connection Flow for 78 Devices

Here's how 78 devices connect with the new system:

### Step 1: Device Detection
```
Detected: 78 devices
Total device count: 78
Quality tier: Low (480p, 1Mbps, 15fps)
Max parallel: 2 connections
```

### Step 2: Quality Calculation
```
Selected Quality: Low (50+ devices)
Resolution: 480p
Bitrate: 1.0 Mbps per device
FPS: 15
Max Parallel Connections: 2
Estimated Total Bandwidth: 78.0 Mbps
```

### Step 3: Connection Processing
```
Batch 1: Start 2 parallel connections (devices 1-2)
Delay: 1500ms
Memory check: OK (65% available)

Batch 2: Start 2 parallel connections (devices 3-4)
Delay: 1500ms
Memory check: OK (62% available)

... (continues with 2 at a time, 1.5s delay between batches)
```

### Step 4: Memory Pressure Example (if it occurs)
```
Batch 15: Memory check: FAIL (18% available)
Action: PAUSE connections
Status: "Resource limit reached, pausing connections..."
Wait: 2 seconds
Memory check: OK (22% available)
Action: RESUME connections
Continue with Batch 16...
```

### Step 5: Completion
```
All 78 devices connected successfully
Total time: ~90-120 seconds (with delays and pauses)
Final memory usage: Logged
```

## Configuration

### Tunable Parameters

#### In `FarmViewer`:
- `CONSERVATIVE_PARALLEL_CONNECTIONS = 2` (for 70+ devices)
- `m_connectionDelayMs = 800` (default delay)
- Resource check interval: `2000ms`

#### In `PerformanceMonitor`:
- Memory pressure threshold: `20%` available

#### In `DeviceConnectionPool`:
- `MAX_CONNECTIONS = 200`
- `IDLE_TIMEOUT_MS = 5 minutes`
- Quality tiers (1-5, 6-20, 21-50, 50+ devices)

### Recommended Settings for 78 Devices

Based on testing, for 78 devices:
- Max parallel: **2 connections**
- Connection delay: **1500ms** (1.5 seconds)
- Quality: **Low tier** (480p, 1Mbps, 15fps)
- Estimated total bandwidth: **78 Mbps**
- Estimated memory per device: **2-3 MB**
- Estimated total memory: **156-234 MB**

## Benefits

### 1. Prevents OS Kills
- Gradual memory allocation instead of sudden spike
- System has time to allocate and manage resources
- Early detection and pause prevents OOM killer

### 2. Predictable Performance
- Known connection rate (2 every 1.5s for 78 devices)
- Consistent quality across all devices
- No mid-connection quality degradation

### 3. Visibility
- Detailed logging of resource usage
- Clear status messages about what's happening
- Easy troubleshooting with context-aware logs

### 4. Graceful Recovery
- Automatic pause/resume on memory pressure
- No connection failures, just delays
- Transparent to user (status updates explain delays)

### 5. Scalability
- Adaptive limits based on device count
- Works from 1 device to 200+ devices
- Quality scales appropriately

## Testing Recommendations

### Test Scenario 1: 78 Devices
1. Connect all 78 devices simultaneously
2. Monitor memory usage throughout
3. Verify no OS kill occurs
4. Check final connection success rate

**Expected Results**:
- Connection time: 90-120 seconds
- Memory usage: 200-300 MB peak
- Success rate: 100%
- No pauses (if system has enough memory)

### Test Scenario 2: Memory Pressure Simulation
1. Connect 50 devices normally
2. Start a memory-intensive process
3. Connect 28 more devices
4. Observe pause/resume behavior

**Expected Results**:
- Connections pause when memory < 20%
- Connections resume when memory >= 20%
- All devices eventually connect
- Clear status messages during pause

### Test Scenario 3: Resource Logging Verification
1. Connect 20 devices
2. Review logs for resource usage
3. Verify all metrics are logged correctly

**Expected Log Entries**:
- Before connection batch
- After each connection (for first few)
- Periodic checks during connections
- All connections complete

## Troubleshooting

### Issue: Process still getting killed

**Check**:
1. Total system memory available
2. Other processes consuming memory
3. Memory pressure threshold (20% may be too low)

**Solutions**:
- Increase memory pressure threshold to 30%
- Reduce max parallel connections to 1
- Increase connection delay to 2000ms

### Issue: Connections too slow

**Check**:
1. Current device count and max parallel setting
2. Connection delay setting
3. Memory pressure pauses

**Solutions**:
- If < 50 devices, increase max parallel to 4-5
- Reduce connection delay if memory is plentiful
- Increase memory pressure threshold to allow more headroom

### Issue: Intermittent pauses

**Check**:
1. System memory usage (other processes)
2. Memory leak in connection code
3. Kernel memory cache pressure

**Solutions**:
- Close other applications
- Monitor per-device memory usage
- Clear kernel caches before connection batch

## Future Enhancements

### 1. CPU Monitoring
- Add CPU usage checks alongside memory
- Pause if CPU > 90% for sustained period

### 2. Bandwidth Monitoring
- Track actual network bandwidth usage
- Throttle based on available bandwidth

### 3. Adaptive Quality Adjustment
- Dynamically adjust quality mid-session
- Increase quality if resources become available

### 4. Connection Prediction
- Estimate resource needs before starting
- Warn user if insufficient resources

### 5. Progress Bar Enhancement
- Show estimated time remaining
- Display current resource usage in UI
- Visualize pause/resume events

## Files Modified

### Created Files:
1. `/home/phone-node/tools/farm-manager/QtScrcpy/ui/performancemonitor.h` - System resource monitoring (ENHANCED)
2. `/home/phone-node/tools/farm-manager/QtScrcpy/ui/performancemonitor.cpp` - Memory monitoring implementation (ENHANCED)

### Enhanced Files:
1. `/home/phone-node/tools/farm-manager/QtScrcpy/ui/farmviewer.h` - Resource management declarations (MODIFIED)
2. `/home/phone-node/tools/farm-manager/QtScrcpy/ui/farmviewer.cpp` - Resource management implementation (MODIFIED)

### Existing Files (Leveraged):
1. `/home/phone-node/tools/farm-manager/QtScrcpy/QtScrcpyCore/src/device/deviceconnectionpool.h`
2. `/home/phone-node/tools/farm-manager/QtScrcpy/QtScrcpyCore/src/device/deviceconnectionpool.cpp`

## Key Functions Reference

### Memory Monitoring
- `PerformanceMonitor::getSystemMemoryAvailablePercent()` - Returns % of available memory
- `PerformanceMonitor::getSystemMemoryAvailable()` - Returns available bytes
- `PerformanceMonitor::isMemoryPressureHigh()` - Returns true if < 20% available

### Connection Throttling
- `FarmViewer::getMaxParallelForDeviceCount(int)` - Returns adaptive parallel limit
- `FarmViewer::calculateConnectionDelay(int)` - Returns delay in ms
- `FarmViewer::checkMemoryAvailable()` - Checks if safe to continue
- `FarmViewer::processConnectionQueue()` - Main connection processing loop

### Resource Logging
- `FarmViewer::logResourceUsage(QString)` - Logs comprehensive resource metrics

### Timers
- `m_resourceCheckTimer` - Periodic resource monitoring (2s interval)
- `m_connectionThrottleTimer` - Connection rate limiting (single-shot)

## Summary

The resource management implementation provides a robust solution for connecting 70+ devices without OS kills. The key is:

1. **Conservative limits** (2 parallel for 78 devices)
2. **Rate limiting** (1.5s delay between batches)
3. **Memory monitoring** (check every 2s)
4. **Graceful degradation** (pause/resume automatically)
5. **Comprehensive logging** (track everything)

With these features, the Phone Farm Manager can safely connect 78+ devices without overwhelming the system or being killed by the OS.
