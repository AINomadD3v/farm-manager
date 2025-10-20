# Testing Farm Viewer Already-Connected Devices Fix

## Quick Test Steps

### Test 1: Open Farm Viewer AFTER Connecting Devices (Main Test)

1. **Setup:**
   ```bash
   cd /home/phone-node/tools/farm-manager/output/x64/Release
   ./QtScrcpy
   ```

2. **Connect devices in main Dialog:**
   - Click "Update Devices" button
   - Wait for devices to appear in list
   - Click "Connect All" (or connect 2-3 devices individually)
   - **Verify:** See video streams in main Dialog window
   - **Verify:** Device status shows "Connected" or "Streaming"

3. **Open Farm Viewer:**
   - Click "Farm Manager" button/tab
   - Farm Viewer window opens

4. **Expected Result:**
   - ✓ Farm Viewer immediately shows all connected devices in grid layout
   - ✓ Video appears in ALL tiles **without clicking**
   - ✓ Status shows "Streaming" for all devices
   - ✓ NO "Ready to Connect" placeholders for already-connected devices

5. **Success Criteria:**
   - Video frames visible in Farm Viewer tiles immediately
   - No need to click devices to start streaming
   - Log shows: "Registered VideoForm as observer for already-connected device"

### Test 2: Click Handler for Already-Connected Device

1. **Setup:** Follow Test 1 to get Farm Viewer showing connected devices

2. **Click a streaming device:**
   - Click any tile showing video

3. **Expected Result:**
   - ✓ Device disconnects
   - ✓ Video stops
   - ✓ Status changes to "Ready to Connect"
   - ✓ Placeholder appears

4. **Click again to reconnect:**
   - Click the same tile again

5. **Expected Result:**
   - ✓ Device connects
   - ✓ Video starts streaming
   - ✓ Status changes: "Connecting..." → "Streaming"

### Test 3: Edge Case - Manual Connection After Farm Viewer Opens

1. **Open Farm Viewer FIRST:**
   - Start QtScrcpy
   - Click "Farm Manager" before connecting any devices
   - Farm Viewer shows "No devices detected"

2. **Connect devices in background:**
   - Switch back to main Dialog window (don't close Farm Viewer)
   - Click "Update Devices"
   - Connect 1-2 devices
   - **Verify:** Devices stream in main Dialog

3. **Switch back to Farm Viewer:**
   - Farm Viewer still shows no devices (ADB detection ran before connection)

4. **Add devices manually:**
   - Close Farm Viewer
   - Click "Farm Manager" again to reopen
   - **OR** trigger device re-detection

5. **Expected Result:**
   - ✓ Farm Viewer detects already-connected devices
   - ✓ Video appears immediately
   - ✓ Logs show observer registration

## Log Verification

### Success Logs

When Farm Viewer opens with 2 connected devices:

```
FarmViewer: showFarmViewer() called
FarmViewer: Loading already-connected devices...
DeviceManage::getAllConnectedSerials() called
  m_devices map size: 2
  Connected serials: ("192.168.40.121:5555", "192.168.40.122:5555")
    - 192.168.40.121:5555 device pointer: valid
    - 192.168.40.122:5555 device pointer: valid
FarmViewer: Found 2 already-connected devices
FarmViewer: Adding already-connected device: 192.168.40.121:5555
FarmViewer: Registered VideoForm as observer for already-connected device: 192.168.40.121:5555
FarmViewer: Device marked as streaming: 192.168.40.121:5555 Active connections: 1
FarmViewer: Adding already-connected device: 192.168.40.122:5555
FarmViewer: Registered VideoForm as observer for already-connected device: 192.168.40.122:5555
FarmViewer: Device marked as streaming: 192.168.40.122:5555 Active connections: 2
```

### When Clicking Already-Connected Device

```
FarmViewer: Device tile clicked: 192.168.40.121:5555
FarmViewer: Device already connected in DeviceManage, registering observer: 192.168.40.121:5555
FarmViewer: Registered VideoForm as observer for existing connection: 192.168.40.121:5555
FarmViewer: Device now streaming in FarmViewer: 192.168.40.121:5555 Active connections: 1
```

### Failure Logs (OLD BEHAVIOR - Should NOT See)

```
❌ Device already connected: 192.168.40.121:5555  (Should not appear anymore)
❌ FarmViewer: Connection failed  (Should not happen for already-connected devices)
```

## Debugging

### If Video Doesn't Appear

1. **Check logs for observer registration:**
   ```bash
   # Should see this line:
   grep "Registered VideoForm as observer" logs.txt
   ```

2. **Check DeviceManage map:**
   ```bash
   # Should show connected devices:
   grep "m_devices map size" logs.txt
   ```

3. **Check if devices are actually connected:**
   - Look at main Dialog window
   - Verify video streaming there
   - Check connection status

4. **Check for errors:**
   ```bash
   grep -i "error\|fail" logs.txt
   ```

### If "Device Already Connected" Error Still Appears

This means the click handler fix didn't work:

1. **Check the logs:**
   - Should see: "Device already connected in DeviceManage, registering observer"
   - Should NOT see: "Device is disconnected, connecting"

2. **Verify code changes:**
   ```bash
   cd /home/phone-node/tools/farm-manager
   grep -A 20 "onDeviceTileClicked" QtScrcpy/ui/farmviewer.cpp
   ```

3. **Rebuild:**
   ```bash
   cd build
   make -j$(nproc)
   ```

## Performance Testing

### With 10 Devices

1. Connect 10 devices in main Dialog
2. Open Farm Viewer
3. **Verify:**
   - All 10 devices appear in grid
   - All 10 show video immediately
   - No lag or stuttering
   - Grid layout looks good (adaptive sizing)

### With 20+ Devices

1. Connect 20+ devices in main Dialog
2. Open Farm Viewer
3. **Verify:**
   - All devices appear in grid
   - Video quality reduced appropriately (adaptive quality)
   - Grid has more rows/columns (adaptive layout)
   - No crashes or freezes

## Common Issues

### Issue: Farm Viewer shows "Ready to Connect" instead of streaming

**Cause:** Observer registration didn't happen
**Fix:** Click the device tile - it should register and start streaming

### Issue: Video appears in main Dialog but not Farm Viewer

**Cause:** Observer not registered
**Solution:**
1. Check logs for "Registered VideoForm as observer"
2. If missing, click device tile to trigger registration

### Issue: Multiple devices don't all appear

**Cause:** `getAllConnectedSerials()` not returning all devices
**Solution:**
1. Check DeviceManage logs: "m_devices map size"
2. Verify devices are actually connected in main Dialog
3. Try disconnecting and reconnecting devices

## Success Checklist

- [ ] Video appears in Farm Viewer without clicking
- [ ] All connected devices show up automatically
- [ ] No "Device already connected" errors in logs
- [ ] Clicking device disconnects (not reconnects)
- [ ] Logs show observer registration
- [ ] Multiple devices work (tested with 2+)
- [ ] Performance is good (no lag)

## Rollback Plan

If the fix causes issues:

```bash
cd /home/phone-node/tools/farm-manager
git diff QtScrcpy/ui/farmviewer.cpp QtScrcpyCore/src/devicemanage/devicemanage.cpp
git checkout QtScrcpy/ui/farmviewer.cpp QtScrcpyCore/src/devicemanage/devicemanage.cpp
cd build
make -j$(nproc)
```
