// #include <QDesktopWidget>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QSemaphore>
#include <QShortcut>
#include <QStyle>
#include <QStyleOption>
#include <QThread>
#include <QTimer>
#include <QWindow>
#include <QtWidgets/QHBoxLayout>

#if defined(Q_OS_WIN32)
#include <Windows.h>
#endif

#include "config.h"
#include "iconhelper.h"
#include "qyuvopenglwidget.h"
#include "toolform.h"
#include "mousetap/mousetap.h"
#include "ui_videoform.h"
#include "videoform.h"

// Global semaphore to serialize OpenGL widget creation across all devices
// This prevents GPU driver crash when many devices try to create OpenGL contexts simultaneously
// PHASE 1: Increased from 5 to 20 concurrent OpenGL context creations
// Modern GPUs can handle 20 concurrent contexts safely (supports 200+ devices with batching)
static QSemaphore g_openglCreationSemaphore(20);

VideoForm::VideoForm(bool framelessWindow, bool skin, bool showToolbar, QWidget *parent) : QWidget(parent), ui(new Ui::videoForm), m_skin(skin)
{
    ui->setupUi(this);
    initUI();
    installShortcut();
    updateShowSize(size());
    bool vertical = size().height() > size().width();
    this->show_toolbar = showToolbar;
    if (m_skin) {
        updateStyleSheet(vertical);
    }
    if (framelessWindow) {
        setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    }
}

VideoForm::~VideoForm()
{
    delete ui;
}

void VideoForm::initUI()
{
    if (m_skin) {
        QPixmap phone;
        if (phone.load(":/res/phone.png")) {
            m_widthHeightRatio = 1.0f * phone.width() / phone.height();
        }

#ifndef Q_OS_OSX
        // mac下去掉标题栏影响showfullscreen
        // 去掉标题栏
        setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
        // 根据图片构造异形窗口
        setAttribute(Qt::WA_TranslucentBackground);
#endif
    }

    // DON'T create QYUVOpenGLWidget here - will be created on-demand when first frame arrives
    // This prevents GPU resource exhaustion when showing 78+ device tiles
    m_videoWidget = nullptr;

    ui->keepRatioWidget->setWidthHeightRatio(m_widthHeightRatio);

    // Simple styling - just a border and background for empty state
    ui->keepRatioWidget->setStyleSheet(R"(
        QWidget {
            background-color: #2d3436;
            border: 2px solid #74b9ff;
            border-radius: 4px;
        }
    )");

    // Create simple footer label for device info
    m_footerLabel = new QLabel(ui->keepRatioWidget);
    m_footerLabel->setStyleSheet(R"(
        QLabel {
            background-color: rgba(0, 0, 0, 180);
            color: #dfe6e9;
            border: none;
            padding: 5px;
            font-size: 10px;
        }
    )");
    m_footerLabel->setAlignment(Qt::AlignCenter);
    m_footerLabel->setText("Device");
    m_footerLabel->setGeometry(0, ui->keepRatioWidget->height() - 30, ui->keepRatioWidget->width(), 30);
    // CRITICAL: Allow mouse events to pass through to video widget below
    m_footerLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    // Install event filter to handle resizing
    ui->keepRatioWidget->installEventFilter(this);

    // Set cursor to pointing hand for clickable tiles
    setCursor(Qt::PointingHandCursor);

    setMouseTracking(true);
    ui->keepRatioWidget->setMouseTracking(true);
}

void VideoForm::createVideoWidget()
{
    // Only create once
    if (m_videoWidget) {
        return;
    }

    qInfo() << "========================================";
    qInfo() << "VideoForm::createVideoWidget() - ACQUIRING SEMAPHORE for:" << m_serial;
    qInfo() << "  Available permits:" << g_openglCreationSemaphore.available();

    // CRITICAL: Acquire semaphore before creating OpenGL widget
    // This serializes OpenGL context creation to prevent GPU driver crash
    // when many devices try to create contexts simultaneously (supports 200+ devices)
    g_openglCreationSemaphore.acquire();

    qInfo() << "VideoForm::createVideoWidget() - SEMAPHORE ACQUIRED, creating OpenGL widget for:" << m_serial;

    try {
        // Create the OpenGL video widget
        qInfo() << "VideoForm::createVideoWidget() - About to call QYUVOpenGLWidget constructor...";
        qInfo() << "  Parent widget:" << (void*)this;
        m_videoWidget = new QYUVOpenGLWidget(this);
        qInfo() << "VideoForm::createVideoWidget() - QYUVOpenGLWidget constructor returned:" << (void*)m_videoWidget;

        qInfo() << "VideoForm::createVideoWidget() - About to call setWidget()...";
        ui->keepRatioWidget->setWidget(m_videoWidget);
        qInfo() << "VideoForm::createVideoWidget() - setWidget() completed";

        // Create FPS label as child of video widget
        m_fpsLabel = new QLabel(m_videoWidget);
        QFont ft;
        ft.setPointSize(15);
        ft.setWeight(QFont::Light);
        ft.setBold(true);
        m_fpsLabel->setFont(ft);
        m_fpsLabel->move(5, 15);
        m_fpsLabel->setMinimumWidth(100);
        m_fpsLabel->setStyleSheet(R"(QLabel {color: #00FF00;})");
        m_fpsLabel->hide(); // Hidden by default

        // Show the video widget
        m_videoWidget->show();
        m_videoWidget->setMouseTracking(true);

        qInfo() << "VideoForm::createVideoWidget() - OpenGL widget created successfully for:" << m_serial;
        qInfo() << "========================================";

        // CRITICAL: Release semaphore after successful creation
        g_openglCreationSemaphore.release();
        qInfo() << "VideoForm::createVideoWidget() - SEMAPHORE RELEASED";

    } catch (const std::exception& e) {
        qCritical() << "VideoForm::createVideoWidget() - EXCEPTION:" << e.what() << "for:" << m_serial;
        // CRITICAL: Always release semaphore even on exception
        g_openglCreationSemaphore.release();
        throw;
    } catch (...) {
        qCritical() << "VideoForm::createVideoWidget() - UNKNOWN EXCEPTION for:" << m_serial;
        // CRITICAL: Always release semaphore even on exception
        g_openglCreationSemaphore.release();
        throw;
    }
}

bool VideoForm::eventFilter(QObject *watched, QEvent *event)
{
    // Update footer label position when keepRatioWidget is resized
    if (event->type() == QEvent::Resize && watched == ui->keepRatioWidget) {
        if (m_footerLabel) {
            m_footerLabel->setGeometry(0, ui->keepRatioWidget->height() - 30, ui->keepRatioWidget->width(), 30);
        }
    }
    return QWidget::eventFilter(watched, event);
}

QRect VideoForm::getGrabCursorRect()
{
    QRect rc;
    if (!m_videoWidget) {
        return rc; // Return empty rect if video widget not created yet
    }
#if defined(Q_OS_WIN32)
    rc = QRect(ui->keepRatioWidget->mapToGlobal(m_videoWidget->pos()), m_videoWidget->size());
    // high dpi support
    rc.setTopLeft(rc.topLeft() * m_videoWidget->devicePixelRatioF());
    rc.setBottomRight(rc.bottomRight() * m_videoWidget->devicePixelRatioF());

    rc.setX(rc.x() + 10);
    rc.setY(rc.y() + 10);
    rc.setWidth(rc.width() - 20);
    rc.setHeight(rc.height() - 20);
#elif defined(Q_OS_OSX)
    rc = m_videoWidget->geometry();
    rc.setTopLeft(ui->keepRatioWidget->mapToGlobal(rc.topLeft()));
    rc.setBottomRight(ui->keepRatioWidget->mapToGlobal(rc.bottomRight()));

    rc.setX(rc.x() + 10);
    rc.setY(rc.y() + 10);
    rc.setWidth(rc.width() - 20);
    rc.setHeight(rc.height() - 20);
#elif defined(Q_OS_LINUX)
    rc = QRect(ui->keepRatioWidget->mapToGlobal(m_videoWidget->pos()), m_videoWidget->size());
    // high dpi support -- taken from the WIN32 section and untested
    rc.setTopLeft(rc.topLeft() * m_videoWidget->devicePixelRatioF());
    rc.setBottomRight(rc.bottomRight() * m_videoWidget->devicePixelRatioF());

    rc.setX(rc.x() + 10);
    rc.setY(rc.y() + 10);
    rc.setWidth(rc.width() - 20);
    rc.setHeight(rc.height() - 20);
#endif
    return rc;
}

const QSize &VideoForm::frameSize()
{
    return m_frameSize;
}

void VideoForm::resizeSquare()
{
    QRect screenRect = getScreenRect();
    if (screenRect.isEmpty()) {
        qWarning() << "getScreenRect is empty";
        return;
    }
    resize(screenRect.height(), screenRect.height());
}

void VideoForm::removeBlackRect()
{
    resize(ui->keepRatioWidget->goodSize());
}

void VideoForm::showFPS(bool show)
{
    if (!m_fpsLabel) {
        return;
    }
    m_fpsLabel->setVisible(show);
}

void VideoForm::updateRender(int width, int height, uint8_t* dataY, uint8_t* dataU, uint8_t* dataV, int linesizeY, int linesizeU, int linesizeV)
{
    qInfo() << "VideoForm::updateRender() ENTRY - Serial:" << m_serial << "Size:" << width << "x" << height;

    // Create OpenGL video widget on-demand when first frame arrives
    // This prevents GPU resource exhaustion when showing 78+ device tiles
    if (!m_videoWidget) {
        qInfo() << "========================================";
        qInfo() << "VideoForm::updateRender() - FIRST FRAME RECEIVED!";
        qInfo() << "  Serial:" << m_serial;
        qInfo() << "  Frame size:" << width << "x" << height;
        qInfo() << "  Creating OpenGL widget on-demand...";
        qInfo() << "========================================";

        try {
            createVideoWidget();
            qInfo() << "VideoForm::updateRender() - createVideoWidget() returned successfully";
        } catch (const std::exception& e) {
            qCritical() << "VideoForm::updateRender() - EXCEPTION in createVideoWidget():" << e.what();
            return;
        } catch (...) {
            qCritical() << "VideoForm::updateRender() - UNKNOWN EXCEPTION in createVideoWidget()";
            return;
        }
    }

    // Safety check - should never happen but be defensive
    if (!m_videoWidget) {
        qWarning() << "VideoForm::updateRender() - Failed to create video widget for:" << m_serial;
        return;
    }

    qInfo() << "VideoForm::updateRender() - Video widget exists, proceeding with rendering...";

    if (m_videoWidget->isHidden()) {
        qInfo() << "VideoForm: Video widget was hidden, showing it now for:" << m_serial;
        if (m_loadingWidget) {
            m_loadingWidget->close();
        }
        m_videoWidget->show();
    }

    updateShowSize(QSize(width, height));

    qInfo() << "VideoForm::updateRender() - About to call setFrameSize and updateTextures...";
    m_videoWidget->setFrameSize(QSize(width, height));
    qInfo() << "VideoForm::updateRender() - Calling updateTextures()...";
    m_videoWidget->updateTextures(dataY, dataU, dataV, linesizeY, linesizeU, linesizeV);
    qInfo() << "VideoForm::updateRender() - updateTextures() completed, EXIT";
}

void VideoForm::setSerial(const QString &serial)
{
    m_serial = serial;

    // Update footer label with serial number
    if (m_footerLabel) {
        m_footerLabel->setText(serial);
    }
}

void VideoForm::updatePlaceholderStatus(const QString& status, const QString& backgroundColor)
{
    // Update footer label with status
    if (m_footerLabel) {
        m_footerLabel->setText(m_serial + " - " + status);
    }

    // Update border color based on state
    if (!backgroundColor.isEmpty() && ui->keepRatioWidget) {
        QString borderColor = "#74b9ff";  // Default blue
        if (backgroundColor == "connecting") {
            borderColor = "#fdcb6e";  // Orange for connecting
        } else if (backgroundColor == "streaming") {
            borderColor = "#00b894";  // Green for streaming
        }

        ui->keepRatioWidget->setStyleSheet(QString(R"(
            QWidget {
                background-color: #2d3436;
                border: 2px solid %1;
                border-radius: 4px;
            }
        )").arg(borderColor));
    }
}

void VideoForm::showToolForm(bool show)
{
    if (!m_toolForm) {
        m_toolForm = new ToolForm(this, ToolForm::AP_OUTSIDE_RIGHT);
        m_toolForm->setSerial(m_serial);
    }
    m_toolForm->move(pos().x() + geometry().width(), pos().y() + 30);
    m_toolForm->setVisible(show);
}

void VideoForm::moveCenter()
{
    QRect screenRect = getScreenRect();
    if (screenRect.isEmpty()) {
        qWarning() << "getScreenRect is empty";
        return;
    }
    // 窗口居中
    move(screenRect.center() - QRect(0, 0, size().width(), size().height()).center());
}

void VideoForm::installShortcut()
{
    QShortcut *shortcut = nullptr;

    // switchFullScreen
    shortcut = new QShortcut(QKeySequence("Ctrl+f"), this);
    shortcut->setAutoRepeat(false);
    connect(shortcut, &QShortcut::activated, this, [this]() {
        auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
        if (!device) {
            return;
        }
        switchFullScreen();
    });

    // resizeSquare
    shortcut = new QShortcut(QKeySequence("Ctrl+g"), this);
    shortcut->setAutoRepeat(false);
    connect(shortcut, &QShortcut::activated, this, [this]() { resizeSquare(); });

    // removeBlackRect
    shortcut = new QShortcut(QKeySequence("Ctrl+w"), this);
    shortcut->setAutoRepeat(false);
    connect(shortcut, &QShortcut::activated, this, [this]() { removeBlackRect(); });

    // postGoHome
    shortcut = new QShortcut(QKeySequence("Ctrl+h"), this);
    shortcut->setAutoRepeat(false);
    connect(shortcut, &QShortcut::activated, this, [this]() {
        auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
        if (!device) {
            return;
        }
        device->postGoHome();
    });

    // postGoBack
    shortcut = new QShortcut(QKeySequence("Ctrl+b"), this);
    shortcut->setAutoRepeat(false);
    connect(shortcut, &QShortcut::activated, this, [this]() {
        auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
        if (!device) {
            return;
        }
        device->postGoBack();
    });

    // postAppSwitch
    shortcut = new QShortcut(QKeySequence("Ctrl+s"), this);
    shortcut->setAutoRepeat(false);
    connect(shortcut, &QShortcut::activated, this, [this]() {
        auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
        if (!device) {
            return;
        }
        emit device->postAppSwitch();
    });

    // postGoMenu
    shortcut = new QShortcut(QKeySequence("Ctrl+m"), this);
    shortcut->setAutoRepeat(false);
    connect(shortcut, &QShortcut::activated, this, [this]() {
        auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
        if (!device) {
            return;
        }
        device->postGoMenu();
    });

    // postVolumeUp
    shortcut = new QShortcut(QKeySequence("Ctrl+up"), this);
    connect(shortcut, &QShortcut::activated, this, [this]() {
        auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
        if (!device) {
            return;
        }
        emit device->postVolumeUp();
    });

    // postVolumeDown
    shortcut = new QShortcut(QKeySequence("Ctrl+down"), this);
    connect(shortcut, &QShortcut::activated, this, [this]() {
        auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
        if (!device) {
            return;
        }
        emit device->postVolumeDown();
    });

    // postPower
    shortcut = new QShortcut(QKeySequence("Ctrl+p"), this);
    shortcut->setAutoRepeat(false);
    connect(shortcut, &QShortcut::activated, this, [this]() {
        auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
        if (!device) {
            return;
        }
        emit device->postPower();
    });

    shortcut = new QShortcut(QKeySequence("Ctrl+o"), this);
    shortcut->setAutoRepeat(false);
    connect(shortcut, &QShortcut::activated, this, [this]() {
        auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
        if (!device) {
            return;
        }
        emit device->setDisplayPower(false);
    });

    // expandNotificationPanel
    shortcut = new QShortcut(QKeySequence("Ctrl+n"), this);
    shortcut->setAutoRepeat(false);
    connect(shortcut, &QShortcut::activated, this, [this]() {
        auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
        if (!device) {
            return;
        }
        emit device->expandNotificationPanel();
    });

    // collapsePanel
    shortcut = new QShortcut(QKeySequence("Ctrl+Shift+n"), this);
    shortcut->setAutoRepeat(false);
    connect(shortcut, &QShortcut::activated, this, [this]() {
        auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
        if (!device) {
            return;
        }
        emit device->collapsePanel();
    });

    // copy
    shortcut = new QShortcut(QKeySequence("Ctrl+c"), this);
    shortcut->setAutoRepeat(false);
    connect(shortcut, &QShortcut::activated, this, [this]() {
        auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
        if (!device) {
            return;
        }
        emit device->postCopy();
    });

    // cut
    shortcut = new QShortcut(QKeySequence("Ctrl+x"), this);
    shortcut->setAutoRepeat(false);
    connect(shortcut, &QShortcut::activated, this, [this]() {
        auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
        if (!device) {
            return;
        }
        emit device->postCut();
    });

    // clipboardPaste
    shortcut = new QShortcut(QKeySequence("Ctrl+v"), this);
    shortcut->setAutoRepeat(false);
    connect(shortcut, &QShortcut::activated, this, [this]() {
        auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
        if (!device) {
            return;
        }
        emit device->setDeviceClipboard();
    });

    // setDeviceClipboard
    shortcut = new QShortcut(QKeySequence("Ctrl+Shift+v"), this);
    shortcut->setAutoRepeat(false);
    connect(shortcut, &QShortcut::activated, this, [this]() {
        auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
        if (!device) {
            return;
        }
        emit device->clipboardPaste();
    });
}

QRect VideoForm::getScreenRect()
{
    QRect screenRect;
    QScreen *screen = QGuiApplication::primaryScreen();
    QWidget *win = window();
    if (win) {
        QWindow *winHandle = win->windowHandle();
        if (winHandle) {
            screen = winHandle->screen();
        }
    }

    if (screen) {
        screenRect = screen->availableGeometry();
    }
    return screenRect;
}

void VideoForm::updateStyleSheet(bool vertical)
{
    if (vertical) {
        setStyleSheet(R"(
                 #videoForm {
                     border-image: url(:/image/videoform/phone-v.png) 150px 65px 85px 65px;
                     border-width: 150px 65px 85px 65px;
                 }
                 )");
    } else {
        setStyleSheet(R"(
                 #videoForm {
                     border-image: url(:/image/videoform/phone-h.png) 65px 85px 65px 150px;
                     border-width: 65px 85px 65px 150px;
                 }
                 )");
    }
    layout()->setContentsMargins(getMargins(vertical));
}

QMargins VideoForm::getMargins(bool vertical)
{
    QMargins margins;
    if (vertical) {
        margins = QMargins(10, 68, 12, 62);
    } else {
        margins = QMargins(68, 12, 62, 10);
    }
    return margins;
}

void VideoForm::updateShowSize(const QSize &newSize)
{
    if (m_frameSize != newSize) {
        m_frameSize = newSize;

        m_widthHeightRatio = 1.0f * newSize.width() / newSize.height();
        ui->keepRatioWidget->setWidthHeightRatio(m_widthHeightRatio);

        bool vertical = m_widthHeightRatio < 1.0f ? true : false;
        QSize showSize = newSize;
        QRect screenRect = getScreenRect();
        if (screenRect.isEmpty()) {
            qWarning() << "getScreenRect is empty";
            return;
        }
        if (vertical) {
            showSize.setHeight(qMin(newSize.height(), screenRect.height() - 200));
            showSize.setWidth(showSize.height() * m_widthHeightRatio);
        } else {
            showSize.setWidth(qMin(newSize.width(), screenRect.width() / 2));
            showSize.setHeight(showSize.width() / m_widthHeightRatio);
        }

        if (isFullScreen() && qsc::IDeviceManage::getInstance().getDevice(m_serial)) {
            switchFullScreen();
        }

        if (isMaximized()) {
            showNormal();
        }

        if (m_skin) {
            QMargins m = getMargins(vertical);
            showSize.setWidth(showSize.width() + m.left() + m.right());
            showSize.setHeight(showSize.height() + m.top() + m.bottom());
        }

        if (showSize != size()) {
            resize(showSize);
            if (m_skin) {
                updateStyleSheet(vertical);
            }
            moveCenter();
        }
    }
}

void VideoForm::switchFullScreen()
{
    if (isFullScreen()) {
        // 横屏全屏铺满全屏，恢复时，恢复保持宽高比
        if (m_widthHeightRatio > 1.0f) {
            ui->keepRatioWidget->setWidthHeightRatio(m_widthHeightRatio);
        }

        showNormal();
        // back to normal size.
        resize(m_normalSize);
        // fullscreen window will move (0,0). qt bug?
        move(m_fullScreenBeforePos);

#ifdef Q_OS_OSX
        //setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
        //show();
#endif
        if (m_skin) {
            updateStyleSheet(m_frameSize.height() > m_frameSize.width());
        }
        showToolForm(this->show_toolbar);
#ifdef Q_OS_WIN32
        ::SetThreadExecutionState(ES_CONTINUOUS);
#endif
    } else {
        // 横屏全屏铺满全屏，不保持宽高比
        if (m_widthHeightRatio > 1.0f) {
            ui->keepRatioWidget->setWidthHeightRatio(-1.0f);
        }

        // record current size before fullscreen, it will be used to rollback size after exit fullscreen.
        m_normalSize = size();

        m_fullScreenBeforePos = pos();
        // 这种临时增加标题栏再全屏的方案会导致收不到mousemove事件，导致setmousetrack失效
        // mac fullscreen must show title bar
#ifdef Q_OS_OSX
        //setWindowFlags(windowFlags() & ~Qt::FramelessWindowHint);
#endif
        showToolForm(false);
        if (m_skin) {
            layout()->setContentsMargins(0, 0, 0, 0);
        }
        showFullScreen();

        // 全屏状态禁止电脑休眠、息屏
#ifdef Q_OS_WIN32
        ::SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
#endif
    }
}

bool VideoForm::isHost()
{
    if (!m_toolForm) {
        return false;
    }
    return m_toolForm->isHost();
}

void VideoForm::updateFPS(quint32 fps)
{
    //qDebug() << "FPS:" << fps;
    if (!m_fpsLabel) {
        return;
    }
    m_fpsLabel->setText(QString("FPS:%1").arg(fps));
}

void VideoForm::grabCursor(bool grab)
{
    QRect rc = getGrabCursorRect();
    MouseTap::getInstance()->enableMouseEventTap(rc, grab);
}

void VideoForm::onFrame(int width, int height, uint8_t *dataY, uint8_t *dataU, uint8_t *dataV, int linesizeY, int linesizeU, int linesizeV)
{
    // CRITICAL: This may be called from Demuxer thread due to DirectConnection in decoder
    // OpenGL widgets MUST be created/updated only in main GUI thread
    // Check if we're in the correct thread

    if (QThread::currentThread() != thread()) {
        // We're in background thread (Demuxer thread) - copy data and dispatch to main GUI thread
        // CRITICAL: Use QueuedConnection instead of BlockingQueuedConnection to avoid deadlocks
        // When user interacts with device (mouse clicks), main thread processes those events
        // If Demuxer thread is blocked waiting for main thread (BlockingQueuedConnection),
        // and main thread is busy processing mouse events -> DEADLOCK

        // Copy YUV frame data (required since we're using non-blocking QueuedConnection)
        int sizeY = linesizeY * height;
        int sizeU = linesizeU * (height / 2);
        int sizeV = linesizeV * (height / 2);

        QByteArray copiedY((const char*)dataY, sizeY);
        QByteArray copiedU((const char*)dataU, sizeU);
        QByteArray copiedV((const char*)dataV, sizeV);

        QMetaObject::invokeMethod(this, [=]() {
            updateRender(width, height,
                        (uint8_t*)copiedY.data(), (uint8_t*)copiedU.data(), (uint8_t*)copiedV.data(),
                        linesizeY, linesizeU, linesizeV);
        }, Qt::QueuedConnection);
    } else {
        // Already in main GUI thread - call directly
        updateRender(width, height, dataY, dataU, dataV, linesizeY, linesizeU, linesizeV);
    }
}

void VideoForm::staysOnTop(bool top)
{
    bool needShow = false;
    if (isVisible()) {
        needShow = true;
    }
    setWindowFlag(Qt::WindowStaysOnTopHint, top);
    if (m_toolForm) {
        m_toolForm->setWindowFlag(Qt::WindowStaysOnTopHint, top);
    }
    if (needShow) {
        show();
    }
}

void VideoForm::mousePressEvent(QMouseEvent *event)
{
    qDebug() << "VideoForm::mousePressEvent() - Serial:" << m_serial << "Button:" << event->button();
    qDebug() << "  Position:" << event->pos();
    qDebug() << "  Video widget exists:" << (m_videoWidget != nullptr);
    if (m_videoWidget) {
        qDebug() << "  Video widget geometry:" << m_videoWidget->geometry();
        qDebug() << "  Contains point:" << m_videoWidget->geometry().contains(event->pos());
    }

    auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
    qDebug() << "  Device exists:" << (device != nullptr);

    // Check if click is on empty area (no video widget) - emit deviceClicked signal
    if (event->button() == Qt::LeftButton && !m_videoWidget) {
        qDebug() << "  -> Emitting deviceClicked (no video widget)";
        emit deviceClicked(m_serial);
        event->accept();
        return;
    }

    if (event->button() == Qt::MiddleButton) {
        if (device && !device->isCurrentCustomKeymap()) {
            device->postGoHome();
            return;
        }
    }

    if (event->button() == Qt::RightButton) {
        if (device && !device->isCurrentCustomKeymap()) {
            device->postGoBack();
            return;
        }
    }

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        QPointF localPos = event->localPos();
        QPointF globalPos = event->globalPos();
#else
        QPointF localPos = event->position();
        QPointF globalPos = event->globalPosition();
#endif

    if (m_videoWidget && m_videoWidget->geometry().contains(event->pos())) {
        if (!device) {
            qWarning() << "  -> Cannot forward mouse event: device is null!";
            return;
        }
        qDebug() << "  -> Forwarding mouse event to device";
        QPointF mappedPos = m_videoWidget->mapFrom(this, localPos.toPoint());
        QMouseEvent newEvent(event->type(), mappedPos, globalPos, event->button(), event->buttons(), event->modifiers());
        emit device->mouseEvent(&newEvent, m_videoWidget->frameSize(), m_videoWidget->size());
        qDebug() << "  -> Mouse event forwarded successfully";

        // debug keymap pos
        if (event->button() == Qt::LeftButton) {
            qreal x = localPos.x() / m_videoWidget->size().width();
            qreal y = localPos.y() / m_videoWidget->size().height();
            QString posTip = QString(R"("pos": {"x": %1, "y": %2})").arg(x).arg(y);
            qInfo() << posTip.toStdString().c_str();
        }
    } else {
        qDebug() << "  -> Mouse event outside video widget bounds";
        if (event->button() == Qt::LeftButton) {
            m_dragPosition = globalPos.toPoint() - frameGeometry().topLeft();
            event->accept();
        }
    }
}

void VideoForm::mouseReleaseEvent(QMouseEvent *event)
{
    auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
    if (m_dragPosition.isNull()) {
        if (!device || !m_videoWidget) {
            return;
        }
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        QPointF localPos = event->localPos();
        QPointF globalPos = event->globalPos();
#else
        QPointF localPos = event->position();
        QPointF globalPos = event->globalPosition();
#endif
        // local check
        QPointF local = m_videoWidget->mapFrom(this, localPos.toPoint());
        if (local.x() < 0) {
            local.setX(0);
        }
        if (local.x() > m_videoWidget->width()) {
            local.setX(m_videoWidget->width());
        }
        if (local.y() < 0) {
            local.setY(0);
        }
        if (local.y() > m_videoWidget->height()) {
            local.setY(m_videoWidget->height());
        }
        QMouseEvent newEvent(event->type(), local, globalPos, event->button(), event->buttons(), event->modifiers());
        emit device->mouseEvent(&newEvent, m_videoWidget->frameSize(), m_videoWidget->size());
    } else {
        m_dragPosition = QPoint(0, 0);
    }
}

void VideoForm::mouseMoveEvent(QMouseEvent *event)
{
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        QPointF localPos = event->localPos();
        QPointF globalPos = event->globalPos();
#else
        QPointF localPos = event->position();
        QPointF globalPos = event->globalPosition();
#endif
    auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
    if (m_videoWidget && m_videoWidget->geometry().contains(event->pos())) {
        if (!device) {
            return;
        }
        QPointF mappedPos = m_videoWidget->mapFrom(this, localPos.toPoint());
        QMouseEvent newEvent(event->type(), mappedPos, globalPos, event->button(), event->buttons(), event->modifiers());
        emit device->mouseEvent(&newEvent, m_videoWidget->frameSize(), m_videoWidget->size());
    } else if (!m_dragPosition.isNull()) {
        if (event->buttons() & Qt::LeftButton) {
            move(globalPos.toPoint() - m_dragPosition);
            event->accept();
        }
    }
}

void VideoForm::mouseDoubleClickEvent(QMouseEvent *event)
{
    auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
    if (event->button() == Qt::LeftButton && m_videoWidget && !m_videoWidget->geometry().contains(event->pos())) {
        if (!isMaximized()) {
            removeBlackRect();
        }
    }

    if (event->button() == Qt::RightButton && device && !device->isCurrentCustomKeymap()) {
        emit device->postBackOrScreenOn(event->type() == QEvent::MouseButtonPress);
    }

    if (m_videoWidget && m_videoWidget->geometry().contains(event->pos())) {
        if (!device) {
            return;
        }
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        QPointF localPos = event->localPos();
        QPointF globalPos = event->globalPos();
#else
        QPointF localPos = event->position();
        QPointF globalPos = event->globalPosition();
#endif
        QPointF mappedPos = m_videoWidget->mapFrom(this, localPos.toPoint());
        QMouseEvent newEvent(event->type(), mappedPos, globalPos, event->button(), event->buttons(), event->modifiers());
        emit device->mouseEvent(&newEvent, m_videoWidget->frameSize(), m_videoWidget->size());
    }
}

void VideoForm::wheelEvent(QWheelEvent *event)
{
    auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
    if (!device || !m_videoWidget) {
        return;
    }
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    if (m_videoWidget->geometry().contains(event->position().toPoint())) {
        QPointF pos = m_videoWidget->mapFrom(this, event->position().toPoint());
        QWheelEvent wheelEvent(
            pos, event->globalPosition(), event->pixelDelta(), event->angleDelta(), event->buttons(), event->modifiers(), event->phase(), event->inverted());
#else
    if (m_videoWidget->geometry().contains(event->pos())) {
        QPointF pos = m_videoWidget->mapFrom(this, event->pos());

        QWheelEvent wheelEvent(
            pos, event->globalPosF(), event->pixelDelta(), event->angleDelta(), event->delta(), event->orientation(),
            event->buttons(), event->modifiers(), event->phase(), event->source(), event->inverted());
#endif
        emit device->wheelEvent(&wheelEvent, m_videoWidget->frameSize(), m_videoWidget->size());
    }
}

void VideoForm::keyPressEvent(QKeyEvent *event)
{
    auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
    if (!device) {
        return;
    }
    if (Qt::Key_Escape == event->key() && !event->isAutoRepeat() && isFullScreen()) {
        switchFullScreen();
    }

    if (m_videoWidget) {
        emit device->keyEvent(event, m_videoWidget->frameSize(), m_videoWidget->size());
    }
}

void VideoForm::keyReleaseEvent(QKeyEvent *event)
{
    auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
    if (!device) {
        return;
    }
    if (m_videoWidget) {
        emit device->keyEvent(event, m_videoWidget->frameSize(), m_videoWidget->size());
    }
}

void VideoForm::paintEvent(QPaintEvent *paint)
{
    Q_UNUSED(paint)
    QStyleOption opt;
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    opt.init(this);
#else
    opt.initFrom(this);
#endif
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void VideoForm::showEvent(QShowEvent *event)
{
    Q_UNUSED(event)
    if (!isFullScreen() && this->show_toolbar) {
        QTimer::singleShot(500, this, [this](){
            showToolForm(this->show_toolbar);
        });
    }

    // Update footer label position when showing
    if (m_footerLabel) {
        m_footerLabel->setGeometry(0, ui->keepRatioWidget->height() - 30, ui->keepRatioWidget->width(), 30);
    }
}

void VideoForm::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event)
    QSize goodSize = ui->keepRatioWidget->goodSize();
    if (goodSize.isEmpty()) {
        return;
    }
    QSize curSize = size();
    // 限制VideoForm尺寸不能小于keepRatioWidget good size
    if (m_widthHeightRatio > 1.0f) {
        // hor
        if (curSize.height() <= goodSize.height()) {
            setMinimumHeight(goodSize.height());
        } else {
            setMinimumHeight(0);
        }
    } else {
        // ver
        if (curSize.width() <= goodSize.width()) {
            setMinimumWidth(goodSize.width());
        } else {
            setMinimumWidth(0);
        }
    }

    // Update footer label position
    if (m_footerLabel) {
        m_footerLabel->setGeometry(0, ui->keepRatioWidget->height() - 30, ui->keepRatioWidget->width(), 30);
    }
}

void VideoForm::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event)
    auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
    if (!device) {
        return;
    }
    Config::getInstance().setRect(device->getSerial(), geometry());
    device->disconnectDevice();
}

void VideoForm::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}

void VideoForm::dragMoveEvent(QDragMoveEvent *event)
{
    Q_UNUSED(event)
}

void VideoForm::dragLeaveEvent(QDragLeaveEvent *event)
{
    Q_UNUSED(event)
}

void VideoForm::dropEvent(QDropEvent *event)
{
    auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
    if (!device) {
        return;
    }
    const QMimeData *qm = event->mimeData();
    QList<QUrl> urls = qm->urls();

    for (const QUrl &url : urls) {
        QString file = url.toLocalFile();
        QFileInfo fileInfo(file);

        if (!fileInfo.exists()) {
            QMessageBox::warning(this, "QtScrcpy", tr("file does not exist"), QMessageBox::Ok);
            continue;
        }

        if (fileInfo.isFile() && fileInfo.suffix() == "apk") {
            emit device->installApkRequest(file);
            continue;
        }
        emit device->pushFileRequest(file, Config::getInstance().getPushFilePath() + fileInfo.fileName());
    }
}
