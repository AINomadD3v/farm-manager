# FarmViewer Async Fix Test Plan

## Test Environment Setup
1. Ensure you have ADB installed and configured
2. Have at least 2-3 Android devices connected via TCP (use `adb connect <ip>:5555`)
3. Build the application with the fixes
4. Monitor application logs for async operation messages

## Test Cases

### Test 1: No Devices Available
**Steps:**
1. Disconnect all ADB devices (`adb disconnect`)
2. Launch the application
3. Observe the FarmViewer window

**Expected Results:**
- Status shows "Detecting devices..." briefly (blue text)
- Status changes to "No devices found" (gray text)
- UI remains responsive throughout
- No freezing or blocking

**Log Messages to Verify:**
```
FarmViewer: No devices connected yet, scheduling auto-detection...
FarmViewer: Scheduling ADB devices command...
FarmViewer: Executing ADB devices command...
FarmViewer: No devices detected
```

---

### Test 2: Multiple TCP Devices Connected
**Steps:**
1. Connect 3-5 devices via TCP: `adb connect 192.168.1.100:5555`, etc.
2. Verify devices are visible: `adb devices`
3. Launch the application
4. Watch the status label updates

**Expected Results:**
- Status shows "Detecting devices..." (blue text)
- Status changes to "Connecting to X devices..." (blue text)
- Status shows "Connecting: X/Y (active: A, queued: Q)" during connection
- Status shows "Devices: X | Quality: Y (720p @ 30fps)" when complete
- Each device appears in the grid as it connects
- UI remains smooth and responsive

**Log Messages to Verify:**
```
FarmViewer: Found X devices
FarmViewer: Adding X devices to connection queue
FarmViewer: Starting connection to <serial>
FarmViewer: Device connected via signal: <serial>
FarmViewer: Connection successful for <serial>
```

---

### Test 3: Rapid Window Show/Hide
**Steps:**
1. Have 2-3 devices connected
2. Show the FarmViewer window
3. Immediately hide it (before devices connect)
4. Show it again
5. Repeat 3-4 times quickly

**Expected Results:**
- No crashes or freezes
- UI remains responsive
- Duplicate device detection is prevented
- Already-connected devices are shown immediately
- Status updates work correctly

**Log Messages to Verify:**
```
FarmViewer: Loading already-connected devices...
FarmViewer: Found X already-connected devices
FarmViewer: Adding already-connected device: <serial>
```

---

### Test 4: Connection Failures
**Steps:**
1. Connect to a device via ADB
2. Note the device serial
3. Disconnect the device physically/network-wise but don't `adb disconnect`
4. Launch the application

**Expected Results:**
- Status shows connection attempts
- Failed devices are logged
- UI doesn't freeze waiting for failed connections
- Status eventually shows successful device count
- Queue continues processing despite failures

**Log Messages to Verify:**
```
FarmViewer: Connection failed for <serial>
FarmViewer: Connection complete (Active: X Queue: Y)
```

---

### Test 5: Large Device Count (Stress Test)
**Steps:**
1. Connect 10+ devices via TCP (if available)
2. Launch the application
3. Monitor connection progress

**Expected Results:**
- Parallel connection limit (MAX_PARALLEL_CONNECTIONS = 5) is respected
- Status shows correct active/queued counts
- UI remains responsive throughout
- Grid layout adjusts correctly
- Quality tier changes based on device count

**Log Messages to Verify:**
```
FarmViewer: Starting connection to <serial> (Active: 5 Queue: X)
FarmViewer: Quality tier changed from X to Y
FarmViewer: Connection in progress - Active: 5 Queue: X
```

---

### Test 6: Already Connected Devices
**Steps:**
1. Connect to 2-3 devices manually via Dialog
2. Open FarmViewer
3. Observe behavior

**Expected Results:**
- Status does NOT show "Detecting devices..."
- Already connected devices appear immediately
- No duplicate connections attempted
- Status shows correct device count immediately

**Log Messages to Verify:**
```
FarmViewer: Found 3 already-connected devices
FarmViewer: Adding already-connected device: <serial>
FarmViewer: Device already exists in farm viewer: <serial>
```

---

### Test 7: Event Loop Deferral Verification
**Steps:**
1. Add debug timestamps to log messages
2. Connect 2-3 devices
3. Launch application
4. Review timestamps

**Expected Results:**
- "Scheduling" message appears before "Executing" message
- Time gap exists between scheduling and execution (proves async)
- UI show() completes before detection starts
- All operations logged in correct async order

**Example Log Sequence:**
```
[T+0ms]   FarmViewer: showFarmViewer() called
[T+1ms]   FarmViewer: show() completed
[T+2ms]   FarmViewer: No devices connected yet, scheduling auto-detection...
[T+3ms]   FarmViewer: showFarmViewer() completed
[T+10ms]  FarmViewer: Scheduling ADB devices command...
[T+20ms]  FarmViewer: Executing ADB devices command...
```

---

## Performance Metrics to Monitor

### UI Responsiveness
- Window should appear within 100ms
- Status updates should be visible immediately
- No frame drops during connection process
- Smooth grid layout updates

### Connection Performance
- ADB device detection: < 500ms
- Per-device connection: 1-3 seconds (normal)
- 5 parallel connections at a time (configurable via MAX_PARALLEL_CONNECTIONS)

### Memory Usage
- No memory leaks from lambda captures
- QPointer usage prevents dangling pointers
- Proper cleanup on device disconnect

## Known Limitations

1. **ADB Command Timeout**: If ADB hangs, the UI will still be responsive but detection won't complete
2. **Network Latency**: TCP device connections depend on network quality
3. **Device Offline**: Devices shown by `adb devices` but offline will fail to connect
4. **Quality Changes**: Stream quality changes only apply to new connections, not existing ones

## Success Criteria

All tests should pass with:
- ✅ No UI freezes or blocking operations
- ✅ Proper status label updates showing current state
- ✅ Async operations logged with "Scheduling" → "Executing" pattern
- ✅ Responsive window operations throughout
- ✅ Correct device connection handling
- ✅ Proper error handling and recovery

## Tools for Testing

```bash
# Connect TCP device
adb connect 192.168.1.100:5555

# List all devices
adb devices -l

# Disconnect specific device
adb disconnect 192.168.1.100:5555

# Disconnect all
adb disconnect

# Monitor application logs
./QtScrcpy 2>&1 | grep "FarmViewer:"

# Monitor with timestamps
./QtScrcpy 2>&1 | ts '[%Y-%m-%d %H:%M:%.S]' | grep "FarmViewer:"
```

## Regression Testing

After any changes to FarmViewer:
1. Re-run all test cases above
2. Verify log messages match expected patterns
3. Check for new Qt warnings or errors
4. Monitor for memory leaks with valgrind
5. Test with varying device counts (1, 5, 10, 20+)
