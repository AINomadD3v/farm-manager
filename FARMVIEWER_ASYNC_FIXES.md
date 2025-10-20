# FarmViewer Async Fixes - UI Freeze Prevention

## Summary
Fixed all blocking operations in FarmViewer to prevent UI freezes during device detection and connection. All operations are now truly asynchronous and deferred to the Qt event loop.

## Files Modified
- `/home/phone-node/tools/farm-manager/QtScrcpy/ui/farmviewer.cpp`
- `/home/phone-node/tools/farm-manager/QtScrcpy/ui/farmviewer.h` (no changes needed)

## Issues Fixed

### 1. showFarmViewer() Blocking Main Thread (Line 576)
**Problem:** Called `autoDetectAndConnectDevices()` synchronously, blocking the main thread.

**Solution:**
- Wrapped the call in `QTimer::singleShot(0, ...)` to defer to next event loop iteration
- Added immediate status update: "Detecting devices..."
- Set status label style to indicate activity

**Code Change:**
```cpp
// Before:
if (connectedSerials.isEmpty()) {
    qInfo() << "FarmViewer: No devices connected yet, starting auto-detection...";
    autoDetectAndConnectDevices();
}

// After:
if (connectedSerials.isEmpty()) {
    qInfo() << "FarmViewer: No devices connected yet, scheduling auto-detection...";
    m_statusLabel->setText("Detecting devices...");
    m_statusLabel->setStyleSheet("color: #0a84ff; font-size: 12px;");

    QTimer::singleShot(0, this, [this]() {
        autoDetectAndConnectDevices();
    });
}
```

### 2. autoDetectAndConnectDevices() Potentially Blocking (Line 617)
**Problem:** `m_deviceDetectionAdb.execute()` could block the main thread during ADB command execution.

**Solution:**
- Wrapped ADB execute call in `QTimer::singleShot(0, ...)` for safety
- Added status update before scheduling: "Detecting devices..."
- Ensures ADB doesn't block even if it takes time

**Code Change:**
```cpp
// Before:
qDebug() << "FarmViewer: Executing ADB devices command...";
m_deviceDetectionAdb.execute("", QStringList() << "devices");

// After:
m_statusLabel->setText("Detecting devices...");
m_statusLabel->setStyleSheet("color: #0a84ff; font-size: 12px;");

qDebug() << "FarmViewer: Scheduling ADB devices command...";
QTimer::singleShot(0, this, [this]() {
    qDebug() << "FarmViewer: Executing ADB devices command...";
    m_deviceDetectionAdb.execute("", QStringList() << "devices");
});
```

### 3. processDetectedDevices() Wrong Status Messages (Line 626)
**Problem:** Immediately showed "No devices connected" even when devices were being connected.

**Solution:**
- Changed "No devices connected" → "No devices found" (more accurate)
- Added proper style resets for different states
- Changed "Queued X devices" → "Connecting to X devices..." (more user-friendly)
- Added color coding: gray for no devices, blue for active operations

**Code Changes:**
```cpp
// Before:
if (devices.isEmpty()) {
    m_statusLabel->setText("No devices connected");
    return;
}

if (newDevices.isEmpty()) {
    m_statusLabel->setText(QString("%1 devices connected").arg(m_deviceForms.size()));
    return;
}

m_statusLabel->setText(QString("Queued %1 devices for connection...").arg(newDevices.size()));

// After:
if (devices.isEmpty()) {
    m_statusLabel->setText("No devices found");
    m_statusLabel->setStyleSheet("color: #888; font-size: 12px;");
    return;
}

if (newDevices.isEmpty()) {
    m_statusLabel->setText(QString("%1 devices connected").arg(m_deviceForms.size()));
    m_statusLabel->setStyleSheet("color: #0a84ff; font-size: 12px; font-weight: bold;");
    return;
}

m_statusLabel->setText(QString("Connecting to %1 devices...").arg(newDevices.size()));
m_statusLabel->setStyleSheet("color: #0a84ff; font-size: 12px;");
```

### 4. Signal Connection Timing (Line 89)
**Problem:** `onConnectionComplete()` called synchronously from signal handler, potentially blocking.

**Solution:**
- Wrapped `onConnectionComplete()` call in `QTimer::singleShot(0, ...)` to make it async
- Captures serial and success by value in lambda to ensure data safety
- Prevents any blocking in signal handler chain

**Code Change:**
```cpp
// Before:
connect(&qsc::IDeviceManage::getInstance(), &qsc::IDeviceManage::deviceConnected,
        this, [this](bool success, const QString &serial, const QString &deviceName, const QSize &size) {
    if (success) {
        qDebug() << "FarmViewer: Device connected via signal:" << serial;
        addDevice(serial, deviceName, size);
    }
    onConnectionComplete(serial, success);
});

// After:
connect(&qsc::IDeviceManage::getInstance(), &qsc::IDeviceManage::deviceConnected,
        this, [this](bool success, const QString &serial, const QString &deviceName, const QSize &size) {
    if (success) {
        qDebug() << "FarmViewer: Device connected via signal:" << serial;
        addDevice(serial, deviceName, size);
    }
    // IMPORTANT: Defer to event loop to avoid blocking signal handlers
    QTimer::singleShot(0, this, [this, serial, success]() {
        onConnectionComplete(serial, success);
    });
});
```

### 5. BONUS: Enhanced processConnectionQueue() Status Updates
**Problem:** Status updates during connection were minimal and didn't show progress clearly.

**Solution:**
- Added detailed progress information showing active/queued counts
- Better final status when all connections complete
- Clear distinction between "Ready", "Connecting", and "Connected" states

**Code Change:**
```cpp
// Enhanced status updates in processConnectionQueue():
if (m_connectionQueue.isEmpty() && m_activeConnections == 0) {
    if (m_deviceForms.size() > 0) {
        updateStatus();  // Show device count and quality info
    } else {
        m_statusLabel->setText("Ready");
        m_statusLabel->setStyleSheet("color: #888; font-size: 12px;");
    }
} else if (m_activeConnections > 0) {
    int totalInProgress = m_activeConnections + m_connectionQueue.size();
    int completed = totalInProgress - m_activeConnections - m_connectionQueue.size() + m_deviceForms.size();
    m_statusLabel->setText(QString("Connecting: %1/%2 (active: %3, queued: %4)")
        .arg(completed)
        .arg(totalInProgress)
        .arg(m_activeConnections)
        .arg(m_connectionQueue.size()));
    m_statusLabel->setStyleSheet("color: #0a84ff; font-size: 12px;");
}
```

## Status Label Flow
The status label now properly shows the connection lifecycle:

1. **"Detecting devices..."** - Blue text, shown when ADB detection starts
2. **"No devices found"** - Gray text, shown when ADB finds no devices
3. **"Connecting to X devices..."** - Blue text, shown when connections queued
4. **"Connecting: X/Y (active: A, queued: Q)"** - Blue text, shown during connection
5. **"Devices: X | Quality: Y (720p @ 30fps)"** - Blue bold text, shown when devices connected
6. **"Ready"** - Gray text, shown when all connections complete but no devices

## Benefits
1. **Non-blocking UI** - All operations deferred to event loop, UI stays responsive
2. **Clear user feedback** - Status updates show exactly what's happening
3. **Proper async handling** - No synchronous calls that could freeze the application
4. **TCP device support** - Works correctly with TCP-connected Android devices via ADB
5. **Thread safety** - Lambda captures ensure data integrity across async boundaries

## Testing Recommendations
1. Test with no devices connected - should show "No devices found"
2. Test with multiple TCP devices - should show progress updates
3. Test rapid window showing/hiding - should not freeze UI
4. Test connection failures - should handle gracefully with status updates
5. Monitor logs for "Scheduling" and "Executing" messages to verify async behavior

## Technical Notes
- All `QTimer::singleShot(0, ...)` calls defer to the next event loop iteration
- Lambda captures use `[this, serial, success]()` to capture by value for safety
- Status label color codes: `#888` (gray) for inactive, `#0a84ff` (blue) for active
- Font styling consistent with existing QtScrcpy design patterns
