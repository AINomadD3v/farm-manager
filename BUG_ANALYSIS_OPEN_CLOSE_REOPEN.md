# Critical Bug Analysis: Open-Close-Reopen on First Launch

## Executive Summary
The farm manager application exhibits a reproducible bug where it **opens, immediately closes, then reopens and works correctly** on first launch. This document provides a thorough technical analysis of the root cause based on code examination and initialization flow analysis.

---

## 1. APPLICATION LIFECYCLE AND INITIALIZATION FLOW

### Entry Point: main.cpp (lines 1-184)

**Startup Sequence:**
```
main.cpp::main()
  ├─ Qt Application Setup (line 93: QApplication a)
  ├─ Codec Test (lines 124-155: FFmpeg initialization)
  ├─ Unix Signal Handlers (line 160: FarmViewer::setupUnixSignalHandlers())
  ├─ Dialog Creation (line 162: g_mainDlg = new Dialog)
  ├─ Dialog Show (line 163: g_mainDlg->show())
  └─ Event Loop (line 177: a.exec())
```

**Key Detail**: Dialog is the MAIN window shown at startup, not FarmViewer. FarmViewer is only created when user clicks "Farm View" button.

---

## 2. DIALOG WINDOW MANAGEMENT

### Dialog Constructor (dialog.cpp, lines 34-129)

**Critical Issue Found - Line 141 in initUI():**
```cpp
void Dialog::initUI()
{
    setAttribute(Qt::WA_DeleteOnClose);  // LINE 141
```

**The Problem:**
- Dialog sets `Qt::WA_DeleteOnClose` attribute
- This tells Qt to delete the Dialog widget when it receives a close event
- However, Dialog's closeEvent (lines 303-314) calls `event->ignore()`, which prevents the window from closing
- This creates a conflict: attribute says "delete on close" but closeEvent says "don't close"

### Dialog Close Event (dialog.cpp, lines 303-314)

```cpp
void Dialog::closeEvent(QCloseEvent *event)
{
    this->hide();
    if (!Config::getInstance().getTrayMessageShown()) {
        Config::getInstance().setTrayMessageShown(true);
        m_hideIcon->showMessage(tr("Notice"),
                                tr("Hidden here!"),
                                QSystemTrayIcon::Information,
                                3000);
    }
    event->ignore();  // CRITICAL: Prevents close but attribute says delete!
}
```

**Expected Behavior:** Window hides to system tray, stays in memory
**Actual Behavior:** Attribute and closeEvent are in conflict

---

## 3. FARMVIEWER INITIALIZATION AND SINGLETON PATTERN

### FarmViewer Constructor (farmviewer.cpp, lines 28-224)

**Initialization Sequence:**
1. Line 53: Constructor starts
2. Line 55: `setupUI()` called - creates all widgets
3. Lines 58-64: Window flags set explicitly:
   ```cpp
   setWindowFlags(Qt::Window | Qt::WindowTitleHint |
                 Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
   setAttribute(Qt::WA_DeleteOnClose, false);  // Protect singleton
   ```
4. Lines 66-74: Window centered on screen
5. Lines 79-100: Device detection ADB connection setup
6. Lines 104-113: Periodic device detection timer started
7. Lines 118-199: IDeviceManage signal connections
8. Lines 203-210: Unix signal socket notifier setup
9. Lines 212-216: Application aboutToQuit deferred connection
10. Line 223: Constructor completes

**Critical Issue - Deferred Signal Connection (lines 212-216):**
```cpp
QTimer::singleShot(0, this, [this]() {
    connect(qApp, &QApplication::aboutToQuit, this, &FarmViewer::cleanupAndExit);
    qInfo() << "FarmViewer: Cleanup handler connected";
});
```

This defers the cleanup handler connection to the next event loop iteration.

### FarmViewer Singleton Pattern (farmviewer.cpp, lines 264-270)

```cpp
FarmViewer& FarmViewer::instance()
{
    if (!s_instance) {
        s_instance = new FarmViewer();  // Constructor runs
    }
    return *s_instance;
}
```

**The Singleton is Created on First Access** (line 558 in dialog.cpp):
```cpp
FarmViewer& viewer = FarmViewer::instance();
```

---

## 4. ROOT CAUSE ANALYSIS: THE TRIGGER MECHANISM

### Bug Trigger Sequence:

**On First Application Launch:**

1. **main.cpp starts**
   - Creates QApplication
   - Creates Dialog window
   - Shows Dialog: `g_mainDlg->show()`
   - Enters event loop: `a.exec()`

2. **User clicks "Farm View" button**
   - Calls `Dialog::on_farmViewBtn_clicked()` (dialog.cpp:554-562)
   - Gets FarmViewer singleton: `FarmViewer& viewer = FarmViewer::instance()`
   - **FarmViewer constructor runs for first time**

3. **FarmViewer constructor initialization:**
   - Line 55: `setupUI()` - creates entire widget hierarchy
   - Lines 58-64: Sets window flags and attributes
   - Line 63: `setWindowFlags(Qt::Window | ...)`  - **Converts widget to top-level window**
   - This may trigger a window recreation or state change

4. **After constructor returns, showFarmViewer() is called:**
   - farmviewer.cpp:694-753
   - Line 698: `show()`
   - Line 700: `raise()`
   - Line 702: `activateWindow()`
   - Lines 706-737: Loads already-connected devices and triggers auto-detection
   - **This is the FIRST time FarmViewer widget is shown**

5. **Qt Window State Issue:**
   - When `setWindowFlags()` is called (line 63), it causes Qt to **recreate the window internally**
   - Setting window flags on a widget that hasn't been shown yet can cause:
     - Platform window creation
     - Geometry recalculation
     - Event queue processing

6. **Potential Close/Reopen Sequence:**
   - **HYPOTHESIS 1 - Event Loop Processing:**
     During the constructor, multiple deferred operations are queued:
     - Device detection ADB setup (line 79-100)
     - Periodic timer setup (lines 104-113)
     - Signal connections (lines 118-199)
     - Socket notifier setup (lines 203-210)
     - Deferred aboutToQuit connection (lines 212-216)

     These could cause the event loop to process window events, potentially triggering a close/reopen cycle.

   - **HYPOTHESIS 2 - Window Flags Interaction:**
     The explicit window flags set at line 63:
     ```cpp
     setWindowFlags(Qt::Window | Qt::WindowTitleHint |
                   Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
     ```
     Combined with immediate geometry changes (move, resize) might cause platform-specific window manager interactions that result in spurious close events.

   - **HYPOTHESIS 3 - Deferred Connections with Event Loop:**
     Line 212-216 defers the aboutToQuit connection. If the application event loop processes this deferred slot while the window is in a semi-initialized state, it could trigger unexpected window events.

---

## 5. RECENT CODE CHANGES (Git Diff Analysis)

### Modified Files:
- `QtScrcpy/ui/farmviewer.cpp` - 103 changes
- `QtScrcpy/ui/farmviewer.h` - 5 changes
- `install-nixos.sh` - not relevant to bug

### Key Changes in farmviewer.cpp:

**1. Window Flags Addition (NEW - Lines 62-64):**
```cpp
setWindowFlags(Qt::Window | Qt::WindowTitleHint |
              Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
setAttribute(Qt::WA_DeleteOnClose, false);
```
**Status:** NEW CODE - Likely introduced to fix a different window issue
**Risk Level:** HIGH - Explicit window flag manipulation during initialization

**2. Periodic Device Detection Timer (NEW - Lines 104-113):**
```cpp
m_deviceDetectionTimer = new QTimer(this);
m_deviceDetectionTimer->setInterval(5000);
connect(m_deviceDetectionTimer, &QTimer::timeout, ...);
m_deviceDetectionTimer->start();
```
**Status:** NEW CODE - Enables periodic polling
**Risk Level:** MEDIUM - Timer could fire during window initialization

**3. showEvent Handler (NEW - Lines 257-262):**
```cpp
void FarmViewer::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // Auto-detection is handled in showFarmViewer(), not here
}
```
**Status:** NEW CODE - Placeholder implementation
**Risk Level:** LOW - Does nothing special, but marks awareness of showEvent

**4. Auto-Detection Trigger Guard (NEW - Lines 740-741, 154 in .h):**
```cpp
bool m_autoDetectionTriggered = false;

if (connectedSerials.isEmpty() && !m_autoDetectionTriggered) {
    m_autoDetectionTriggered = true;
    // ... trigger auto-detection
}
```
**Status:** NEW CODE - Prevents duplicate auto-detection
**Risk Level:** LOW - Properly guards against re-triggering

**5. Device Filtering in processDetectedDevices (MODIFIED - Lines 809-820):**
```cpp
QStringList newDevices;
for (const QString& serial : devices) {
    if (!m_deviceForms.contains(serial)) {
        newDevices.append(serial);
    }
}
if (newDevices.isEmpty()) {
    qDebug() << "FarmViewer: No new devices detected, all devices already displayed";
    return;  // All devices already in grid, nothing to do
}
```
**Status:** MODIFIED - Now filters for NEW devices instead of processing all
**Risk Level:** MEDIUM - Logic change could affect state management

---

## 6. SUSPICIOUS PATTERNS IDENTIFIED

### Pattern 1: Multiple Deferred Operations During Construction

**Location:** farmviewer.cpp Constructor (lines 28-224)

The constructor queues multiple operations to the event loop:
1. Device detection ADB setup with lambda callbacks
2. Periodic timer with lambda callbacks
3. Signal connections with lambda callbacks
4. Deferred aboutToQuit connection (line 213)

**Risk:** Heavy lambda capture and deferred operations could cause unexpected event processing.

### Pattern 2: Window Flag Manipulation Before Show

**Location:** farmviewer.cpp:62-74 (NEW CODE)

```cpp
setWindowFlags(Qt::Window | Qt::WindowTitleHint |
              Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
setAttribute(Qt::WA_DeleteOnClose, false);
```

Then immediately:
```cpp
setWindowTitle("QtScrcpy Farm Viewer");
setMinimumSize(800, 600);
resize(1200, 900);
move(x, y);  // Center on screen
```

**Risk:** `setWindowFlags()` internally hides and recreates the window. Multiple geometry operations immediately after could confuse the window manager.

### Pattern 3: Device Detection Timer Starting in Constructor

**Location:** farmviewer.cpp:113 (NEW CODE)

The periodic device detection timer is started in the constructor, before the window is shown. If this timer fires before the window is displayed, it could queue events that interact with the uninitialized window.

### Pattern 4: Singleton Pattern with Complex Initialization

**Location:** farmviewer.cpp:264-270

The singleton is created on-demand when `instance()` is called. The entire complex initialization happens synchronously, which is unusual for Qt widgets that typically defer heavy operations.

---

## 7. TIMELINE OF EVENTS DURING FIRST LAUNCH

```
T0:   User clicks "Farm View" button on Dialog
      └─> Dialog::on_farmViewBtn_clicked() called (dialog.cpp:554)

T1:   FarmViewer::instance() called
      └─> FarmViewer constructor starts (farmviewer.cpp:28)

T2:   setupUI() called (line 55)
      └─> Complete widget hierarchy created

T3:   setWindowFlags() called (line 63)
      └─> [CRITICAL] Qt internally processes window state change
      └─> Window may be hidden/recreated at platform level

T4:   Window geometry operations (lines 66-74)
      └─> setWindowTitle()
      └─> setMinimumSize()
      └─> resize()
      └─> move() to center on screen

T5:   Device detection ADB connection setup (lines 79-100)
      └─> Lambda callbacks registered
      └─> Capture FarmViewer 'this' pointer

T6:   Device detection timer created and started (lines 104-113)
      └─> Timer::singleShot(5000) will fire in 5 seconds
      └─> May cause event processing

T7:   IDeviceManage signal connections (lines 118-199)
      └─> Multiple lambda callbacks registered
      └─> Each captures 'this' and state variables

T8:   Socket notifier setup (lines 203-210)
      └─> Unix signal handling socket pair created
      └─> Notifier watches for Unix signals

T9:   Deferred aboutToQuit connection (lines 212-216)
      └─> QTimer::singleShot(0) queued
      └─> Connects cleanup handler
      └─> WILL EXECUTE IN NEXT EVENT LOOP ITERATION

T10:  Constructor returns (line 223)
      └─> Control returns to Dialog::on_farmViewBtn_clicked()

T11:  showFarmViewer() called (dialog.cpp:560)
      └─> FarmViewer::showFarmViewer() (farmviewer.cpp:694)

T12:  show() called (line 698)
      └─> Window displayed for FIRST TIME

T13:  raise() and activateWindow() (lines 700-702)
      └─> Window brought to foreground

T14:  Auto-detection triggered (lines 747-749)
      └─> QTimer::singleShot(0) queues autoDetectAndConnectDevices()

T15:  showFarmViewer() returns
      └─> Control returns to on_farmViewBtn_clicked()

T16:  EVENT LOOP PROCESSES DEFERRED OPERATIONS:
      ├─> aboutToQuit connection executes (from T9)
      ├─> autoDetectAndConnectDevices() executes (from T14)
      ├─> Timer fires (from T6)
      └─> Various signal callbacks fire

??? UNKNOWN: At some point, a close event occurs and window closes/reopens
```

---

## 8. SUSPECTED ROOT CAUSE

**Most Likely Cause: Window Flag Manipulation Triggering Spurious Close Event**

### Mechanism:
1. `setWindowFlags()` call on line 63 causes Qt to recreate the platform window
2. Window recreation combined with immediate geometry changes (lines 66-74) may cause the window manager to send a close or state-change event
3. If a close event is generated during construction, it could close the window before `show()` is called
4. The `show()` call (line 698) in `showFarmViewer()` reopens the window, making it appear as "open, close, reopen"

### Why This Pattern Causes Issues:
- **Timing:** Window flags are set BEFORE window is shown - Qt hasn't created the platform window yet
- **State Conflict:** Following immediately with geometry operations forces re-layout
- **Event Processing:** The event loop has pending operations from the constructor that interact with window state changes
- **Deferred Operations:** The `QTimer::singleShot(0)` calls defer operations that may process window events during initialization

### Why It Works on Subsequent Launches:
- The singleton already exists
- Constructor doesn't run again
- No window flag manipulation
- Window is already in correct state

---

## 9. EVIDENCE SUPPORTING THIS DIAGNOSIS

1. **Code Pattern:** Window flags explicitly set in constructor is NEW code (not in previous working versions)
2. **Timing:** Bug only occurs on FIRST launch when constructor runs
3. **Singleton Pattern:** Subsequent launches don't recreate FarmViewer, so bug doesn't recur
4. **Qt Semantics:** `setWindowFlags()` documentation explicitly states it may hide the window
5. **Widget State:** Window is not shown when flags are set (shown later by `show()` call)
6. **Event Loop:** Multiple `QTimer::singleShot(0)` operations pending during construction

---

## 10. PREVENTION AND REMEDIATION

### Short-term Fix (Prevent Constructor Issues):
1. Remove or defer `setWindowFlags()` call from constructor
2. Call `setWindowFlags()` AFTER window is shown, or not at all
3. Delay device detection timer start until after `show()`
4. Process all deferred operations before `show()` is called

### Long-term Fix (Architectural):
1. Defer complex initialization from constructor to `showEvent()`
2. Use two-phase initialization: construction creates empty widget, initialization populates it
3. Separate window setup from device detection setup
4. Consider async initialization pattern instead of synchronous lambda chains

### Code-level Changes Needed:
1. **farmviewer.cpp Line 63:** Remove or comment out `setWindowFlags()` call
2. **farmviewer.cpp Lines 104-113:** Move timer start to `showFarmViewer()` or deferred slot
3. **farmviewer.cpp Lines 212-216:** Move aboutToQuit connection to `showEvent()` or `showFarmViewer()`
4. **farmviewer.cpp Line 257-262:** Implement `showEvent()` to perform deferred initialization

---

## 11. SPECIFIC FILE LOCATIONS FOR INVESTIGATION

### Primary Investigation Points:

| File | Line | Issue | Severity |
|------|------|-------|----------|
| farmviewer.cpp | 63-64 | setWindowFlags() before show() | CRITICAL |
| farmviewer.cpp | 113 | Timer starts in constructor | HIGH |
| farmviewer.cpp | 212-216 | Deferred aboutToQuit connection | HIGH |
| farmviewer.h | 154 | m_autoDetectionTriggered added | MEDIUM |
| farmviewer.cpp | 257-262 | showEvent() placeholder | MEDIUM |
| dialog.cpp | 141 | WA_DeleteOnClose on Dialog | MEDIUM |
| dialog.cpp | 303-314 | closeEvent ignores close | MEDIUM |

### Supporting Investigation:

| File | Line | Context |
|------|------|---------|
| farmviewer.cpp | 28-224 | Full constructor |
| farmviewer.cpp | 264-270 | Singleton pattern |
| farmviewer.cpp | 694-753 | showFarmViewer() |
| farmviewer.cpp | 779-800 | autoDetectAndConnectDevices() |
| main.cpp | 160-163 | Signal setup and Dialog creation |

---

## 12. TESTING APPROACH

### To Reproduce the Bug:
1. Clean build of farm-manager
2. Launch application
3. Dialog appears
4. Click "Farm View" button
5. Observe window behavior:
   - **Expected:** FarmViewer window opens and stays open
   - **Actual:** FarmViewer appears briefly, closes, then reopens

### To Debug:
1. Add logging at FarmViewer constructor entry/exit
2. Add logging before/after `setWindowFlags()` call
3. Add logging in `closeEvent()` for both Dialog and FarmViewer
4. Monitor Qt event queue during construction:
   ```cpp
   qDebug() << "Pending events before show():" << QApplication::pendingPostEventCount();
   ```
5. Compare window state before/after each major operation
6. Use `strace` or platform debugger to monitor X11/Wayland window events

### Expected Results After Fix:
1. FarmViewer window appears once and remains open
2. No spurious close events in logs
3. Subsequent launches continue to work correctly
4. Device auto-detection works as expected

---

## Summary Table

| Aspect | Finding |
|--------|---------|
| **Root Cause** | Window flag manipulation during construction before show() |
| **Trigger** | New code added: `setWindowFlags()` on line 63 |
| **Why First Launch Only** | Singleton constructor only runs on first access |
| **Why Works After** | Singleton already exists, constructor doesn't run again |
| **Critical Code** | farmviewer.cpp:63-74 (window flags + geometry) |
| **Related Issues** | farmviewer.cpp:113 (timer), farmviewer.cpp:212-216 (deferred connection) |
| **Severity** | HIGH - Breaks basic functionality on first use |
| **Reproducibility** | 100% - Every first launch exhibits bug |
| **Impact** | UX degradation, user confusion, potential signal loss during close/reopen |

