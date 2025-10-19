#include "deviceconnectiontask.h"
#include "farmviewer.h"
#include <QDebug>
#include <QThread>
#include <QThreadPool>
#include <QRandomGenerator>
#include <QCoreApplication>

// ============================================================================
// DeviceConnectionTask Implementation
// ============================================================================

DeviceConnectionTask::DeviceConnectionTask(const QString& serial,
                                           FarmViewer* farmViewer,
                                           QObject* parent)
    : QObject(parent)
    , m_serial(serial)
    , m_farmViewer(farmViewer)
    , m_state(ConnectionState::Queued)
    , m_retryCount(0)
{
    setAutoDelete(true); // QThreadPool will auto-delete after run()
}

DeviceConnectionTask::~DeviceConnectionTask()
{
    qDebug() << "DeviceConnectionTask destroyed for device:" << m_serial;
}

void DeviceConnectionTask::run()
{
    qDebug() << "DeviceConnectionTask::run() starting for device:" << m_serial
             << "in thread:" << QThread::currentThreadId();

    setState(ConnectionState::Connecting);
    emit connectionProgress(m_serial, QString("Connecting to device %1...").arg(m_serial));

    // Check if already connected
    if (m_farmViewer && m_farmViewer->isManagingDevice(m_serial)) {
        qDebug() << "Device already connected:" << m_serial;
        setState(ConnectionState::Connected);
        emit connectionCompleted(m_serial, true, "");
        return;
    }

    // Create device parameters
    qsc::DeviceParams params = createDeviceParams();

    // Start the connection attempt (non-blocking)
    bool connectStarted = qsc::IDeviceManage::getInstance().connectDevice(params);

    if (!connectStarted) {
        qCritical() << "Failed to start connection for device:" << m_serial;
        setError("Failed to start connection");
        setState(ConnectionState::Failed);
        emit connectionCompleted(m_serial, false, "Failed to start connection");
        return;
    }

    qDebug() << "Connection initiated for device:" << m_serial
             << "- waiting for deviceConnected signal from IDeviceManage";

    // Emit completion immediately - the actual connection result will be handled
    // by FarmViewer's existing signal handlers connected to IDeviceManage::deviceConnected
    emit connectionCompleted(m_serial, true, "");
}

void DeviceConnectionTask::setState(ConnectionState state)
{
    m_state = state;
    emit connectionStateChanged(m_serial, state);
}

void DeviceConnectionTask::setError(const QString& error)
{
    m_lastError = error;
}

qsc::DeviceParams DeviceConnectionTask::createDeviceParams()
{
    qsc::DeviceParams params;
    params.serial = m_serial;
    params.maxSize = 720; // Farm viewer default size for better performance
    params.bitRate = 4000000; // 4Mbps for good quality/performance balance
    params.maxFps = static_cast<quint32>(Config::getInstance().getMaxFps());

    // Assign unique port for each device to avoid conflicts
    // Use a hash-based approach to get consistent ports for same serial
    quint32 hash = qHash(m_serial);
    params.localPort = 27183 + (hash % 10000);
    qDebug() << "DeviceConnectionTask: Assigning port" << params.localPort
             << "to device" << m_serial;

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
    params.serverLocalPath = FarmViewer::getServerPath();
    params.serverRemotePath = Config::getInstance().getServerPath();
    params.pushFilePath = Config::getInstance().getPushFilePath();
    params.serverVersion = Config::getInstance().getServerVersion();
    params.logLevel = Config::getInstance().getLogLevel();
    params.codecOptions = Config::getInstance().getCodecOptions();
    params.codecName = Config::getInstance().getCodecName();
    params.scid = QRandomGenerator::global()->bounded(1, 10000) & 0x7FFFFFFF;

    return params;
}


// ============================================================================
// DeviceConnectionManager Implementation
// ============================================================================

DeviceConnectionManager* DeviceConnectionManager::s_instance = nullptr;

DeviceConnectionManager& DeviceConnectionManager::instance()
{
    if (!s_instance) {
        s_instance = new DeviceConnectionManager();
    }
    return *s_instance;
}

DeviceConnectionManager::DeviceConnectionManager()
    : m_maxParallelConnections(calculateOptimalThreadCount())
    , m_totalDevices(0)
    , m_completedDevices(0)
    , m_failedDevices(0)
{
    updateThreadPoolConfiguration();
    qDebug() << "DeviceConnectionManager initialized with max parallel connections:"
             << m_maxParallelConnections;
}

DeviceConnectionManager::~DeviceConnectionManager()
{
    cancelAllConnections();
}

void DeviceConnectionManager::connectDevices(const QStringList& serials,
                                             FarmViewer* farmViewer)
{
    qDebug() << "DeviceConnectionManager: Starting batch connection for"
             << serials.size() << "devices";

    m_totalDevices = serials.size();
    m_completedDevices = 0;
    m_failedDevices = 0;
    m_connectionStates.clear();

    emit connectionBatchStarted(m_totalDevices);

    // Create and queue connection tasks
    for (const QString& serial : serials) {
        m_connectionStates[serial] = ConnectionState::Queued;

        DeviceConnectionTask* task = new DeviceConnectionTask(serial, farmViewer);

        // Connect signals
        connect(task, &DeviceConnectionTask::connectionStateChanged,
                this, &DeviceConnectionManager::onConnectionStateChanged,
                Qt::QueuedConnection);

        connect(task, &DeviceConnectionTask::connectionCompleted,
                this, &DeviceConnectionManager::onConnectionCompleted,
                Qt::QueuedConnection);

        connect(task, &DeviceConnectionTask::connectionProgress,
                [serial](const QString& msg) {
                    qDebug() << "Connection progress [" << serial << "]:" << msg;
                });

        // Add to thread pool
        QThreadPool::globalInstance()->start(task);
    }

    qDebug() << "DeviceConnectionManager: All tasks queued. Active threads:"
             << QThreadPool::globalInstance()->activeThreadCount();
}

void DeviceConnectionManager::cancelAllConnections()
{
    qDebug() << "DeviceConnectionManager: Cancelling all connections";
    QThreadPool::globalInstance()->clear();
    m_connectionStates.clear();
}

int DeviceConnectionManager::getActiveConnectionCount() const
{
    int count = 0;
    for (auto state : m_connectionStates.values()) {
        if (state == ConnectionState::Connecting || state == ConnectionState::Retrying) {
            count++;
        }
    }
    return count;
}

int DeviceConnectionManager::getQueuedConnectionCount() const
{
    int count = 0;
    for (auto state : m_connectionStates.values()) {
        if (state == ConnectionState::Queued) {
            count++;
        }
    }
    return count;
}

int DeviceConnectionManager::getCompletedConnectionCount() const
{
    return m_completedDevices;
}

int DeviceConnectionManager::getFailedConnectionCount() const
{
    return m_failedDevices;
}

void DeviceConnectionManager::setMaxParallelConnections(int maxConnections)
{
    m_maxParallelConnections = qMax(1, maxConnections);
    updateThreadPoolConfiguration();
}

void DeviceConnectionManager::onConnectionStateChanged(QString serial,
                                                       ConnectionState state)
{
    m_connectionStates[serial] = state;

    QString stateStr;
    switch (state) {
    case ConnectionState::Queued: stateStr = "Queued"; break;
    case ConnectionState::Connecting: stateStr = "Connecting"; break;
    case ConnectionState::Connected: stateStr = "Connected"; break;
    case ConnectionState::Failed: stateStr = "Failed"; break;
    case ConnectionState::Retrying: stateStr = "Retrying"; break;
    }

    qDebug() << "Device" << serial << "state changed to:" << stateStr;
}

void DeviceConnectionManager::onConnectionCompleted(QString serial,
                                                    bool success,
                                                    QString error)
{
    m_completedDevices++;

    if (success) {
        qDebug() << "Device connection succeeded:" << serial;
    } else {
        m_failedDevices++;
        qWarning() << "Device connection failed:" << serial << "Error:" << error;
    }

    // Emit progress
    emit connectionBatchProgress(m_completedDevices, m_totalDevices, m_failedDevices);

    // Check if batch is complete
    if (m_completedDevices >= m_totalDevices) {
        int successful = m_completedDevices - m_failedDevices;
        qDebug() << "DeviceConnectionManager: Batch completed. Successful:"
                 << successful << "Failed:" << m_failedDevices;
        emit connectionBatchCompleted(successful, m_failedDevices);
    }
}

void DeviceConnectionManager::updateThreadPoolConfiguration()
{
    QThreadPool* pool = QThreadPool::globalInstance();
    pool->setMaxThreadCount(m_maxParallelConnections);

    qDebug() << "Thread pool configured:"
             << "Max threads:" << pool->maxThreadCount()
             << "Expiry timeout:" << pool->expiryTimeout() << "ms";
}

int DeviceConnectionManager::calculateOptimalThreadCount()
{
    // Base on CPU cores, but cap for practical limits
    int cpuCores = QThread::idealThreadCount();

    // For device connections, we can be more aggressive since most time
    // is spent waiting on I/O (adb, network), not CPU
    int optimalThreads = cpuCores * 4;

    // Cap at reasonable limits
    optimalThreads = qMax(4, qMin(optimalThreads, 64));

    qDebug() << "Calculated optimal thread count:"
             << "CPU cores:" << cpuCores
             << "Optimal threads:" << optimalThreads;

    return optimalThreads;
}
