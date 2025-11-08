# Qt Window Initialization Best Practices - Reference Guide

**Created**: 2025-11-08
**Context**: FarmViewer window bug fix
**Reference**: Qt 6 Documentation + Industry Best Practices

---

## Summary

This guide documents Qt best practices for window initialization, based on the FarmViewer bug fix. The key principle: **Window customization must happen at the right time in the widget lifecycle**, or it can trigger unexpected behavior.

---

## The Problem We Solved

### The Bug Pattern
```cpp
class MyWindow : public QWidget {
public:
    MyWindow() : QWidget() {
        // Constructor setup
        setupUI();

        // PROBLEM: Called before show()
        setWindowFlags(Qt::Window | Qt::WindowTitleHint | ...);

        // Result: Window will hide and be recreated when show() is called later
        // User sees: open -> close -> open (flicker/glitch)
    }
};

int main() {
    MyWindow window;
    window.show();  // Platform window recreation happens HERE
                    // But setWindowFlags already scheduled hide from earlier
    return app.exec();
}
```

### Why This Pattern Fails

Qt's `setWindowFlags()` implementation:
1. **Hides the widget** - `hide()` is called internally
2. **Removes the platform window** - `winId()` is invalidated
3. **Schedules window recreation** - Qt defers platform window creation
4. **Reapplies all attributes** - Qt resets decorations and state

When called BEFORE `show()`:
- Widget isn't visible yet, so hide() doesn't appear to do anything
- But Qt internally flags the widget as needing recreation
- When `show()` is called later, Qt sees the deferred hide and recreates the window
- This recreation is visible as a flicker/brief close-reopen cycle

---

## Qt Widget Lifecycle

Understanding when to call window customization methods requires knowing the Qt widget lifecycle:

### Phase 1: Construction
```cpp
MyWindow::MyWindow() : QWidget(parent) {
    // Phase 1: Widget properties
    setWindowTitle("My Window");
    resize(800, 600);

    // Phase 1: NOT here - window doesn't exist yet
    // setWindowFlags(...);  // WRONG - causes issues
    // setAttribute(Qt::WA_DeleteOnClose, true);  // OK here

    // Phase 1: UI setup
    setupUI();
}
```

**What exists**: Qt widget object, properties, but NO platform window yet

### Phase 2: Initial Show
```cpp
int main() {
    MyWindow window;
    window.show();  // <-- Platform window is created HERE for the first time
}
```

**What happens**:
1. Qt detects `show()` call
2. Qt creates platform window (if not already created)
3. Qt applies window flags from construction
4. Qt makes window visible at OS level

### Phase 3: Show Events
```cpp
void MyWindow::showEvent(QShowEvent *event) {
    // Called when widget is about to be shown
    // Platform window EXISTS at this point
    // Safe to call any window customization here

    QWidget::showEvent(event);  // Call parent implementation
}
```

**What exists**: Platform window is ready, widget is visible or about to be visible

### Phase 4: Runtime (After Show)
```cpp
void MyWindow::onSomeUserAction() {
    // Long after show(), can safely modify window state
    setWindowTitle("Updated Title");

    // Can use setWindowFlag() for individual flags
    setWindowFlag(Qt::WindowStaysOnTopHint, true);
    show();  // Must call show() again after setWindowFlag()
}
```

**What exists**: Platform window is fully established, can be safely modified

---

## Correct Patterns

### Pattern 1: Set Flags at Construction (BEST)
```cpp
class MyWindow : public QWidget {
public:
    MyWindow() : QWidget(nullptr, Qt::Window | Qt::WindowTitleHint) {
        // Flags are set at construction time
        // Qt will apply them when platform window is created
        setWindowTitle("My Window");
        resize(800, 600);
        setupUI();
    }
};
```

**Why it works**:
- Flags are passed to QWidget constructor
- Qt applies them when platform window is created
- No intermediate hide/recreation happens
- Simple and clear

**Best for**: Setting flags once at startup (most common case)

### Pattern 2: Customize in showEvent() (GOOD)
```cpp
class MyWindow : public QWidget {
protected:
    void showEvent(QShowEvent *event) override {
        static bool firstShow = true;
        if (firstShow) {
            firstShow = false;

            // Safe to customize here - platform window exists
            setWindowFlags(Qt::Window | Qt::WindowTitleHint | ...);

            // Or set individual flags
            setWindowFlag(Qt::WindowStaysOnTopHint, true);

            qInfo() << "Window customized on first show";
        }

        QWidget::showEvent(event);
    }
};
```

**Why it works**:
- `showEvent()` is called when platform window is ready
- Can safely modify window state
- Deferred execution pattern (first show only)
- Clear intent (customization happens at show time)

**Best for**: Conditional customization based on runtime state

### Pattern 3: Modify After Show (LESS COMMON)
```cpp
void MyWindow::updateWindowState() {
    // Already visible, now change a flag

    setWindowFlag(Qt::WindowStaysOnTopHint, true);
    show();  // MUST call show() again after setWindowFlag()
}
```

**Why it works**:
- `setWindowFlag()` (singular) allows modifying individual flags
- Doesn't hide the widget like `setWindowFlags()` (plural)
- Must call `show()` again to apply changes
- Platform window recreation happens, but user doesn't see it (window already visible)

**Best for**: Runtime flag changes, toggling specific behaviors

### Pattern 4: Constructor with Parameters (MOST EXPLICIT)
```cpp
class MyWindow : public QWidget {
public:
    explicit MyWindow(Qt::WindowFlags flags = Qt::Window)
        : QWidget(nullptr, flags) {
        setWindowTitle("My Window");
        resize(800, 600);
        setupUI();
    }
};

int main() {
    // Caller specifies flags at construction
    MyWindow window(Qt::Window | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    window.show();
    return app.exec();
}
```

**Why it works**:
- Flags passed to QWidget constructor
- Caller controls window appearance
- Explicit and clear
- Maximum flexibility

**Best for**: Reusable components where different instances may need different flags

---

## Anti-Patterns (What NOT To Do)

### Anti-Pattern 1: setWindowFlags() Before Show() ❌
```cpp
class MyWindow : public QWidget {
public:
    MyWindow() {
        setupUI();

        // WRONG - causes window recreation
        setWindowFlags(Qt::Window | Qt::WindowTitleHint | ...);

        // Result: When show() is called later, platform window
        // is hidden and recreated, causing visible flicker
    }
};

int main() {
    MyWindow w;
    w.show();  // Window appears, disappears, reappears (flicker!)
}
```

**Why it fails**:
- `setWindowFlags()` internally hides the widget
- Called before widget is visible, so hide is deferred
- When `show()` is called, deferred hide is processed
- User sees close/reopen sequence

**Real-world consequence**: The FarmViewer bug (first-launch flicker)

### Anti-Pattern 2: setWindowFlags() in Slot Handler ❌
```cpp
void MyWindow::onButtonClicked() {
    // Already visible, modifying flags
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);  // WRONG
    // Result: Window briefly disappears and reappears
}
```

**Why it fails**:
- Window is visible when flags change
- Platform window recreation happens
- Causes visible flicker to user
- Confusing UX

**Solution**: Use `setWindowFlag()` (singular) instead:
```cpp
void MyWindow::onButtonClicked() {
    setWindowFlag(Qt::FramelessWindowHint, true);  // Better
    show();  // Must call show() to apply
}
```

### Anti-Pattern 3: Checking isVisible() During Construction ❌
```cpp
MyWindow::MyWindow() {
    setupUI();

    if (isVisible()) {  // WRONG - always false in constructor!
        setWindowFlags(...);
    }
    // isVisible() is always false until show() is called
}
```

**Why it fails**:
- Widget not visible during construction
- Condition never true
- Dead code pattern

**Solution**: Use `showEvent()` override instead

### Anti-Pattern 4: Multiple setWindowFlags() Calls ❌
```cpp
MyWindow::MyWindow() {
    setWindowFlags(Qt::Window);
    // More UI setup
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);  // WRONG
    // Multiple calls = multiple recreations
}
```

**Why it fails**:
- Each call triggers potential recreation
- Inefficient and error-prone
- Multiple recreations cause issues

**Solution**: Set all flags in one call
```cpp
MyWindow::MyWindow() {
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | ...);
}
```

---

## Quick Reference Table

| Scenario | Method | Timing | Risk | Example |
|----------|--------|--------|------|---------|
| Set flags once at startup | Constructor parameter | Construction | None | `MyWindow(parent, Qt::Window)` |
| Set flags in constructor | `setWindowFlags()` in constructor | Before show() | None | OK if always done |
| Customize on first display | `showEvent()` override | First show | Low | Static bool firstShow |
| Change single flag later | `setWindowFlag()` (singular) | After show | Low | `setWindowFlag(Qt::WindowTopHint, true)` |
| Change multiple flags later | `setWindowFlags()` (plural) | After show | Medium | Window will briefly hide/reappear |
| Check visibility | `isVisible()` | Runtime only | Medium | Don't use in constructor |

---

## FarmViewer Case Study

### The Bug
```cpp
// farmviewer.cpp, lines 62-64 (ORIGINAL - BUGGY)
setWindowFlags(Qt::Window | Qt::WindowTitleHint |
              Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
setAttribute(Qt::WA_DeleteOnClose, false);
```

**Problem**: Called in constructor before `show()`, causing platform window recreation when `show()` is later called.

### The Fix
```cpp
// farmviewer.cpp, lines 62-70 (FIXED)
// NOTE: setWindowFlags() is NOT called here because calling it on an unshown widget
// causes Qt to hide the window and reconstruct the platform window, triggering spurious
// close events. The widget is created as a top-level window by Qt automatically, so
// explicit flag setting is unnecessary and causes the first-launch open-close-reopen bug.
```

**Solution**: Remove the problematic calls. Qt creates top-level windows automatically with correct defaults.

### Why This Works
1. FarmViewer inherits from QWidget with no parent
2. Qt automatically creates it as top-level window
3. Qt provides all standard window decorations by default
4. No special flag setting needed
5. No platform window recreation = no flicker

---

## References

### Qt Documentation
- QWidget class: https://doc.qt.io/qt-6/qwidget.html
- Window Flags: https://doc.qt.io/qt-6/qt.html#WindowType-enum
- showEvent(): https://doc.qt.io/qt-6/qwidget.html#showEvent
- setWindowFlags(): https://doc.qt.io/qt-6/qwidget.html#setWindowFlags

### Key Qt Design Principles
1. **Constructor for Setup**: Set properties that don't need platform window
2. **showEvent() for Runtime**: Customize when window exists
3. **Minimal Flags**: Only set flags you actually need
4. **Pass to Constructor**: Preferred over calling setWindowFlags()

### Industry Best Practices
- Set window properties once at construction
- Avoid modifying window state after show()
- If must modify, use setWindowFlag() (singular) not setWindowFlags()
- Document any window customization with "why" comments
- Test window behavior on different platforms (Windows, macOS, Linux)

---

## Testing Your Window Implementation

### Test Checklist
- [ ] Window appears on first show (no flicker)
- [ ] Window decorations present (title bar, buttons)
- [ ] Window can be closed by user
- [ ] Window can be resized (if enabled)
- [ ] Window position/size correct
- [ ] Multiple show/hide cycles work
- [ ] Changing flags after show works without flicker
- [ ] Test on Windows, macOS, and Linux if possible

### Debug Commands
```cpp
// Log window state
qInfo() << "Window visible:" << isVisible();
qInfo() << "Window flags:" << windowFlags();
qInfo() << "Window geometry:" << geometry();
qInfo() << "Is top-level:" << isWindow();

// Trace setWindowFlags calls
qDebug() << "setWindowFlags called with:" << flags;
```

---

## Conclusion

Window initialization in Qt requires understanding the widget lifecycle and when platform resources are available. The key principle:

**Set window flags early (in constructor or pass to constructor), or wait until showEvent() to customize. Never call setWindowFlags() before show() is called.**

Following these patterns ensures smooth window creation without visible glitches or unexpected behavior.

---

**Document Status**: Reference Guide
**Last Updated**: 2025-11-08
**Related**: FarmViewer bug fix, Qt 6 best practices
