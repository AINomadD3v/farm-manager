# Potential Fixes for Open-Close-Reopen Bug

## Overview
This document presents several candidate fixes for the FarmViewer window open-close-reopen bug. The most likely fixes are presented first, with increasing complexity.

---

## Fix Candidate 1: Remove setWindowFlags() from Constructor (RECOMMENDED - SIMPLEST)

**Confidence Level:** Very High
**Complexity:** Low
**Risk:** Very Low
**Estimated Fix Time:** 2 minutes

### The Problem
Lines 62-64 in `farmviewer.cpp` call `setWindowFlags()` before the window is shown, causing Qt to recreate the platform window, which triggers spurious close events.

### Solution
Remove or comment out the problematic lines.

### Code Change

**File:** `/home/aidev/tools/farm-manager/QtScrcpy/ui/farmviewer.cpp`
**Lines:** 62-64

**BEFORE:**
```cpp
    resize(1200, 900);

    // Set explicit window flags to prevent Qt from treating as transient window
    setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
    setAttribute(Qt::WA_DeleteOnClose, false); // Prevent accidental deletion of singleton

    qInfo() << "FarmViewer: Centering window on screen...";
```

**AFTER - Option A (Remove entirely):**
```cpp
    resize(1200, 900);

    qInfo() << "FarmViewer: Centering window on screen...";
```

**AFTER - Option B (Comment out with explanation):**
```cpp
    resize(1200, 900);

    // NOTE: Window flags are NOT set here as setWindowFlags() on an unshown widget
    // causes Qt to recreate the platform window, triggering spurious close events.
    // The window will be properly created as a top-level window by Qt automatically.
    // setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
    // setAttribute(Qt::WA_DeleteOnClose, false); // Prevent accidental deletion of singleton

    qInfo() << "FarmViewer: Centering window on screen...";
```

### Testing the Fix
```bash
cd /home/aidev/tools/farm-manager
# Make the change above

# Rebuild
rm -rf build && mkdir build && cd build
cmake .. && make -j4

# Run and test
./QtScrcpy 2>&1 | tee test.log

# Check that window appears correctly
# Click "Farm View" button
# Observe: Window should appear once and stay visible
# Expected: No close/reopen cycle
```

### Validation Criteria
✓ Window opens and stays open on first launch
✓ Window closes correctly when user clicks close button
✓ Subsequent launches continue to work
✓ No window state errors in logs
✓ Device detection works as expected

### Rollback Plan
If this breaks window appearance:
```bash
git checkout QtScrcpy/ui/farmviewer.cpp
```

---

## Fix Candidate 2: Defer Window Customization to showEvent()

**Confidence Level:** High
**Complexity:** Medium
**Risk:** Low
**Estimated Fix Time:** 15 minutes

### The Problem
Window customization happens in the constructor before the window exists. Instead, it should happen when the window is first shown.

### Solution
Move window flag and attribute setting to the `showEvent()` override, which is called when the window is first shown.

### Code Changes

**File:** `/home/aidev/tools/farm-manager/QtScrcpy/ui/farmviewer.cpp`
**Lines:** 62-64 (remove) and 257-262 (modify)

**BEFORE - Constructor:**
```cpp
    resize(1200, 900);

    // Set explicit window flags to prevent Qt from treating as transient window
    setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
    setAttribute(Qt::WA_DeleteOnClose, false); // Prevent accidental deletion of singleton

    qInfo() << "FarmViewer: Centering window on screen...";
```

**AFTER - Constructor:**
```cpp
    resize(1200, 900);

    qInfo() << "FarmViewer: Centering window on screen...";
```

**BEFORE - showEvent():**
```cpp
void FarmViewer::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // Auto-detection is handled in showFarmViewer(), not here
    // This prevents window recreation issues during first show
}
```

**AFTER - showEvent():**
```cpp
void FarmViewer::showEvent(QShowEvent *event)
{
    // Only customize on first show
    static bool firstShow = true;
    if (firstShow) {
        firstShow = false;
        qInfo() << "FarmViewer::showEvent() - Customizing window on first show";

        // Now it's safe to set window flags (window exists and is being shown)
        setWindowFlags(Qt::Window | Qt::WindowTitleHint |
                      Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
        setAttribute(Qt::WA_DeleteOnClose, false); // Prevent accidental deletion of singleton

        qInfo() << "FarmViewer::showEvent() - Window customization applied";
    }

    QWidget::showEvent(event);
    // Auto-detection is handled in showFarmViewer(), not here
    // This prevents window recreation issues during first show
}
```

### Why This Works
- `showEvent()` is called when the platform window is ready
- Window flags can be safely modified at this point
- No spurious close events since the window already exists

### Testing the Fix
Same as Fix Candidate 1.

### Validation Criteria
Same as Fix Candidate 1.

---

## Fix Candidate 3: Move Timer Start to showFarmViewer()

**Confidence Level:** Medium
**Complexity:** Low
**Risk:** Low
**Estimated Fix Time:** 5 minutes

### The Problem
Device detection timer is started in the constructor before the window is shown. This can cause the timer to fire and queue device detection while the window is in an indeterminate state.

### Solution
Move the timer start to `showFarmViewer()` after the window is shown.

### Code Changes

**File:** `/home/aidev/tools/farm-manager/QtScrcpy/ui/farmviewer.cpp`
**Lines:** 104-113 (modify) and 694-753 (modify)

**BEFORE - Constructor:**
```cpp
    // Periodic device polling (matches Dialog's m_autoUpdatetimer behavior)
    m_deviceDetectionTimer = new QTimer(this);
    m_deviceDetectionTimer->setInterval(5000);  // Poll every 5 seconds
    connect(m_deviceDetectionTimer, &QTimer::timeout, this, [this]() {
        // Only poll if visible, not connecting, and ADB not already running
        if (this->isVisible() && !m_isConnecting && !m_deviceDetectionAdb.isRuning()) {
            qDebug() << "FarmViewer: Auto-polling for new devices...";
            m_deviceDetectionAdb.execute("", QStringList() << "devices");
        }
    });
    m_deviceDetectionTimer->start();
    qInfo() << "FarmViewer: Periodic device detection enabled (5s interval)";
```

**AFTER - Constructor:**
```cpp
    // Periodic device polling (matches Dialog's m_autoUpdatetimer behavior)
    m_deviceDetectionTimer = new QTimer(this);
    m_deviceDetectionTimer->setInterval(5000);  // Poll every 5 seconds
    connect(m_deviceDetectionTimer, &QTimer::timeout, this, [this]() {
        // Only poll if visible, not connecting, and ADB not already running
        if (this->isVisible() && !m_isConnecting && !m_deviceDetectionAdb.isRuning()) {
            qDebug() << "FarmViewer: Auto-polling for new devices...";
            m_deviceDetectionAdb.execute("", QStringList() << "devices");
        }
    });
    // NOTE: Timer is started in showFarmViewer() after window is shown
    qInfo() << "FarmViewer: Periodic device detection configured (will start when window shows)";
```

**BEFORE - showFarmViewer():**
```cpp
void FarmViewer::showFarmViewer()
{
    qInfo() << "FarmViewer: showFarmViewer() called";
    qInfo() << "FarmViewer: Calling show()...";
    show();
    qInfo() << "FarmViewer: show() completed, calling raise()...";
    raise();
    qInfo() << "FarmViewer: raise() completed, calling activateWindow()...";
    activateWindow();
    qInfo() << "FarmViewer: activateWindow() completed";

    // Load already-connected devices from IDeviceManage
```

**AFTER - showFarmViewer():**
```cpp
void FarmViewer::showFarmViewer()
{
    qInfo() << "FarmViewer: showFarmViewer() called";
    qInfo() << "FarmViewer: Calling show()...";
    show();
    qInfo() << "FarmViewer: show() completed, calling raise()...";
    raise();
    qInfo() << "FarmViewer: raise() completed, calling activateWindow()...";
    activateWindow();
    qInfo() << "FarmViewer: activateWindow() completed";

    // Start device detection timer now that window is shown
    if (m_deviceDetectionTimer && !m_deviceDetectionTimer->isActive()) {
        qInfo() << "FarmViewer: Starting periodic device detection timer...";
        m_deviceDetectionTimer->start();
    }

    // Load already-connected devices from IDeviceManage
```

### Testing the Fix
Same as Fix Candidate 1.

### Note
This can be combined with Fix Candidate 1 or 2 for better reliability.

---

## Fix Candidate 4: Move Deferred Connection to showFarmViewer()

**Confidence Level:** Medium
**Complexity:** Low
**Risk:** Low
**Estimated Fix Time:** 5 minutes

### The Problem
The aboutToQuit connection is deferred to the next event loop iteration before the window is shown. This can interact poorly with window state changes.

### Solution
Move the connection to happen after the window is shown, in `showFarmViewer()`.

### Code Changes

**File:** `/home/aidev/tools/farm-manager/QtScrcpy/ui/farmviewer.cpp`
**Lines:** 212-216 (remove) and 694-753 (modify)

**BEFORE - Constructor:**
```cpp
    // Connect to application aboutToQuit to ensure cleanup - deferred to avoid blocking
    QTimer::singleShot(0, this, [this]() {
        connect(qApp, &QApplication::aboutToQuit, this, &FarmViewer::cleanupAndExit);
        qInfo() << "FarmViewer: Cleanup handler connected";
    });
```

**AFTER - Constructor:**
```cpp
    // Connect to application aboutToQuit - moved to showFarmViewer() to avoid
    // deferred execution during window initialization
    // qApp cleanup will be connected when window is shown
```

**BEFORE - showFarmViewer():**
```cpp
void FarmViewer::showFarmViewer()
{
    qInfo() << "FarmViewer: showFarmViewer() called";
    // ... show operations ...
    activateWindow();
    qInfo() << "FarmViewer: activateWindow() completed";

    // Load already-connected devices...
```

**AFTER - showFarmViewer():**
```cpp
void FarmViewer::showFarmViewer()
{
    qInfo() << "FarmViewer: showFarmViewer() called";
    // ... show operations ...
    activateWindow();
    qInfo() << "FarmViewer: activateWindow() completed";

    // Connect to application aboutToQuit for cleanup (deferred from constructor)
    static bool cleanupConnected = false;
    if (!cleanupConnected) {
        cleanupConnected = true;
        connect(qApp, &QApplication::aboutToQuit, this, &FarmViewer::cleanupAndExit);
        qInfo() << "FarmViewer: Cleanup handler connected";
    }

    // Load already-connected devices...
```

### Testing the Fix
Same as Fix Candidate 1.

### Note
This can be combined with Fix Candidate 1, 2, or 3 for comprehensive fix.

---

## Fix Candidate 5: Comprehensive - All Three Changes

**Confidence Level:** Very High
**Complexity:** Medium
**Risk:** Very Low
**Estimated Fix Time:** 20 minutes

### Approach
Apply Candidates 1, 2, and 4 together for a comprehensive, robust fix that eliminates all potential initialization order issues.

### Summary of Changes

1. **Remove setWindowFlags from constructor**
2. **Move timer start to showFarmViewer()**
3. **Move cleanup connection to showFarmViewer()**

### Benefits
- Eliminates multiple sources of potential issues
- Makes initialization order explicit and clear
- Ensures all window customization happens after window is ready
- Ensures all deferred operations happen after window is shown

### Testing the Fix
```bash
cd /home/aidev/tools/farm-manager

# Apply all changes from Candidates 1, 3, and 4

# Rebuild
rm -rf build && mkdir build && cd build
cmake .. && make -j4

# Run comprehensive test
./QtScrcpy 2>&1 | tee comprehensive_test.log

# Verify:
# 1. Window opens normally on first launch
# 2. "Farm View" click shows window correctly
# 3. Device detection starts and works
# 4. Application shutdown is clean
# 5. Subsequent launches work correctly
```

### Validation Checklist
✓ FarmViewer window appears once and stays visible on first launch
✓ No close/reopen cycle observed
✓ Device detection works (periodic polling works)
✓ Cleanup handler is properly connected
✓ Application exits cleanly
✓ Dialog tray icon works correctly
✓ No window state errors in logs
✓ Multiple launches produce consistent behavior
✓ Device connections and streaming work as expected

---

## Comparison Table

| Candidate | Complexity | Risk | Effect | Recommended? |
|-----------|------------|------|--------|--------------|
| 1 - Remove setWindowFlags | Very Low | Very Low | Directly fixes main bug | **YES** |
| 2 - Move to showEvent | Medium | Low | Proper initialization order | Yes (if 1 breaks something) |
| 3 - Move timer start | Low | Low | Prevents premature timer fire | Yes (combining) |
| 4 - Move cleanup connection | Low | Low | Prevents deferred issues | Yes (combining) |
| 5 - All together | Medium | Very Low | Most robust | **BEST** |

---

## Recommended Fix Path

### Step 1: Quick Fix (Try First)
- Apply Fix Candidate 1 (remove setWindowFlags)
- Test to see if bug is gone
- If yes, commit and done
- If no, proceed to Step 2

### Step 2: Comprehensive Fix (If Step 1 Doesn't Work)
- Apply Fix Candidates 1, 3, and 4 together
- Test all scenarios
- Commit with comprehensive explanation

### Step 3: Fallback (If Symptoms Persist)
- Consider deeper refactoring of FarmViewer initialization
- Possibly implement proper async initialization pattern
- Consult with Qt expertise if available

---

## Code Snippets Ready to Apply

### Quick Patch (Candidate 1)
```bash
# Comment out problematic lines
sed -i '62,64s/^/\/\/ /' /home/aidev/tools/farm-manager/QtScrcpy/ui/farmviewer.cpp
```

### Verify Changes
```bash
# View the changes
git diff /home/aidev/tools/farm-manager/QtScrcpy/ui/farmviewer.cpp | head -30
```

### Revert if Needed
```bash
# Revert all changes
git checkout /home/aidev/tools/farm-manager/QtScrcpy/ui/farmviewer.cpp
```

---

## Post-Fix Validation

### Required Tests
1. First launch - "Farm View" button
2. Window appearance and state
3. Device detection starts
4. Device connections work
5. Application shutdown
6. Multiple sequential launches
7. Tray icon interaction

### Expected Results
- Window appears once, stays visible
- No flashing or state changes
- Clean logs with no error messages
- Consistent behavior across launches
- All features work as designed

---

## Next Steps for Fix Implementation

1. **Select** the appropriate fix candidate (Candidate 1 recommended first)
2. **Apply** the code changes
3. **Test** following the validation criteria
4. **Commit** with detailed explanation of the root cause
5. **Document** the fix in the code comments
6. **Run** full test suite to ensure no regressions
7. **Close** the bug ticket

---

**Document Created:** 2025-11-08
**Status:** Ready for Implementation
