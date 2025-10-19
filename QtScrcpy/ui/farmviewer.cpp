#include "farmviewer.h"
#include "videoform.h"
#include "../groupcontroller/groupcontroller.h"
#include "../util/config.h"
#include "QtScrcpyCore.h"
#include "adbprocess.h"

#include <QDebug>
#include <QApplication>
#include <QScreen>
#include <QTimer>
#include <QProcess>
#include <QRandomGenerator>
#include <QFileInfo>
#include <QCoreApplication>

#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>

FarmViewer* FarmViewer::s_instance = nullptr;
int FarmViewer::s_signalFd[2] = {-1, -1};

FarmViewer::FarmViewer(QWidget *parent)
    : QWidget(parent)
    , m_scrollArea(nullptr)
    , m_gridWidget(nullptr)
    , m_gridLayout(nullptr)
    , m_mainLayout(nullptr)
    , m_toolbarLayout(nullptr)
    , m_connectionThrottleTimer(nullptr)
    , m_resourceCheckTimer(nullptr)
    , m_gridRows(2)
    , m_gridCols(2)
    , m_currentQualityProfile(720, 4000000, 30, "Default")
    , m_currentQualityTier(qsc::DeviceConnectionPool::TIER_HIGH)
    , m_screenshotAllBtn(nullptr)
    , m_syncActionBtn(nullptr)
    , m_statusLabel(nullptr)
    , m_connectionProgressBar(nullptr)
    , m_isConnecting(false)
    , m_signalNotifier(nullptr)
    , m_isShuttingDown(false)
{
    qInfo() << "FarmViewer: Constructor started";
    qInfo() << "FarmViewer: Calling setupUI()...";
    setupUI();
    qInfo() << "FarmViewer: setupUI() completed";
    qInfo() << "FarmViewer: Setting window properties...";
    setWindowTitle("QtScrcpy Farm Viewer");
    setMinimumSize(800, 600);
    resize(1200, 900);

    qInfo() << "FarmViewer: Centering window on screen...";
    // Center window on screen
    QScreen *screen = QApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->availableGeometry();
        int x = (screenGeometry.width() - width()) / 2;
        int y = (screenGeometry.height() - height()) / 2;
        move(x, y);
    }
    qInfo() << "FarmViewer: Window centered";

    qInfo() << "FarmViewer: Setting up device detection ADB connection...";
    // Setup device detection using existing Dialog pattern
    connect(&m_deviceDetectionAdb, &qsc::AdbProcess::adbProcessResult, this,
        [this](qsc::AdbProcess::ADB_EXEC_RESULT processResult) {
            qInfo() << "FarmViewer: ADB CALLBACK TRIGGERED! Result:" << processResult;

            if (processResult == qsc::AdbProcess::AER_SUCCESS_EXEC) {
                qInfo() << "FarmViewer: ADB success, checking arguments...";
                QStringList args = m_deviceDetectionAdb.arguments();
                qInfo() << "FarmViewer: ADB arguments:" << args;

                if (args.contains("devices")) {
                    qInfo() << "FarmViewer: Getting device list from ADB output...";
                    QStringList devices = m_deviceDetectionAdb.getDevicesSerialFromStdOut();
                    qInfo() << "FarmViewer: Found" << devices.size() << "devices:" << devices;
                    qInfo() << "FarmViewer: Calling processDetectedDevices()...";
                    processDetectedDevices(devices);
                } else {
                    qWarning() << "FarmViewer: ADB args don't contain 'devices'";
                }
            } else {
                qWarning() << "FarmViewer: ADB failed with result:" << processResult;
            }
        });
    qInfo() << "FarmViewer: Device detection ADB connection completed";

    qInfo() << "FarmViewer: Connecting to IDeviceManage signals...";
    // SIMPLIFIED: Remove IDeviceManage signal connections since we're not auto-connecting
    // Devices are only displayed, not connected
    qInfo() << "FarmViewer: Skipping IDeviceManage signal connections (display-only mode)";

    qInfo() << "FarmViewer: Setting up Unix signal socket notifier...";
    // Setup socket notifier for Unix signals
    // The socketpair was created in setupSocketPair() before Qt event loop started
    if (s_signalFd[1] != -1) {
        m_signalNotifier = new QSocketNotifier(s_signalFd[1], QSocketNotifier::Read, this);
        connect(m_signalNotifier, &QSocketNotifier::activated, this, &FarmViewer::handleUnixSignal);
        qInfo() << "FarmViewer: Unix signal handler initialized (socketpair pattern)";
    }

    // Connect to application aboutToQuit to ensure cleanup - deferred to avoid blocking
    QTimer::singleShot(0, this, [this]() {
        connect(qApp, &QApplication::aboutToQuit, this, &FarmViewer::cleanupAndExit);
        qInfo() << "FarmViewer: Cleanup handler connected";
    });

    // SIMPLIFIED: No connection management needed in display-only mode
    m_resourceCheckTimer = nullptr;
    m_connectionThrottleTimer = nullptr;
    qInfo() << "FarmViewer: Resource monitoring and connection throttle DISABLED (display-only mode)";

    qInfo() << "FarmViewer: Constructor completed successfully";
}

FarmViewer::~FarmViewer()
{
    qDebug() << "FarmViewer: Destructor called";

    // Ensure cleanup happens
    if (!m_isShuttingDown) {
        cleanupAndExit();
    }

    // Cleanup socket notifier
    if (m_signalNotifier) {
        delete m_signalNotifier;
        m_signalNotifier = nullptr;
    }

    // Close signal sockets
    if (s_signalFd[0] != -1) {
        ::close(s_signalFd[0]);
        s_signalFd[0] = -1;
    }
    if (s_signalFd[1] != -1) {
        ::close(s_signalFd[1]);
        s_signalFd[1] = -1;
    }
}

FarmViewer& FarmViewer::instance()
{
    if (!s_instance) {
        s_instance = new FarmViewer();
    }
    return *s_instance;
}

void FarmViewer::setupUI()
{
    // Main layout
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(5, 5, 5, 5);
    m_mainLayout->setSpacing(5);
    
    // Toolbar
    m_toolbarLayout = new QHBoxLayout();
    m_toolbarLayout->setContentsMargins(0, 0, 0, 0);
    m_toolbarLayout->setSpacing(10);
    
    // Toolbar buttons
    m_screenshotAllBtn = new QPushButton("Screenshot All");
    m_screenshotAllBtn->setMaximumWidth(120);
    connect(m_screenshotAllBtn, &QPushButton::clicked, this, &FarmViewer::onScreenshotAllClicked);
    
    m_syncActionBtn = new QPushButton("Sync Actions");
    m_syncActionBtn->setMaximumWidth(120);
    connect(m_syncActionBtn, &QPushButton::clicked, this, &FarmViewer::onSyncActionClicked);
    
    // Status label
    m_statusLabel = new QLabel("No devices connected");
    m_statusLabel->setStyleSheet("color: #888; font-size: 12px;");

    // Connection progress bar
    m_connectionProgressBar = new QProgressBar();
    m_connectionProgressBar->setMaximumWidth(200);
    m_connectionProgressBar->setMaximumHeight(20);
    m_connectionProgressBar->setVisible(false);  // Hidden by default

    // Add to toolbar
    m_toolbarLayout->addWidget(m_screenshotAllBtn);
    m_toolbarLayout->addWidget(m_syncActionBtn);
    m_toolbarLayout->addWidget(m_connectionProgressBar);
    m_toolbarLayout->addStretch();
    m_toolbarLayout->addWidget(m_statusLabel);
    
    // Scroll area for device grid
    m_scrollArea = new QScrollArea();
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    // Grid widget
    m_gridWidget = new QWidget();
    m_gridLayout = new QGridLayout(m_gridWidget);
    m_gridLayout->setContentsMargins(10, 10, 10, 10);
    m_gridLayout->setSpacing(10);
    
    m_scrollArea->setWidget(m_gridWidget);
    
    // Add to main layout
    m_mainLayout->addLayout(m_toolbarLayout);
    m_mainLayout->addWidget(m_scrollArea, 1);
    
    setLayout(m_mainLayout);
}

void FarmViewer::addDevice(const QString& serial, const QString& deviceName, const QSize& size)
{
    Q_UNUSED(size);
    qDebug() << "FarmViewer: Adding device to UI (display only, no connection):" << serial << deviceName;

    // Don't add if already exists
    if (m_deviceForms.contains(serial)) {
        qDebug() << "Device already exists in farm viewer:" << serial;
        return;
    }

    // Calculate optimal tile size for current device count
    int newDeviceCount = m_deviceForms.size() + 1;
    QSize tileSize = getOptimalTileSize(newDeviceCount, this->size());

    // Create VideoForm for this device with dynamic sizing
    auto videoForm = new VideoForm(true, false, false, this); // frameless, no skin, no toolbar
    videoForm->setSerial(serial);
    videoForm->setMinimumSize(tileSize * 0.9); // Slightly smaller than container
    videoForm->setMaximumSize(tileSize * 1.8); // Allow some growth

    // Create container widget with device label
    QWidget* container = createDeviceWidget(serial, deviceName);

    // Add VideoForm to container
    QVBoxLayout* containerLayout = qobject_cast<QVBoxLayout*>(container->layout());
    if (containerLayout) {
        containerLayout->insertWidget(0, videoForm, 1); // Insert before label, with stretch
    }

    // Store references
    m_deviceForms[serial] = videoForm;
    m_deviceContainers[serial] = container;

    // SIMPLIFIED: Do NOT register observers or connect to device
    // Just display the placeholder UI
    qDebug() << "FarmViewer: Device displayed in UI (ready for manual connection):" << serial;

    // Recalculate optimal grid for new device count
    calculateOptimalGrid(newDeviceCount, this->size());

    // Update grid layout
    updateGridLayout();

    // Update status
    updateStatus();

    // Show VideoForm
    videoForm->show();

    qDebug() << "FarmViewer: Device UI placeholder added successfully" << serial
             << "Grid:" << m_gridRows << "x" << m_gridCols
             << "Tile size:" << tileSize;
}

void FarmViewer::removeDevice(const QString& serial)
{
    qDebug() << "FarmViewer: Removing device UI placeholder:" << serial;

    // Remove from grid layout
    if (m_deviceContainers.contains(serial) && !m_deviceContainers[serial].isNull()) {
        m_gridLayout->removeWidget(m_deviceContainers[serial]);
        delete m_deviceContainers[serial];
        m_deviceContainers.remove(serial);
    }

    // Clean up VideoForm (no observer deregistration needed since we never registered)
    if (m_deviceForms.contains(serial) && !m_deviceForms[serial].isNull()) {
        delete m_deviceForms[serial];
        m_deviceForms.remove(serial);
    }

    // Recalculate grid for remaining devices
    int remainingDevices = m_deviceForms.size();
    if (remainingDevices > 0) {
        calculateOptimalGrid(remainingDevices, this->size());
    }

    // Update grid layout
    updateGridLayout();

    // Update status
    updateStatus();

    qDebug() << "FarmViewer: Device UI placeholder removed successfully:" << serial
             << "Remaining devices:" << remainingDevices;
}

QWidget* FarmViewer::createDeviceWidget(const QString& serial, const QString& deviceName)
{
    // Calculate dynamic tile size based on current device count
    int currentDeviceCount = m_deviceForms.size() + 1; // +1 for the device being added
    QSize tileSize = getOptimalTileSize(currentDeviceCount, size());

    QWidget* container = new QWidget();
    container->setMinimumSize(tileSize);
    container->setMaximumSize(tileSize * 2); // Allow some growth
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QVBoxLayout* layout = new QVBoxLayout(container);
    layout->setContentsMargins(5, 5, 5, 5);
    layout->setSpacing(5);

    // Device info label - scale font based on tile size
    int fontSize = (tileSize.width() < 150) ? 9 : 11;
    QLabel* deviceLabel = new QLabel(QString("%1\n%2").arg(deviceName, serial));
    deviceLabel->setAlignment(Qt::AlignCenter);
    deviceLabel->setStyleSheet(QString("QLabel { color: #333; font-size: %1px; font-weight: bold; "
                              "background-color: #f0f0f0; padding: 5px; border-radius: 3px; }").arg(fontSize));
    deviceLabel->setMaximumHeight(50);

    // VideoForm will be inserted at position 0 by addDevice
    layout->addWidget(deviceLabel);

    container->setLayout(layout);
    return container;
}

void FarmViewer::updateGridLayout()
{
    // Remove all widgets from layout first
    while (QLayoutItem* item = m_gridLayout->takeAt(0)) {
        delete item;
    }
    
    // Add devices back to grid
    int deviceCount = 0;
    for (auto it = m_deviceContainers.begin(); it != m_deviceContainers.end(); ++it) {
        if (!it.value().isNull()) {
            int row = deviceCount / m_gridCols;
            int col = deviceCount % m_gridCols;
            m_gridLayout->addWidget(it.value(), row, col);
            deviceCount++;
        }
    }
    
    // Adjust grid widget size
    m_gridWidget->adjustSize();
}

void FarmViewer::updateStatus()
{
    int deviceCount = m_deviceForms.size();
    if (deviceCount == 0) {
        m_statusLabel->setText("No devices detected");
        m_statusLabel->setStyleSheet("color: #888; font-size: 12px;");
    } else {
        // SIMPLIFIED: Just show device count (no quality info since we're not streaming)
        m_statusLabel->setText(QString("%1 devices detected (ready for connection)")
            .arg(deviceCount));
        m_statusLabel->setStyleSheet("color: #0a84ff; font-size: 12px; font-weight: bold;");
    }
}

void FarmViewer::setGridSize(int rows, int cols)
{
    m_gridRows = rows;
    m_gridCols = cols;
    updateGridLayout();
}

void FarmViewer::calculateOptimalGrid(int deviceCount, const QSize& windowSize)
{
    if (deviceCount <= 0) {
        m_gridRows = 1;
        m_gridCols = 1;
        return;
    }

    // Calculate optimal columns based on window size and device count
    m_gridCols = calculateColumns(deviceCount, windowSize);

    // Calculate rows needed
    m_gridRows = (deviceCount + m_gridCols - 1) / m_gridCols; // Ceiling division

    qDebug() << "FarmViewer: Calculated optimal grid for" << deviceCount << "devices:"
             << m_gridRows << "rows x" << m_gridCols << "cols"
             << "Window size:" << windowSize;

    // Update quality profile based on device count
    auto newProfile = getOptimalStreamSettings(deviceCount);
    auto newTier = qsc::DeviceConnectionPool::instance().getQualityTier(deviceCount);

    // If quality tier changed, apply to all devices
    if (newTier != m_currentQualityTier) {
        qDebug() << "FarmViewer: Quality tier changed from" << m_currentQualityTier
                 << "to" << newTier << "- applying to all devices";
        m_currentQualityTier = newTier;
        m_currentQualityProfile = newProfile;
        applyQualityToAllDevices();
    }
}

int FarmViewer::calculateColumns(int deviceCount, const QSize& windowSize) const
{
    // Calculate based on both device count and window width
    // Aim for roughly square grid, but consider window aspect ratio

    if (deviceCount <= 1) return 1;
    if (deviceCount <= 4) return 2;  // 2x2 for up to 4 devices
    if (deviceCount <= 9) return 3;  // 3x3 for 5-9 devices

    // For larger counts, calculate based on window width
    // Target tile width varies by device count
    QSize tileSize = getOptimalTileSize(deviceCount, windowSize);

    int availableWidth = windowSize.width() - 40; // Account for margins/scrollbar
    int cols = qMax(1, availableWidth / tileSize.width());

    // Clamp to reasonable values
    if (deviceCount <= 20) {
        return qMin(cols, 5); // Max 5 columns for 10-20 devices
    } else if (deviceCount <= 50) {
        return qMin(cols, 8); // Max 8 columns for 21-50 devices
    } else {
        return qMin(cols, 10); // Max 10 columns for 50+ devices
    }
}

QSize FarmViewer::getOptimalTileSize(int deviceCount, const QSize& windowSize) const
{
    Q_UNUSED(windowSize);

    // Tile sizes scale down with device count
    // Maintain 9:16 aspect ratio (portrait phone)

    int width, height;

    if (deviceCount <= 5) {
        width = 300;
        height = 600;
    } else if (deviceCount <= 20) {
        width = 220;
        height = 450;
    } else if (deviceCount <= 50) {
        width = 160;
        height = 320;
    } else {
        // 100+ devices: very small tiles
        width = 120;
        height = 240;
    }

    return QSize(width, height);
}

qsc::StreamQualityProfile FarmViewer::getOptimalStreamSettings(int totalDeviceCount)
{
    return qsc::DeviceConnectionPool::instance().getOptimalStreamSettings(totalDeviceCount);
}

void FarmViewer::applyQualityToAllDevices()
{
    qDebug() << "FarmViewer: Applying quality profile to all devices:" << m_currentQualityProfile.description;

    // Note: For existing connections, we can't change stream quality on the fly
    // This will apply to new connections. For a full implementation, you would need to:
    // 1. Disconnect existing devices
    // 2. Reconnect with new quality settings
    // For now, we'll just log a warning

    if (!m_deviceForms.isEmpty()) {
        qWarning() << "FarmViewer: Quality changes will apply to new connections only.";
        qWarning() << "FarmViewer: Consider reconnecting devices to apply new quality settings.";
    }
}

void FarmViewer::updateDeviceQuality(const QString& serial, const qsc::StreamQualityProfile& profile)
{
    Q_UNUSED(serial);
    Q_UNUSED(profile);

    // Placeholder for per-device quality updates
    // Would require reconnection with new params
    qDebug() << "FarmViewer: Per-device quality update requested for" << serial;
}

void FarmViewer::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    // Recalculate grid when window resizes
    int deviceCount = m_deviceForms.size();
    if (deviceCount > 0) {
        calculateOptimalGrid(deviceCount, event->size());

        // Update widget sizes based on new grid
        QSize tileSize = getOptimalTileSize(deviceCount, event->size());

        for (auto it = m_deviceContainers.begin(); it != m_deviceContainers.end(); ++it) {
            if (!it.value().isNull()) {
                it.value()->setMinimumSize(tileSize);
                it.value()->setMaximumSize(tileSize * 2); // Allow some growth
            }
        }

        // Rebuild grid layout
        updateGridLayout();
    }
}

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
    qInfo() << "FarmViewer: Loading already-connected devices...";
    QStringList connectedSerials = qsc::IDeviceManage::getInstance().getAllConnectedSerials();
    qInfo() << "FarmViewer: Found" << connectedSerials.size() << "already-connected devices";

    for (const QString& serial : connectedSerials) {
        auto device = qsc::IDeviceManage::getInstance().getDevice(serial);
        if (device) {
            // Get device name and size from the device
            QString deviceName = serial; // TODO: Get actual device name if available
            QSize size(720, 1280); // TODO: Get actual screen size if available
            qInfo() << "FarmViewer: Adding already-connected device:" << serial;
            addDevice(serial, deviceName, size);
        }
    }

    // If no devices are connected yet, auto-detect and connect to available devices
    // IMPORTANT: Defer to event loop to prevent blocking the UI
    if (connectedSerials.isEmpty()) {
        qInfo() << "FarmViewer: No devices connected yet, scheduling auto-detection...";
        m_statusLabel->setText("Detecting devices...");
        m_statusLabel->setStyleSheet("color: #0a84ff; font-size: 12px;");

        // Defer autoDetectAndConnectDevices to next event loop iteration
        QTimer::singleShot(0, this, [this]() {
            autoDetectAndConnectDevices();
        });
    }

    qInfo() << "FarmViewer: showFarmViewer() completed";
}

void FarmViewer::hideFarmViewer()
{
    hide();
}

void FarmViewer::onScreenshotAllClicked()
{
    qDebug() << "FarmViewer: Screenshot all devices";
    // TODO: Implement screenshot all functionality
    // Need to iterate through devices and call screenshot on each individually
    for (auto it = m_deviceForms.begin(); it != m_deviceForms.end(); ++it) {
        if (!it.value().isNull()) {
            // Each VideoForm would need screenshot capability
            qDebug() << "Taking screenshot of device:" << it.key();
        }
    }
}

void FarmViewer::onSyncActionClicked()
{
    qDebug() << "FarmViewer: Sync actions clicked";
    // TODO: Implement sync action dialog
}

void FarmViewer::autoDetectAndConnectDevices()
{
    qDebug() << "FarmViewer: Starting device auto-detection using existing Dialog pattern...";

    // Use the exact same pattern as Dialog::on_updateDevice_clicked()
    if (m_deviceDetectionAdb.isRuning()) {
        qDebug() << "FarmViewer: Device detection already running, skipping";
        return;
    }

    // Update status to show detection in progress
    m_statusLabel->setText("Detecting devices...");
    m_statusLabel->setStyleSheet("color: #0a84ff; font-size: 12px;");

    // IMPORTANT: Wrap ADB execute in QTimer::singleShot for safety
    // This ensures ADB doesn't block the main thread even if it takes time
    qDebug() << "FarmViewer: Scheduling ADB devices command...";
    QTimer::singleShot(0, this, [this]() {
        qDebug() << "FarmViewer: Executing ADB devices command...";
        m_deviceDetectionAdb.execute("", QStringList() << "devices");
    });
}

void FarmViewer::processDetectedDevices(const QStringList& devices)
{
    qInfo() << "========================================";
    qInfo() << "FarmViewer: Processing detected devices:" << devices;
    qInfo() << "Device count:" << devices.size();
    qInfo() << "========================================";

    if (devices.isEmpty()) {
        qDebug() << "FarmViewer: No devices detected";
        m_statusLabel->setText("No devices found");
        m_statusLabel->setStyleSheet("color: #888; font-size: 12px;");
        return;
    }

    qDebug() << "FarmViewer: Found" << devices.size() << "devices";

    // SIMPLIFIED: Just display devices without connecting/streaming
    // Add each device to the UI grid
    for (const QString& serial : devices) {
        if (!m_deviceForms.contains(serial)) {
            qDebug() << "FarmViewer: Adding device to UI (no connection):" << serial;
            // Display device with default size - NO CONNECTION
            addDevice(serial, serial, QSize(720, 1280));
        } else {
            qDebug() << "FarmViewer: Device already displayed, skipping:" << serial;
        }
    }

    // Update status to show detected devices
    m_statusLabel->setText(QString("%1 devices detected").arg(devices.size()));
    m_statusLabel->setStyleSheet("color: #0a84ff; font-size: 12px; font-weight: bold;");

    qInfo() << "FarmViewer: All" << devices.size() << "devices displayed in UI";
    qInfo() << "========================================";
}

bool FarmViewer::isVisible() const
{
    return QWidget::isVisible();
}

bool FarmViewer::isManagingDevice(const QString& serial) const
{
    return m_deviceForms.contains(serial);
}

void FarmViewer::connectToDevice(const QString& serial)
{
    qDebug() << "FarmViewer: Auto-connecting to device:" << serial;

    // Don't connect if already connected
    if (m_deviceForms.contains(serial)) {
        qDebug() << "FarmViewer: Device already connected:" << serial;
        return;
    }

    // Check connection pool limits
    if (!qsc::DeviceConnectionPool::instance().canAcquireNewConnection()) {
        qWarning() << "FarmViewer: Connection pool limit reached, cannot connect to:" << serial;
        return;
    }

    // Use the GLOBAL quality profile that was set in processDetectedDevices()
    // This ensures all devices in this batch use the same quality tier
    qsc::StreamQualityProfile qualityProfile = m_currentQualityProfile;

    // Create device parameters with adaptive quality settings
    qsc::DeviceParams params;
    params.serial = serial;

    // Apply quality profile
    qsc::DeviceConnectionPool::instance().applyQualityProfile(params, qualityProfile);

    // Assign unique port for each device to avoid conflicts
    static quint16 nextPort = 27183;
    params.localPort = nextPort++;
    if (nextPort > 30000) nextPort = 27183; // Wrap around to avoid exhausting ports

    qDebug() << "FarmViewer: Connecting device" << serial
             << "Port:" << params.localPort
             << "Quality:" << qualityProfile.description
             << "Resolution:" << qualityProfile.maxSize
             << "Bitrate:" << (qualityProfile.bitRate / 1000000.0) << "Mbps"
             << "FPS:" << qualityProfile.maxFps;

    // Set remaining parameters
    params.closeScreen = false;
    params.useReverse = true;
    params.display = true;
    params.renderExpiredFrames = Config::getInstance().getRenderExpiredFrames();
    params.captureOrientationLock = 0; // No orientation lock
    params.captureOrientation = 0;
    params.stayAwake = true;
    params.recordFile = false;
    params.recordPath = "";
    params.recordFileFormat = "mp4";
    params.serverLocalPath = getServerPath();
    params.serverRemotePath = Config::getInstance().getServerPath();
    params.pushFilePath = Config::getInstance().getPushFilePath();
    params.serverVersion = Config::getInstance().getServerVersion();
    params.logLevel = Config::getInstance().getLogLevel();
    params.codecOptions = Config::getInstance().getCodecOptions();
    params.codecName = Config::getInstance().getCodecName();
    params.scid = QRandomGenerator::global()->bounded(1, 10000) & 0x7FFFFFFF;

    // Acquire connection from pool (this will create or reuse a connection)
    auto pooledDevice = qsc::DeviceConnectionPool::instance().acquireConnection(params);

    // Connect the device using IDeviceManage
    qsc::IDeviceManage::getInstance().connectDevice(params);

    qDebug() << "FarmViewer: Connection request sent for device:" << serial
             << "Total pool connections:" << qsc::DeviceConnectionPool::instance().getTotalConnectionCount()
             << "Active:" << qsc::DeviceConnectionPool::instance().getActiveConnectionCount();
}

const QString &FarmViewer::getServerPath()
{
    static QString serverPath;
    if (serverPath.isEmpty()) {
        serverPath = QString::fromLocal8Bit(qgetenv("QTSCRCPY_SERVER_PATH"));
        QFileInfo fileInfo(serverPath);
        if (serverPath.isEmpty() || !fileInfo.isFile()) {
            serverPath = QCoreApplication::applicationDirPath() + "/scrcpy-server";
        }
    }
    return serverPath;
}

void FarmViewer::onGridSizeChanged()
{
    // TODO: Implement grid size selector
}

void FarmViewer::onConnectionBatchStarted(int totalDevices)
{
    qInfo() << "FarmViewer: Starting connection batch for" << totalDevices << "devices";
    m_connectionProgressBar->setMaximum(totalDevices);
    m_connectionProgressBar->setValue(0);
    m_connectionProgressBar->setVisible(true);
    updateStatus();
}

void FarmViewer::onConnectionBatchProgress(int completed, int total, int failed)
{
    qInfo() << "FarmViewer: Connection progress:" << completed << "/" << total << "(failed:" << failed << ")";
    m_connectionProgressBar->setValue(completed);

    QString statusText = QString("Connecting: %1/%2").arg(completed).arg(total);
    if (failed > 0) {
        statusText += QString(" (%1 failed)").arg(failed);
    }
    m_statusLabel->setText(statusText);
}

void FarmViewer::onConnectionBatchCompleted(int successful, int failed)
{
    qInfo() << "FarmViewer: Connection batch completed -" << successful << "successful," << failed << "failed";
    m_connectionProgressBar->setVisible(false);

    QString statusText = QString("Connected: %1 devices").arg(successful);
    if (failed > 0) {
        statusText += QString(" (%1 failed)").arg(failed);
    }
    m_statusLabel->setText(statusText);
    updateStatus();
}

// ============================================================================
// Unix Signal Handling Implementation (Socket-Pair Pattern)
// ============================================================================

void FarmViewer::setupSocketPair()
{
    // Create a Unix domain socket pair for async-signal-safe communication
    // This MUST be called before the Qt event loop starts
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, s_signalFd) != 0) {
        qCritical() << "FarmViewer: Failed to create signal socket pair:" << strerror(errno);
        return;
    }

    qInfo() << "FarmViewer: Signal socket pair created: fd[0]=" << s_signalFd[0]
            << "fd[1]=" << s_signalFd[1];
}

void FarmViewer::unixSignalHandler(int signalNumber)
{
    // CRITICAL: This function runs in signal handler context
    // ONLY async-signal-safe functions are allowed here
    // See: man 7 signal-safety for the list of safe functions
    //
    // We CANNOT:
    // - Call Qt functions
    // - Use malloc/free
    // - Use qDebug/qInfo
    // - Access non-atomic variables (except sig_atomic_t)
    //
    // We CAN:
    // - Use write() to notify Qt event loop via socket

    const char *signalName = "UNKNOWN";
    switch (signalNumber) {
        case SIGINT:  signalName = "SIGINT";  break;
        case SIGTERM: signalName = "SIGTERM"; break;
        default: break;
    }

    // Write signal number to socket to wake up Qt event loop
    // This is async-signal-safe and the ONLY safe way to communicate with Qt
    if (s_signalFd[0] != -1) {
        ssize_t result = ::write(s_signalFd[0], &signalNumber, sizeof(signalNumber));
        (void)result; // Suppress unused warning; we can't do error handling here
    }

    // For informational purposes only - write to stderr (async-signal-safe)
    // This appears in logs but doesn't use Qt logging
    const char msg[] = "Signal received: ";
    ssize_t r1 = ::write(STDERR_FILENO, msg, sizeof(msg) - 1);
    ssize_t r2 = ::write(STDERR_FILENO, signalName, strlen(signalName));
    ssize_t r3 = ::write(STDERR_FILENO, "\n", 1);
    (void)r1; (void)r2; (void)r3; // Suppress unused warnings
}

void FarmViewer::setupUnixSignalHandlers()
{
    // Setup socket pair for signal communication
    setupSocketPair();

    // Install signal handlers for SIGINT (Ctrl+C) and SIGTERM
    struct sigaction sigInt, sigTerm;

    // SIGINT handler (Ctrl+C)
    sigInt.sa_handler = FarmViewer::unixSignalHandler;
    sigemptyset(&sigInt.sa_mask);
    sigInt.sa_flags = 0;
    sigInt.sa_flags |= SA_RESTART; // Restart interrupted system calls

    if (sigaction(SIGINT, &sigInt, nullptr) != 0) {
        qCritical() << "FarmViewer: Failed to install SIGINT handler:" << strerror(errno);
    } else {
        qInfo() << "FarmViewer: SIGINT (Ctrl+C) handler installed";
    }

    // SIGTERM handler (kill command)
    sigTerm.sa_handler = FarmViewer::unixSignalHandler;
    sigemptyset(&sigTerm.sa_mask);
    sigTerm.sa_flags = 0;
    sigTerm.sa_flags |= SA_RESTART;

    if (sigaction(SIGTERM, &sigTerm, nullptr) != 0) {
        qCritical() << "FarmViewer: Failed to install SIGTERM handler:" << strerror(errno);
    } else {
        qInfo() << "FarmViewer: SIGTERM handler installed";
    }
}

void FarmViewer::handleUnixSignal()
{
    // This slot is called by Qt event loop when signal socket has data
    // We're back in Qt context, so all Qt functions are safe to use

    // Disable the notifier temporarily to prevent recursion
    if (m_signalNotifier) {
        m_signalNotifier->setEnabled(false);
    }

    // Read the signal number from the socket
    int signalNumber = 0;
    ssize_t bytesRead = ::read(s_signalFd[1], &signalNumber, sizeof(signalNumber));

    if (bytesRead == sizeof(signalNumber)) {
        const char* signalName = "UNKNOWN";
        switch (signalNumber) {
            case SIGINT:  signalName = "SIGINT (Ctrl+C)"; break;
            case SIGTERM: signalName = "SIGTERM"; break;
            default: break;
        }

        qInfo() << "FarmViewer: Received signal" << signalNumber << "(" << signalName << ")";
        qInfo() << "FarmViewer: Initiating graceful shutdown...";

        // Perform graceful shutdown
        cleanupAndExit();

        // Quit the application
        QApplication::quit();
    } else {
        qWarning() << "FarmViewer: Failed to read signal from socket, bytes read:" << bytesRead;
    }

    // Re-enable the notifier for future signals
    if (m_signalNotifier) {
        m_signalNotifier->setEnabled(true);
    }
}

void FarmViewer::cleanupAndExit()
{
    // Prevent multiple cleanup calls
    if (m_isShuttingDown) {
        qDebug() << "FarmViewer: Cleanup already in progress, skipping";
        return;
    }

    m_isShuttingDown = true;
    qInfo() << "FarmViewer: Starting cleanup sequence...";

    // Stop accepting new connections
    qInfo() << "FarmViewer: Stopping device detection...";
    if (m_deviceDetectionAdb.isRuning()) {
        // AdbProcess doesn't have a kill method, but we can wait for it
        qDebug() << "FarmViewer: Waiting for device detection ADB to finish...";
    }

    // Clean up UI widgets (no actual device disconnection needed)
    qInfo() << "FarmViewer: Cleaning up" << m_deviceForms.size() << "device UI placeholders...";

    QStringList deviceSerials = m_deviceForms.keys();
    for (const QString& serial : deviceSerials) {
        qInfo() << "FarmViewer: Cleaning up device UI:" << serial;

        // Delete the VideoForm (no observer deregistration since we never registered)
        if (m_deviceForms.contains(serial) && !m_deviceForms[serial].isNull()) {
            delete m_deviceForms[serial];
        }

        // Delete the container widget
        if (m_deviceContainers.contains(serial) && !m_deviceContainers[serial].isNull()) {
            delete m_deviceContainers[serial];
        }
    }

    // Clear all maps
    m_deviceForms.clear();
    m_deviceContainers.clear();

    qInfo() << "FarmViewer: All device UI placeholders cleaned up";

    // Give ADB processes a moment to terminate gracefully
    // Process pending events to allow deleteLater() and other cleanup to execute
    qInfo() << "FarmViewer: Processing pending events for cleanup...";
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 1000);

    qInfo() << "FarmViewer: Cleanup sequence completed successfully";
}
int FarmViewer::getMaxParallelForDeviceCount(int totalDeviceCount)
{
    // SIMPLIFIED: Always connect 1 device at a time
    Q_UNUSED(totalDeviceCount);
    return 1;
}

void FarmViewer::processConnectionQueue()
{
    // REMOVED: No connection queue processing in display-only mode
    qDebug() << "FarmViewer: processConnectionQueue() called but disabled in display-only mode";
}

void FarmViewer::onConnectionComplete(const QString& serial, bool success)
{
    // REMOVED: No connection management in display-only mode
    Q_UNUSED(serial);
    Q_UNUSED(success);
    qDebug() << "FarmViewer: onConnectionComplete() called but disabled in display-only mode";
}

// ============================================================================
// Resource Management Implementation
// ============================================================================

bool FarmViewer::checkMemoryAvailable()
{
    // DISABLED: Memory checks not needed for sequential connections
    // Always return true
    return true;
}

void FarmViewer::logResourceUsage(const QString& context)
{
    // SIMPLIFIED: Basic logging only
    int activeDevices = m_deviceForms.size();
    int poolConnections = qsc::DeviceConnectionPool::instance().getTotalConnectionCount();

    qInfo() << "=== Resource Usage [" << context << "] ===";
    qInfo() << "  Active Devices:" << activeDevices;
    qInfo() << "  Pool Connections:" << poolConnections;
    qInfo() << "  Quality Tier:" << m_currentQualityProfile.description;
    qInfo() << "=======================================";
}

int FarmViewer::calculateConnectionDelay(int queueSize)
{
    // SIMPLIFIED: Fixed delay for all connections
    Q_UNUSED(queueSize);
    return 500;  // 500ms delay between all connections
}


void FarmViewer::onResourceCheckTimer()
{
    // DISABLED: Resource monitoring not needed for sequential connections
    // This slot should never be called since the timer is disabled
    qDebug() << "FarmViewer: Resource check timer called (should be disabled)";
}

void FarmViewer::onConnectionThrottleTimer()
{
    // REMOVED: No throttle timer needed in display-only mode
    qDebug() << "FarmViewer: onConnectionThrottleTimer() called but disabled in display-only mode";
}
