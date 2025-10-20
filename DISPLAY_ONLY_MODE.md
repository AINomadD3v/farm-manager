# Phone Farm Manager - Display-Only Mode

## Summary

The Phone Farm Manager has been simplified to **display devices without automatically connecting or streaming video**. This prevents crashes when detecting large numbers of devices (e.g., 78 devices).

## What Changed

### Previous Behavior (PROBLEMATIC)
1. Detected all 78 devices via ADB
2. Added them to a connection queue
3. Attempted to connect and stream video from each device sequentially
4. **CRASH**: System overload from trying to manage 78 video streams

### New Behavior (SIMPLIFIED)
1. Detect all 78 devices via ADB ✓
2. Display them in the FarmViewer UI grid ✓
3. Show status: "78 devices detected (ready for connection)" ✓
4. **NO automatic connection or video streaming** ✓

## Key Code Changes

### 1. processDetectedDevices() - Simplified
**File**: `/home/phone-node/tools/farm-manager/QtScrcpy/ui/farmviewer.cpp`

**Before**: Added devices to connection queue and started sequential connections
**After**: Just calls `addDevice()` to display UI placeholder

```cpp
void FarmViewer::processDetectedDevices(const QStringList& devices)
{
    // SIMPLIFIED: Just display devices without connecting/streaming
    for (const QString& serial : devices) {
        if (!m_deviceForms.contains(serial)) {
            // Display device with default size - NO CONNECTION
            addDevice(serial, serial, QSize(720, 1280));
        }
    }

    // Update status
    m_statusLabel->setText(QString("%1 devices detected").arg(devices.size()));
}
```

### 2. addDevice() - No Observer Registration
**Before**: Registered VideoForm as observer to receive video frames
**After**: Just creates UI placeholder

```cpp
void FarmViewer::addDevice(const QString& serial, const QString& deviceName, const QSize& size)
{
    // Create VideoForm UI placeholder
    auto videoForm = new VideoForm(true, false, false, this);
    videoForm->setSerial(serial);

    // Store references
    m_deviceForms[serial] = videoForm;
    m_deviceContainers[serial] = container;

    // SIMPLIFIED: Do NOT register observers or connect to device
    // Just display the placeholder UI
}
```

### 3. Removed Connection Queue Logic
- Removed `m_connectionQueue` member variable
- Removed `m_activeConnections` tracking
- Removed `m_maxParallelConnections` logic
- Removed `processConnectionQueue()` implementation
- Removed `onConnectionComplete()` implementation
- Removed connection throttle timer usage

### 4. Removed IDeviceManage Signal Connections
**Before**: Connected to `deviceConnected` and `deviceDisconnected` signals
**After**: No signal connections (display-only mode)

### 5. Updated Status Messages
- Changed "No devices connected" → "No devices detected"
- Changed "X devices connected" → "X devices detected (ready for connection)"
- Removed connection progress messages

### 6. Simplified Cleanup
**Before**: Deregistered observers, disconnected devices, released pool connections
**After**: Just deletes UI widgets

## Benefits

1. **No Crashes**: System doesn't attempt to stream from 78 devices simultaneously
2. **Fast Display**: All 78 devices appear in grid immediately
3. **Low Resource Usage**: Only UI widgets, no video decoders or streams
4. **Scalable**: Can display 100+ devices without issues

## Future Enhancements (Optional)

To enable video streaming for specific devices:

1. **Click-to-Connect**: User clicks a device tile to start streaming
2. **Select Devices**: Checkbox to select which devices to connect
3. **Connect Button**: "Connect Selected" button to initiate connections
4. **Limit Connections**: Warn if user tries to connect too many at once

## Files Modified

1. `/home/phone-node/tools/farm-manager/QtScrcpy/ui/farmviewer.cpp`
   - processDetectedDevices()
   - addDevice()
   - removeDevice()
   - updateStatus()
   - cleanupAndExit()
   - processConnectionQueue() (now empty)
   - onConnectionComplete() (now empty)
   - Constructor initialization

2. `/home/phone-node/tools/farm-manager/QtScrcpy/ui/farmviewer.h`
   - Removed connection queue member variables
   - Kept timer pointers for compatibility (unused)

## Testing

Run the application with 78 devices:
```bash
cd /home/phone-node/tools/farm-manager/QtScrcpy/build
./QtScrcpy
```

**Expected Result**:
- Window opens immediately
- All 78 devices displayed in grid
- Status shows: "78 devices detected (ready for connection)"
- No connection attempts
- No crashes
- Low memory usage
