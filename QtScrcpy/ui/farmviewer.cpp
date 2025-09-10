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

FarmViewer* FarmViewer::s_instance = nullptr;

FarmViewer::FarmViewer(QWidget *parent)
    : QWidget(parent)
    , ui(nullptr)
    , m_scrollArea(nullptr)
    , m_gridWidget(nullptr)
    , m_gridLayout(nullptr)
    , m_mainLayout(nullptr)
    , m_toolbarLayout(nullptr)
    , m_gridRows(2)
    , m_gridCols(2)
    , m_screenshotAllBtn(nullptr)
    , m_syncActionBtn(nullptr)
    , m_statusLabel(nullptr)
{
    setupUI();
    setWindowTitle("QtScrcpy Farm Viewer");
    setMinimumSize(800, 600);
    resize(1200, 900);
    
    // Center window on screen
    QScreen *screen = QApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->availableGeometry();
        int x = (screenGeometry.width() - width()) / 2;
        int y = (screenGeometry.height() - height()) / 2;
        move(x, y);
    }
    
    // Setup device detection using existing Dialog pattern
    connect(&m_deviceDetectionAdb, &qsc::AdbProcess::adbProcessResult, this,
        [this](qsc::AdbProcess::ADB_EXEC_RESULT processResult) {
            if (processResult == qsc::AdbProcess::AER_SUCCESS_EXEC) {
                QStringList args = m_deviceDetectionAdb.arguments();
                if (args.contains("devices")) {
                    QStringList devices = m_deviceDetectionAdb.getDevicesSerialFromStdOut();
                    processDetectedDevices(devices);
                }
            }
        });
}

FarmViewer::~FarmViewer()
{
    // Cleanup device forms
    for (auto it = m_deviceForms.begin(); it != m_deviceForms.end(); ++it) {
        if (!it.value().isNull()) {
            delete it.value();
        }
    }
    m_deviceForms.clear();
    m_deviceContainers.clear();
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
    
    // Add to toolbar
    m_toolbarLayout->addWidget(m_screenshotAllBtn);
    m_toolbarLayout->addWidget(m_syncActionBtn);
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
    qDebug() << "FarmViewer: Adding device" << serial << deviceName;
    
    // Don't add if already exists
    if (m_deviceForms.contains(serial)) {
        qDebug() << "Device already exists in farm viewer:" << serial;
        return;
    }
    
    // Create VideoForm for this device
    auto videoForm = new VideoForm(true, false, false, this); // frameless, no skin, no toolbar
    videoForm->setSerial(serial);
    videoForm->setMinimumSize(200, 400); // Minimum size for grid display
    videoForm->setMaximumSize(400, 800); // Maximum size for grid display
    
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
    
    // Register VideoForm as device observer to receive video frames
    auto device = qsc::IDeviceManage::getInstance().getDevice(serial);
    if (device) {
        device->registerDeviceObserver(videoForm);
        qDebug() << "FarmViewer: Registered VideoForm as observer for device:" << serial;
    } else {
        qDebug() << "FarmViewer: Warning - Could not get device for serial:" << serial;
    }
    
    // Add to GroupController for synchronized actions
    GroupController::instance().addDevice(serial);
    
    // Update grid layout
    updateGridLayout();
    
    // Update status
    updateStatus();
    
    // Show VideoForm
    videoForm->show();
    
    qDebug() << "FarmViewer: Device added successfully" << serial;
}

void FarmViewer::removeDevice(const QString& serial)
{
    qDebug() << "FarmViewer: Removing device" << serial;
    
    // Remove from GroupController
    GroupController::instance().removeDevice(serial);
    
    // Remove from grid layout
    if (m_deviceContainers.contains(serial) && !m_deviceContainers[serial].isNull()) {
        m_gridLayout->removeWidget(m_deviceContainers[serial]);
        delete m_deviceContainers[serial];
        m_deviceContainers.remove(serial);
    }
    
    // Clean up VideoForm and deregister observer  
    if (m_deviceForms.contains(serial) && !m_deviceForms[serial].isNull()) {
        // Deregister observer before cleanup (following Dialog pattern)
        auto device = qsc::IDeviceManage::getInstance().getDevice(serial);
        if (device) {
            device->deRegisterDeviceObserver(m_deviceForms[serial]);
            qDebug() << "FarmViewer: Deregistered VideoForm observer for device:" << serial;
        }
        
        delete m_deviceForms[serial];
        m_deviceForms.remove(serial);
    }
    
    // Update grid layout
    updateGridLayout();
    
    // Update status
    updateStatus();
    
    qDebug() << "FarmViewer: Device removed successfully" << serial;
}

QWidget* FarmViewer::createDeviceWidget(const QString& serial, const QString& deviceName)
{
    QWidget* container = new QWidget();
    container->setMinimumSize(220, 450);
    container->setMaximumSize(420, 850);
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    QVBoxLayout* layout = new QVBoxLayout(container);
    layout->setContentsMargins(5, 5, 5, 5);
    layout->setSpacing(5);
    
    // Device info label
    QLabel* deviceLabel = new QLabel(QString("%1\n%2").arg(deviceName, serial));
    deviceLabel->setAlignment(Qt::AlignCenter);
    deviceLabel->setStyleSheet("QLabel { color: #333; font-size: 11px; font-weight: bold; "
                              "background-color: #f0f0f0; padding: 5px; border-radius: 3px; }");
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
        m_statusLabel->setText("No devices connected");
        m_statusLabel->setStyleSheet("color: #888; font-size: 12px;");
    } else {
        m_statusLabel->setText(QString("Devices: %1").arg(deviceCount));
        m_statusLabel->setStyleSheet("color: #0a84ff; font-size: 12px; font-weight: bold;");
    }
}

void FarmViewer::setGridSize(int rows, int cols)
{
    m_gridRows = rows;
    m_gridCols = cols;
    updateGridLayout();
}

void FarmViewer::showFarmViewer()
{
    qDebug() << "FarmViewer: Showing Farm Viewer and auto-detecting devices";
    show();
    raise();
    activateWindow();
    
    // Auto-detect and connect to all available devices
    autoDetectAndConnectDevices();
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
    
    qDebug() << "FarmViewer: Executing ADB devices command...";
    m_deviceDetectionAdb.execute("", QStringList() << "devices");
}

void FarmViewer::processDetectedDevices(const QStringList& devices)
{
    qDebug() << "FarmViewer: Processing detected devices:" << devices;
    
    if (devices.isEmpty()) {
        qDebug() << "FarmViewer: No devices detected";
        return;
    }
    
    qDebug() << "FarmViewer: Found" << devices.size() << "devices, connecting...";
    
    // Connect to each detected device using the same approach as Dialog
    for (const QString& serial : devices) {
        qDebug() << "FarmViewer: Connecting to device:" << serial;
        connectToDevice(serial);
    }
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
    
    // Create device parameters (using defaults similar to Dialog)
    qsc::DeviceParams params;
    params.serial = serial;
    params.maxSize = 720; // Farm viewer default size for better performance
    params.bitRate = 4000000; // 4Mbps for good quality/performance balance
    params.maxFps = static_cast<quint32>(Config::getInstance().getMaxFps());
    
    // Assign unique port for each device to avoid conflicts
    static quint16 nextPort = 27183;
    params.localPort = nextPort++;
    qDebug() << "FarmViewer: Assigning port" << params.localPort << "to device" << serial;
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
    
    // Connect the device
    qsc::IDeviceManage::getInstance().connectDevice(params);
    
    qDebug() << "FarmViewer: Connection request sent for device:" << serial;
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