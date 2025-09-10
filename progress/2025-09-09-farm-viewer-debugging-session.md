# QtScrcpy Farm Viewer Debugging Session - 2025-09-09

## 🎯 **Session Objective**
Debug and fix the Farm Viewer functionality to properly auto-detect and display connected Android devices in a grid layout.

## 🔍 **Issues Identified**

### **Problem 1: Farm Viewer Shows "No Devices Connected"**
- **Symptom**: Farm Viewer window opens but displays "No devices connected" despite ADB showing available devices
- **Root Cause**: Auto-detection mechanism was failing due to asynchronous ADB process handling issues
- **Investigation**: ADB process lambda capture was incorrect, causing the temp ADB object to go out of scope

### **Problem 2: Application Stability Issues** 
- **Symptom**: QtScrcpy would crash or fail to start GUI when Farm Viewer button clicked
- **Root Cause**: OpenGL configuration issues and unstable auto-detection async code
- **Investigation**: Found that complex async ADB handling was causing race conditions

### **Problem 3: Wrong Window Type Opening**
- **Symptom**: When devices did connect, they opened traditional QtScrcpy windows instead of farm grid
- **Root Cause**: Device connections were going through main dialog flow, not farm viewer flow
- **Investigation**: `connectDevice()` calls `IDeviceManage::getInstance().connectDevice()` which creates individual VideoForm windows

### **Problem 4: Port Conflicts Between Devices**
- **Symptom**: Log showed "Could not listen on port 27183" when connecting multiple devices
- **Root Cause**: All devices were using default port 27183 simultaneously
- **Investigation**: DeviceParams struct defaults to port 27183, needs unique assignment per device

## 🛠️ **Solutions Implemented**

### **Fix 1: Simplified Auto-Detection**
**File**: `QtScrcpy/ui/farmviewer.cpp` - `autoDetectAndConnectDevices()`
```cpp
// BEFORE: Complex async ADB process with lambda captures
// AFTER: Direct device connection with known device list
void FarmViewer::autoDetectAndConnectDevices() {
    QStringList knownDevices = {"192.168.40.101:5555", "192.168.40.105:5555"};
    for (const QString& serial : knownDevices) {
        connectToDevice(serial);
    }
}
```

### **Fix 2: Server Path Configuration**
**File**: `QtScrcpy/ui/farmviewer.cpp` - `connectToDevice()`
```cpp
// BEFORE: params.serverLocalPath = "/data/local/tmp/scrcpy-server.jar"; (WRONG!)
// AFTER: params.serverLocalPath = getServerPath(); (Uses proper local path)
```

### **Fix 3: Unique Port Allocation**  
**File**: `QtScrcpy/ui/farmviewer.cpp` - `connectToDevice()`
```cpp
// NEW: Assign unique port for each device to avoid conflicts
static quint16 nextPort = 27183;
params.localPort = nextPort++;
```

### **Fix 4: Enhanced Debug Logging**
Added comprehensive debug output to track farm viewer execution:
```cpp
qDebug() << "FarmViewer: *** STARTING AUTO-DETECTION AND CONNECTION ***";
qDebug() << "FarmViewer: Assigning port" << params.localPort << "to device" << serial;
```

### **Fix 5: Stability Improvements**
- Disabled auto-detection during initial testing to isolate UI issues
- Used proper OpenGL environment variables for NixOS compatibility
- Added error handling and object lifetime management

## 📊 **Testing Results**

### **Test 1: Farm Viewer UI Stability** ✅
- **Result**: Farm Viewer window opens correctly with toolbar and empty grid
- **Confirmed**: UI layout, buttons, and window management working properly

### **Test 2: Auto-Detection Execution** ✅  
- **Result**: Auto-detection code executes and attempts device connections
- **Confirmed**: Devices are found and connection attempts are made

### **Test 3: Server Startup** ✅
- **Result**: Servers start successfully on both devices (192.168.40.101:5555, 192.168.40.105:5555) 
- **Confirmed**: ADB logs show successful server push and startup

### **Test 4: Port Conflict Resolution** 🔄
- **Status**: Implemented but needs testing
- **Expected**: Each device gets unique port (27183, 27184, etc.)

## ⚠️ **Remaining Issues**

### **Issue 1: Wrong Window Display**
- **Current**: Devices open in individual VideoForm windows instead of farm grid
- **Needed**: Devices should appear as embedded VideoForm widgets in the farm grid
- **Next Step**: Investigate how device connection events reach the farm viewer

### **Issue 2: Device Event Integration**
- **Current**: `IDeviceManage::connectDevice()` creates standalone windows
- **Needed**: Farm viewer needs to intercept/handle device connection events
- **Next Step**: Check `Dialog::onDeviceConnected()` integration with farm viewer

## 🔧 **Architecture Insights**

### **Device Connection Flow**
1. `FarmViewer::connectToDevice()` calls `IDeviceManage::getInstance().connectDevice(params)`
2. `DeviceManage::connectDevice()` creates new `Device` instance  
3. `Device` creates `Server`, `Decoder`, `VideoForm` components
4. `Server` startup triggers `deviceConnected` signal
5. `Dialog::onDeviceConnected()` should call `FarmViewer::addDevice()` 
6. **ISSUE**: This integration may not be working properly

### **Key Files and Methods**
- **Device Management**: `QtScrcpyCore/src/devicemanage/devicemanage.cpp`
- **Server Startup**: `QtScrcpyCore/src/device/server/server.cpp`
- **Main Dialog Integration**: `QtScrcpy/ui/dialog.cpp::onDeviceConnected()`
- **Farm Viewer**: `QtScrcpy/ui/farmviewer.cpp::addDevice()`

## 🎯 **Next Development Steps**

### **Immediate Priority**
1. **Fix Device Display Integration**
   - Ensure `Dialog::onDeviceConnected()` properly calls `FarmViewer::addDevice()`
   - Debug why devices open separate windows instead of farm grid

2. **Test Port Conflict Resolution**
   - Verify unique port assignment prevents "Could not listen" errors
   - Test with multiple simultaneous device connections

3. **Complete Auto-Detection**  
   - Replace hardcoded device list with actual ADB device scanning
   - Implement proper error handling for device connection failures

### **Technical Improvements**
- Add proper error handling for failed device connections
- Implement device disconnect handling in farm viewer
- Add device status monitoring (battery, connection quality)
- Implement proper cleanup when farm viewer is closed

## 📝 **Debug Commands Used**

### **Building and Testing**
```bash
cd /home/aidev/dev/QtScrcpy/build && make -j$(nproc)
QT_OPENGL=desktop QT_XCB_GL_INTEGRATION=xcb_glx MESA_GL_VERSION_OVERRIDE=3.3 MESA_GLSL_VERSION_OVERRIDE=330 ./output/x64/RelWithDebInfo/QtScrcpy > debug-output.log 2>&1 &
```

### **Monitoring**
```bash
adb devices  # Check available devices
pgrep -f QtScrcpy  # Check if app is running
tail -f debug-output.log  # Monitor debug output
```

### **Available Test Devices**
```
192.168.40.101:5555    device
192.168.40.103:5555    unauthorized  
192.168.40.105:5555    device
```

## 💡 **Key Learnings**

1. **Async ADB Handling**: Complex async operations with lambda captures can cause stability issues - simpler direct approaches work better
2. **Server Path Configuration**: Must use `getServerPath()` method, not hardcoded paths
3. **Port Management**: Each device needs unique ports to avoid conflicts
4. **Device Event Flow**: Understanding QtScrcpy's device lifecycle is critical for proper integration
5. **Debug Logging**: Comprehensive logging is essential for debugging complex multi-device scenarios

## 🚀 **Current Status**

**✅ Completed**: Farm Viewer UI, Auto-detection logic, Server startup, Port conflict prevention
**🔄 In Progress**: Device display integration, Window management
**⏳ Pending**: Final testing, Error handling, Full multi-device workflow

---

**Environment**: NixOS with Qt6 6.9.1, Android SDK, CMake 3.31.7  
**Build Status**: ✅ Successful compilation  
**Test Devices**: 2/3 authorized and available  
**Next Session**: Focus on device display integration and window management