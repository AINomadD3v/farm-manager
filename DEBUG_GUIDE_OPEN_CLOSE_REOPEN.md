# Debug Guide: Open-Close-Reopen Bug Investigation

## Quick Reference

**Bug:** Application opens FarmViewer window, immediately closes it, then reopens it correctly.

**Affected Code:**
- File: `/home/aidev/tools/farm-manager/QtScrcpy/ui/farmviewer.cpp`
- File: `/home/aidev/tools/farm-manager/QtScrcpy/ui/farmviewer.h`
- File: `/home/aidev/tools/farm-manager/QtScrcpy/ui/dialog.cpp`

**Key Suspect Lines:**
- `farmviewer.cpp:63-64` - `setWindowFlags()` before show()
- `farmviewer.cpp:113` - Device detection timer started in constructor
- `farmviewer.cpp:212-216` - Deferred aboutToQuit connection

---

## Debugging Checklist

### 1. Verify Window State Changes

**What to Monitor:**
- When window is created
- When window is shown
- When window is closed
- When window is recreated

**Add Logging Points:**

```cpp
// farmviewer.cpp - Add to constructor after line 53

qInfo() << "FarmViewer: Constructor START";
qInfo() << "  Current state: visible=" << isVisible() << ", windowState=" << windowState();
```

```cpp
// farmviewer.cpp - Add after line 63

qInfo() << "FarmViewer: After setWindowFlags()";
qInfo() << "  Current state: visible=" << isVisible() << ", windowState=" << windowState();
qInfo() << "  Window ID: " << winId();
```

```cpp
// farmviewer.cpp - Add to showEvent() - line 257

void FarmViewer::showEvent(QShowEvent *event)
{
    qInfo() << "========================================";
    qInfo() << "FarmViewer::showEvent() CALLED";
    qInfo() << "  Visible: " << isVisible();
    qInfo() << "  Window State: " << windowState();
    qInfo() << "========================================";
    QWidget::showEvent(event);
}
```

**Add closeEvent() Handler:**

```cpp
// farmviewer.cpp - Add this function

void FarmViewer::closeEvent(QCloseEvent *event)
{
    qInfo() << "========================================";
    qInfo() << "FarmViewer::closeEvent() CALLED - THIS IS THE BUG!";
    qInfo() << "  Visible: " << isVisible();
    qInfo() << "  Window State: " << windowState();
    qInfo() << "========================================";

    // Don't accept the close - we want to see if this happens
    event->ignore();
}
```

### 2. Monitor Constructor Event Processing

**Location:** `farmviewer.cpp` Constructor (lines 28-224)

```cpp
// Add at start of constructor
qDebug() << "FarmViewer constructor: Starting event queue depth:"
         << QApplication::eventDispatcher()->registeredTimers(this).size();

// Add before each major operation
qDebug() << "Before setupUI: pending events:"
         << QApplication::pendingPostEventCount();

qDebug() << "Before setWindowFlags: pending events:"
         << QApplication::pendingPostEventCount();

qDebug() << "Before ADB setup: pending events:"
         << QApplication::pendingPostEventCount();

qDebug() << "Before timer setup: pending events:"
         << QApplication::pendingPostEventCount();

qDebug() << "Before IDeviceManage signals: pending events:"
         << QApplication::pendingPostEventCount();

qDebug() << "Before socket setup: pending events:"
         << QApplication::pendingPostEventCount();

qDebug() << "Before aboutToQuit connection: pending events:"
         << QApplication::pendingPostEventCount();

// Add at end of constructor
qDebug() << "FarmViewer constructor: Ending event queue depth:"
         << QApplication::eventDispatcher()->registeredTimers(this).size();
```

### 3. Check Window Flag Application Order

**Current Code (lines 62-74):**

```cpp
// Set explicit window flags to prevent Qt from treating as transient window
setWindowFlags(Qt::Window | Qt::WindowTitleHint |
              Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
setAttribute(Qt::WA_DeleteOnClose, false); // Prevent accidental deletion of singleton

qInfo() << "FarmViewer: Centering window on screen...";
// Center window on screen
QScreen *screen = QApplication::primaryScreen();
```

**Investigation:**
- Add before `setWindowFlags()`: `qInfo() << "Before setWindowFlags: winId=" << winId() << "visible=" << isVisible();`
- Add after `setWindowFlags()`: `qInfo() << "After setWindowFlags: winId=" << winId() << "visible=" << isVisible();`
- Add after move(): `qInfo() << "After move: winId=" << winId() << "visible=" << isVisible();`

### 4. Device Detection Timer

**Current Code (lines 104-113):**

```cpp
m_deviceDetectionTimer = new QTimer(this);
m_deviceDetectionTimer->setInterval(5000);  // Poll every 5 seconds
connect(m_deviceDetectionTimer, &QTimer::timeout, this, [this]() {
    if (this->isVisible() && !m_isConnecting && !m_deviceDetectionAdb.isRuning()) {
        qDebug() << "FarmViewer: Auto-polling for new devices...";
        m_deviceDetectionAdb.execute("", QStringList() << "devices");
    }
});
m_deviceDetectionTimer->start();  // STARTS HERE - BEFORE show()
```

**Problem:** Timer starts before window is shown. Timer could fire during window initialization.

**Add Logging:**
```cpp
qInfo() << "FarmViewer: Timer starting";
qDebug() << "  isVisible()=" << isVisible();
qDebug() << "  Pending events=" << QApplication::pendingPostEventCount();
m_deviceDetectionTimer->start();
qInfo() << "FarmViewer: Timer started";
```

### 5. Deferred Connection

**Current Code (lines 212-216):**

```cpp
QTimer::singleShot(0, this, [this]() {
    connect(qApp, &QApplication::aboutToQuit, this, &FarmViewer::cleanupAndExit);
    qInfo() << "FarmViewer: Cleanup handler connected";
});
```

**Problem:** This defers the connection to "next event loop iteration". If window state changes occur in the meantime, this could cause issues.

**Add Logging:**
```cpp
qInfo() << "FarmViewer: Scheduling deferred aboutToQuit connection";
qInfo() << "  Current event count=" << QApplication::pendingPostEventCount();
QTimer::singleShot(0, this, [this]() {
    qInfo() << "FarmViewer: Deferred connection now executing";
    qInfo() << "  isVisible()=" << isVisible();
    connect(qApp, &QApplication::aboutToQuit, this, &FarmViewer::cleanupAndExit);
    qInfo() << "FarmViewer: Cleanup handler connected";
});
```

### 6. showFarmViewer() Sequence

**Location:** `farmviewer.cpp` lines 694-753

```cpp
void FarmViewer::showFarmViewer()
{
    qInfo() << "========================================";
    qInfo() << "FarmViewer: showFarmViewer() called";
    qInfo() << "  isVisible()=" << isVisible();
    qInfo() << "  windowState()=" << windowState();
    qInfo() << "  Pending events=" << QApplication::pendingPostEventCount();
    qInfo() << "========================================";

    qInfo() << "FarmViewer: Calling show()...";
    show();
    qInfo() << "FarmViewer: After show(), isVisible=" << isVisible();

    qInfo() << "FarmViewer: Calling raise()...";
    raise();

    qInfo() << "FarmViewer: Calling activateWindow()...";
    activateWindow();

    // ... rest of function
}
```

---

## Step-by-Step Reproduction with Debug Output

### Scenario 1: Verify the Bug Exists

1. Clean build:
   ```bash
   cd /home/aidev/tools/farm-manager
   rm -rf build
   mkdir build
   cd build
   cmake ..
   make -j4
   ```

2. Run with logging:
   ```bash
   ./QtScrcpy 2>&1 | tee farm_debug.log
   ```

3. Click "Farm View" button

4. Watch output for:
   - Window creation: `FarmViewer: Constructor START`
   - Window flags: `FarmViewer: After setWindowFlags()`
   - Show call: `FarmViewer: Calling show()`
   - Any `closeEvent()` output (indicates close happened)
   - `FarmViewer: After show(), isVisible=` (should be true)

5. Expected buggy behavior:
   - After `Calling show()`, window briefly appears
   - Then window disappears
   - Then window reappears

### Scenario 2: Check Window State Timeline

Expected logging sequence:
```
FarmViewer: Constructor START
  isVisible()=false

FarmViewer: After setWindowFlags()
  isVisible()=false

... various setup ...

FarmViewer: showFarmViewer() called
  isVisible()=false

FarmViewer: Calling show()...
FarmViewer: After show(), isVisible=true
```

**If bug occurs, you'll see:**
```
FarmViewer: Calling show()...
FarmViewer::closeEvent() CALLED - THIS IS THE BUG!    <-- UNEXPECTED!
FarmViewer: After show(), isVisible=false              <-- WRONG STATE!
```

Then the window reopens (not shown in logs).

### Scenario 3: Capture Full Event Trace

```bash
# Run with full event logging
./QtScrcpy 2>&1 | grep -E "(Constructor|setWindowFlags|show|closeEvent|Calling)" | tee trace.log
```

This filters to just the key lifecycle events.

---

## Root Cause Hypothesis Validation

### Hypothesis 1: setWindowFlags() Causes Platform Window Recreation

**Test:**
1. Comment out lines 62-64 in farmviewer.cpp:
   ```cpp
   // setWindowFlags(Qt::Window | Qt::WindowTitleHint |
   //               Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
   // setAttribute(Qt::WA_DeleteOnClose, false);
   ```

2. Rebuild and test

3. **Expected Result if TRUE:**
   - Bug disappears (window doesn't close/reopen)
   - Window still opens normally
   - Device detection still works

4. **Expected Result if FALSE:**
   - Bug persists (window still closes/reopens)
   - Need to investigate other causes

### Hypothesis 2: Device Detection Timer Interferes

**Test:**
1. Comment out lines 113 in farmviewer.cpp:
   ```cpp
   // m_deviceDetectionTimer->start();
   ```

2. Rebuild and test

3. **Expected Result if TRUE:**
   - Bug disappears or is reduced
   - Manual device detection still works (5-second delay)

4. **Expected Result if FALSE:**
   - Bug persists
   - Timer is not the cause

### Hypothesis 3: Deferred Connection Interferes

**Test:**
1. Comment out lines 212-216 in farmviewer.cpp:
   ```cpp
   // QTimer::singleShot(0, this, [this]() {
   //     connect(qApp, &QApplication::aboutToQuit, this, &FarmViewer::cleanupAndExit);
   //     qInfo() << "FarmViewer: Cleanup handler connected";
   // });
   ```

2. Move the connection to after show():
   ```cpp
   // In showFarmViewer(), after show() call:
   connect(qApp, &QApplication::aboutToQuit, this, &FarmViewer::cleanupAndExit);
   ```

3. Rebuild and test

4. **Expected Result if TRUE:**
   - Bug disappears
   - Cleanup still works

5. **Expected Result if FALSE:**
   - Bug persists
   - Deferred connection is not the primary cause

---

## Platform-Specific Debugging

### On X11:
```bash
# Monitor X11 window events
# Run in separate terminal while app is running
xev -id $(xdotool search --name "QtScrcpy Farm Viewer" getwindowfocus)
```

Look for:
- CreateNotify - window created
- DestroyNotify - window destroyed (shouldn't happen)
- MapNotify - window shown
- UnmapNotify - window hidden (watch for unexpected ones)
- ConfigureNotify - window resized/moved

### On Wayland:
```bash
# Wayland doesn't have xev, but check logs
journalctl -e | grep -i "farm\|qtscrcpy"
```

### With Qt Debugging:
```bash
export QT_DEBUG_PLUGINS=1
export QT_QPA_PLATFORM_PLUGIN_PATH=/path/to/qt/plugins
./QtScrcpy 2>&1 | grep -E "(QPA|platform|window)"
```

---

## Key Files and Line Numbers for Reference

```
/home/aidev/tools/farm-manager/QtScrcpy/ui/farmviewer.cpp
  Line 28-224:  FarmViewer constructor
  Line 62-64:   setWindowFlags() - CRITICAL
  Line 113:     Timer start - HIGH PRIORITY
  Line 212-216: Deferred connection - HIGH PRIORITY
  Line 257-262: showEvent() handler
  Line 694-753: showFarmViewer()

/home/aidev/tools/farm-manager/QtScrcpy/ui/farmviewer.h
  Line 154:     m_autoDetectionTriggered flag
  Line 150:     m_deviceDetectionTimer
  Line 78:      showEvent() declaration (NEW)

/home/aidev/tools/farm-manager/QtScrcpy/ui/dialog.cpp
  Line 141:     WA_DeleteOnClose on Dialog
  Line 303-314: Dialog::closeEvent()
  Line 554-562: on_farmViewBtn_clicked() - calls FarmViewer::instance()

/home/aidev/tools/farm-manager/QtScrcpy/main.cpp
  Line 162-163: Dialog creation and show
```

---

## What to Look For in Logs

### Good Signs (No Bug):
- `Constructor START` immediately followed by `showFarmViewer() called`
- `Calling show()...` followed immediately by `After show(), isVisible=true`
- Window appears once and stays visible
- No `closeEvent()` output before the window is intentionally closed

### Bad Signs (Bug Present):
- `Calling show()...` followed by `closeEvent() CALLED`
- `After show(), isVisible=false`
- Window appearing/disappearing rapidly
- Multiple `closeEvent()` calls during initialization

---

## Isolating Which Change Introduced the Bug

The recent changes to farmviewer.cpp can be isolated:

1. **Check git log:**
   ```bash
   cd /home/aidev/tools/farm-manager
   git log --oneline QtScrcpy/ui/farmviewer.cpp | head -5
   ```

2. **Get specific commit:**
   ```bash
   git show <commit_hash>:QtScrcpy/ui/farmviewer.cpp > /tmp/old_farmviewer.cpp
   ```

3. **Compare key lines:**
   ```bash
   diff -u /tmp/old_farmviewer.cpp QtScrcpy/ui/farmviewer.cpp | grep -A5 -B5 "setWindowFlags\|Timer\|singleShot"
   ```

---

## Summary

The open-close-reopen bug most likely stems from:

1. **setWindowFlags() called before show()** - Qt window recreation
2. **Deferred operations during construction** - Event loop interaction
3. **Timer started before show()** - Premature timeout execution

**Most probable cause:** Window flag manipulation causing platform window recreation, which triggers spurious close event during construction.

**Next steps:** Apply logging from Section 1-5 above and rerun to confirm exact sequence of events.

