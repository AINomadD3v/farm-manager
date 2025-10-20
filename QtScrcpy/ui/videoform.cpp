// #include <QDesktopWidget>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QShortcut>
#include <QStyle>
#include <QStyleOption>
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

    // Create placeholder widget for when no video is streaming
    // Make it a child of keepRatioWidget since video widget doesn't exist yet
    m_placeholderWidget = new QWidget(ui->keepRatioWidget);
    m_placeholderWidget->setStyleSheet(R"(
        QWidget#placeholderWidget {
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #2d3436, stop:1 #636e72);
            border: 2px solid #74b9ff;
            border-radius: 8px;
        }
    )");
    m_placeholderWidget->setObjectName("placeholderWidget");

    // Layout for placeholder
    QVBoxLayout* placeholderLayout = new QVBoxLayout(m_placeholderWidget);
    placeholderLayout->setContentsMargins(10, 10, 10, 10);
    placeholderLayout->setSpacing(10);

    // Add spacer to center content
    placeholderLayout->addStretch(1);

    // Phone icon placeholder (using text icon since emoji might not render well)
    QLabel* iconLabel = new QLabel();
    iconLabel->setText("📱");  // Phone emoji
    iconLabel->setAlignment(Qt::AlignCenter);
    QFont iconFont;
    iconFont.setPointSize(48);
    iconLabel->setFont(iconFont);
    iconLabel->setStyleSheet("QLabel { background: transparent; border: none; }");
    placeholderLayout->addWidget(iconLabel);

    // Serial number label
    m_placeholderSerialLabel = new QLabel("Device");
    m_placeholderSerialLabel->setAlignment(Qt::AlignCenter);
    m_placeholderSerialLabel->setWordWrap(true);
    QFont serialFont;
    serialFont.setPointSize(11);
    serialFont.setBold(true);
    m_placeholderSerialLabel->setFont(serialFont);
    m_placeholderSerialLabel->setStyleSheet("QLabel { color: #dfe6e9; background: transparent; border: none; padding: 5px; }");
    placeholderLayout->addWidget(m_placeholderSerialLabel);

    // Status label
    m_placeholderStatusLabel = new QLabel("Ready to Connect");
    m_placeholderStatusLabel->setAlignment(Qt::AlignCenter);
    QFont statusFont;
    statusFont.setPointSize(9);
    m_placeholderStatusLabel->setFont(statusFont);
    m_placeholderStatusLabel->setStyleSheet("QLabel { color: #74b9ff; background: transparent; border: none; }");
    placeholderLayout->addWidget(m_placeholderStatusLabel);

    // Add spacer to center content
    placeholderLayout->addStretch(1);

    // Show placeholder by default (video widget will be created on-demand)
    m_placeholderWidget->show();
    m_placeholderWidget->raise(); // Ensure it's on top

    // Size placeholder to fill the keepRatioWidget
    m_placeholderWidget->setGeometry(ui->keepRatioWidget->rect());

    // Install event filter on keepRatioWidget to handle resizing
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

    qInfo() << "VideoForm::createVideoWidget() - Creating OpenGL widget for:" << m_serial;

    // Create the OpenGL video widget
    m_videoWidget = new QYUVOpenGLWidget(this);
    ui->keepRatioWidget->setWidget(m_videoWidget);

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

    // Reparent placeholder to video widget
    if (m_placeholderWidget) {
        m_placeholderWidget->setParent(m_videoWidget);
        m_placeholderWidget->setGeometry(m_videoWidget->rect());
        m_placeholderWidget->show();
        m_placeholderWidget->raise();
    }

    // Show the video widget
    m_videoWidget->show();
    m_videoWidget->setMouseTracking(true);

    // Install event filter on video widget for resize tracking
    m_videoWidget->installEventFilter(this);

    qInfo() << "VideoForm::createVideoWidget() - OpenGL widget created successfully for:" << m_serial;
}

bool VideoForm::eventFilter(QObject *watched, QEvent *event)
{
    // Update placeholder size when video widget or keepRatioWidget is resized
    if (event->type() == QEvent::Resize && m_placeholderWidget) {
        QWidget* resizedWidget = nullptr;
        if (watched == m_videoWidget && m_videoWidget) {
            resizedWidget = m_videoWidget;
            m_placeholderWidget->setGeometry(m_videoWidget->rect());
        } else if (watched == ui->keepRatioWidget && !m_videoWidget) {
            // If video widget hasn't been created yet, size to keepRatioWidget
            resizedWidget = ui->keepRatioWidget;
            m_placeholderWidget->setGeometry(ui->keepRatioWidget->rect());
        }

        if (!resizedWidget) {
            return QWidget::eventFilter(watched, event);
        }

        // Adjust font sizes based on widget size
        int width = resizedWidget->width();
        int height = resizedWidget->height();

        if (m_placeholderSerialLabel && m_placeholderStatusLabel) {
            // Scale font sizes for small tiles
            int serialFontSize = 11;
            int statusFontSize = 9;
            int iconSize = 48;

            if (width < 150 || height < 250) {
                // Very small tiles (100+ devices)
                serialFontSize = 8;
                statusFontSize = 7;
                iconSize = 24;
            } else if (width < 220 || height < 350) {
                // Small tiles (50-100 devices)
                serialFontSize = 9;
                statusFontSize = 8;
                iconSize = 32;
            }

            // Update fonts
            QFont serialFont = m_placeholderSerialLabel->font();
            serialFont.setPointSize(serialFontSize);
            m_placeholderSerialLabel->setFont(serialFont);

            QFont statusFont = m_placeholderStatusLabel->font();
            statusFont.setPointSize(statusFontSize);
            m_placeholderStatusLabel->setFont(statusFont);

            // Update icon size (find the icon label)
            QList<QLabel*> labels = m_placeholderWidget->findChildren<QLabel*>();
            for (QLabel* label : labels) {
                if (label->text().contains("📱")) {
                    QFont iconFont = label->font();
                    iconFont.setPointSize(iconSize);
                    label->setFont(iconFont);
                    break;
                }
            }
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
    // Create OpenGL video widget on-demand when first frame arrives
    // This prevents GPU resource exhaustion when showing 78+ device tiles
    if (!m_videoWidget) {
        qInfo() << "========================================";
        qInfo() << "VideoForm::updateRender() - FIRST FRAME RECEIVED!";
        qInfo() << "  Serial:" << m_serial;
        qInfo() << "  Frame size:" << width << "x" << height;
        qInfo() << "  Creating OpenGL widget on-demand...";
        qInfo() << "========================================";

        createVideoWidget();
    }

    // Safety check - should never happen but be defensive
    if (!m_videoWidget) {
        qWarning() << "VideoForm::updateRender() - Failed to create video widget for:" << m_serial;
        return;
    }

    if (m_videoWidget->isHidden()) {
        qInfo() << "VideoForm: Video widget was hidden, showing it now for:" << m_serial;
        if (m_loadingWidget) {
            m_loadingWidget->close();
        }
        m_videoWidget->show();

        // Hide placeholder when video starts
        if (m_placeholderWidget) {
            qInfo() << "VideoForm: Hiding placeholder widget for:" << m_serial;
            m_placeholderWidget->hide();
        }

        // Update placeholder status
        if (m_placeholderStatusLabel) {
            m_placeholderStatusLabel->setText("Streaming");
        }
    }

    updateShowSize(QSize(width, height));
    m_videoWidget->setFrameSize(QSize(width, height));
    m_videoWidget->updateTextures(dataY, dataU, dataV, linesizeY, linesizeU, linesizeV);
}

void VideoForm::setSerial(const QString &serial)
{
    m_serial = serial;

    // Update placeholder with serial number
    if (m_placeholderSerialLabel) {
        m_placeholderSerialLabel->setText(serial);
    }
}

void VideoForm::updatePlaceholderStatus(const QString& status, const QString& backgroundColor)
{
    if (m_placeholderStatusLabel) {
        m_placeholderStatusLabel->setText(status);
    }

    if (!backgroundColor.isEmpty() && m_placeholderWidget) {
        QString styleSheet;
        if (backgroundColor == "connecting") {
            // Yellow/orange gradient for connecting state
            styleSheet = R"(
                QWidget#placeholderWidget {
                    background-color: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                        stop:0 #fdcb6e, stop:1 #e17055);
                    border: 2px solid #fdcb6e;
                    border-radius: 8px;
                }
            )";
        } else if (backgroundColor == "streaming") {
            // Green tint for streaming state
            styleSheet = R"(
                QWidget#placeholderWidget {
                    background-color: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                        stop:0 #00b894, stop:1 #00cec9);
                    border: 2px solid #00b894;
                    border-radius: 8px;
                }
            )";
        } else {
            // Default gray gradient for disconnected state
            styleSheet = R"(
                QWidget#placeholderWidget {
                    background-color: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                        stop:0 #2d3436, stop:1 #636e72);
                    border: 2px solid #74b9ff;
                    border-radius: 8px;
                }
            )";
        }
        m_placeholderWidget->setStyleSheet(styleSheet);
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
    updateRender(width, height, dataY, dataU, dataV, linesizeY, linesizeU, linesizeV);
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
    auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);

    // Check if click is on placeholder (when visible) - emit deviceClicked signal
    if (event->button() == Qt::LeftButton && m_placeholderWidget && m_placeholderWidget->isVisible()) {
        // Flash effect for visual feedback
        if (m_placeholderWidget) {
            QString currentBg = m_placeholderWidget->styleSheet();
            m_placeholderWidget->setStyleSheet(R"(
                QWidget#placeholderWidget {
                    background-color: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                        stop:0 #74b9ff, stop:1 #0984e3);
                    border: 2px solid #74b9ff;
                    border-radius: 8px;
                }
            )");
            QTimer::singleShot(100, this, [this, currentBg]() {
                if (m_placeholderWidget) {
                    m_placeholderWidget->setStyleSheet(currentBg);
                }
            });
        }

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
            return;
        }
        QPointF mappedPos = m_videoWidget->mapFrom(this, localPos.toPoint());
        QMouseEvent newEvent(event->type(), mappedPos, globalPos, event->button(), event->buttons(), event->modifiers());
        emit device->mouseEvent(&newEvent, m_videoWidget->frameSize(), m_videoWidget->size());

        // debug keymap pos
        if (event->button() == Qt::LeftButton) {
            qreal x = localPos.x() / m_videoWidget->size().width();
            qreal y = localPos.y() / m_videoWidget->size().height();
            QString posTip = QString(R"("pos": {"x": %1, "y": %2})").arg(x).arg(y);
            qInfo() << posTip.toStdString().c_str();
        }
    } else {
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

    // Update placeholder size when showing
    if (m_placeholderWidget) {
        if (m_videoWidget) {
            m_placeholderWidget->setGeometry(m_videoWidget->rect());
        } else {
            m_placeholderWidget->setGeometry(ui->keepRatioWidget->rect());
        }
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

    // Update placeholder widget size to match video widget or keepRatioWidget
    if (m_placeholderWidget) {
        if (m_videoWidget) {
            m_placeholderWidget->setGeometry(m_videoWidget->rect());
        } else {
            m_placeholderWidget->setGeometry(ui->keepRatioWidget->rect());
        }
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
