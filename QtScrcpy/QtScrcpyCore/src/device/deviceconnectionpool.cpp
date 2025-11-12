#include "deviceconnectionpool.h"
#include "device.h"
#include <QDebug>
#include <algorithm>

namespace qsc {

DeviceConnectionPool* DeviceConnectionPool::s_instance = nullptr;

DeviceConnectionPool::DeviceConnectionPool(QObject* parent)
    : QObject(parent)
    , m_cleanupTimer(nullptr)
    , m_maxConnections(MAX_CONNECTIONS)
    , m_idleTimeoutMs(IDLE_TIMEOUT_MS)
{
    qDebug() << "DeviceConnectionPool: Initializing with max connections:" << m_maxConnections;

    // Setup cleanup timer
    m_cleanupTimer = new QTimer(this);
    m_cleanupTimer->setInterval(CLEANUP_INTERVAL_MS);
    connect(m_cleanupTimer, &QTimer::timeout, this, &DeviceConnectionPool::onCleanupTimer);
    m_cleanupTimer->start();
}

DeviceConnectionPool::~DeviceConnectionPool()
{
    QMutexLocker locker(&m_mutex);
    qDebug() << "DeviceConnectionPool: Shutting down, cleaning up" << m_connections.size() << "connections";

    if (m_cleanupTimer) {
        m_cleanupTimer->stop();
    }

    m_connections.clear();
}

DeviceConnectionPool& DeviceConnectionPool::instance()
{
    if (!s_instance) {
        s_instance = new DeviceConnectionPool();
    }
    return *s_instance;
}

QSharedPointer<Device> DeviceConnectionPool::acquireConnection(const DeviceParams& params)
{
    QMutexLocker locker(&m_mutex);

    const QString& serial = params.serial;
    qDebug() << "DeviceConnectionPool: Acquiring connection for device:" << serial;

    // Check if connection already exists and can be reused
    if (m_connections.contains(serial)) {
        auto pooledConn = m_connections[serial];

        if (!pooledConn->inUse) {
            // Reuse existing idle connection
            pooledConn->inUse = true;
            pooledConn->usageCount++;
            pooledConn->lastUsedTime.restart();
            pooledConn->params = params; // Update params in case they changed

            qDebug() << "DeviceConnectionPool: Reusing existing connection for" << serial
                     << "usage count:" << pooledConn->usageCount;

            emit connectionAcquired(serial);
            return pooledConn->device;
        } else {
            qWarning() << "DeviceConnectionPool: Connection already in use for" << serial;
            return pooledConn->device; // Return existing, even if in use
        }
    }

    // Check connection limit
    if (!canAcquireNewConnection()) {
        qWarning() << "DeviceConnectionPool: Connection limit reached (" << m_maxConnections
                   << "), evicting LRU connection";
        evictLRUConnection();
        emit connectionLimitReached();
    }

    // Create new connection
    qInfo() << "========================================";
    qInfo() << "DeviceConnectionPool: Creating new connection for" << serial;
    qInfo() << "  Port:" << params.localPort;
    qInfo() << "  Resolution:" << params.maxSize;
    qInfo() << "  Bitrate:" << params.bitRate;
    qInfo() << "========================================";

    qInfo() << "DeviceConnectionPool: Creating QSharedPointer<Device>...";
    QSharedPointer<Device> device;
    try {
        device = QSharedPointer<Device>::create(params);
        qInfo() << "DeviceConnectionPool: Device created successfully";
    } catch (const std::exception& e) {
        qCritical() << "DeviceConnectionPool: EXCEPTION creating Device:" << e.what();
        emit connectionFailed(serial);
        return QSharedPointer<Device>();
    } catch (...) {
        qCritical() << "DeviceConnectionPool: UNKNOWN EXCEPTION creating Device";
        emit connectionFailed(serial);
        return QSharedPointer<Device>();
    }

    if (!device) {
        qCritical() << "DeviceConnectionPool: Device pointer is null after creation!";
        emit connectionFailed(serial);
        return QSharedPointer<Device>();
    }

    qInfo() << "DeviceConnectionPool: Creating PooledConnection...";
    auto pooledConn = QSharedPointer<PooledConnection>::create();

    pooledConn->device = device;
    pooledConn->serial = serial;
    pooledConn->params = params;
    pooledConn->inUse = true;
    pooledConn->usageCount = 1;
    pooledConn->lastUsedTime.start();

    qInfo() << "DeviceConnectionPool: Adding to connection pool...";
    m_connections[serial] = pooledConn;

    qInfo() << "DeviceConnectionPool: New connection created successfully!";
    qInfo() << "  Total connections:" << m_connections.size();
    // Don't call getActiveConnectionCount() here - it would cause deadlock (mutex already locked)
    qInfo() << "  Connection is now active (inUse=true)";
    qInfo() << "========================================";

    qInfo() << "DeviceConnectionPool: About to emit connectionAcquired signal...";
    emit connectionAcquired(serial);
    qInfo() << "DeviceConnectionPool: connectionAcquired signal emitted successfully";

    // Check memory usage
    qInfo() << "DeviceConnectionPool: Checking memory usage...";
    quint64 memUsage = estimateMemoryUsage();
    qInfo() << "DeviceConnectionPool: Memory usage:" << (memUsage / 1024 / 1024) << "MB";
    if (memUsage > 500 * 1024 * 1024) { // Warn if over 500MB
        qWarning() << "DeviceConnectionPool: High memory usage detected:"
                   << (memUsage / 1024 / 1024) << "MB";
        emit memoryWarning(memUsage);
    }

    qInfo() << "DeviceConnectionPool: Returning device pointer...";
    return device;
}

void DeviceConnectionPool::releaseConnection(const QString& serial)
{
    QMutexLocker locker(&m_mutex);

    if (!m_connections.contains(serial)) {
        qWarning() << "DeviceConnectionPool: Cannot release non-existent connection:" << serial;
        return;
    }

    auto pooledConn = m_connections[serial];
    pooledConn->inUse = false;
    pooledConn->lastUsedTime.restart();

    qDebug() << "DeviceConnectionPool: Released connection for" << serial;
    // Don't call getActiveConnectionCount() here - mutex already locked

    emit connectionReleased(serial);
}

void DeviceConnectionPool::removeConnection(const QString& serial)
{
    QMutexLocker locker(&m_mutex);

    if (!m_connections.contains(serial)) {
        qDebug() << "DeviceConnectionPool: Connection not found for removal:" << serial;
        return;
    }

    qDebug() << "DeviceConnectionPool: Removing connection for" << serial;
    m_connections.remove(serial);

    emit connectionRemoved(serial);
}

void DeviceConnectionPool::cleanup()
{
    QMutexLocker locker(&m_mutex);

    QStringList toRemove;

    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        if (isConnectionIdle(*it.value())) {
            toRemove.append(it.key());
        }
    }

    if (!toRemove.isEmpty()) {
        qDebug() << "DeviceConnectionPool: Cleaning up" << toRemove.size() << "idle connections";

        for (const QString& serial : toRemove) {
            m_connections.remove(serial);
            emit connectionRemoved(serial);
        }
    }
}

StreamQualityProfile DeviceConnectionPool::getOptimalStreamSettings(int totalDeviceCount)
{
    QualityTier tier = getQualityTier(totalDeviceCount);

    switch (tier) {
    case TIER_ULTRA:
        return StreamQualityProfile(1080, 8000000, 60, "Ultra (1-5 devices)");
    case TIER_HIGH:
        return StreamQualityProfile(720, 4000000, 30, "High (6-20 devices)");
    case TIER_MEDIUM:
        // Aggressive scaling: reduced from 480p to 360p for improved stability
        return StreamQualityProfile(360, 1500000, 20, "Medium (21-50 devices - Stability focused)");
    case TIER_LOW:
        // Aggressive scaling: reduced from 360p to 240p for 51-100 devices (e.g., 96 device deployments)
        // This significantly reduces memory/GPU load while maintaining device monitoring capability
        return StreamQualityProfile(240, 800000, 15, "Low (51-100 devices - Maximum stability)");
    case TIER_MINIMAL:
        // Aggressive scaling: reduced from 240p to 180p for 100+ devices
        // Minimal bitrate and FPS for extreme multi-device scenarios
        return StreamQualityProfile(180, 400000, 10, "Minimal (100+ devices - Maximum stability)");
    default:
        return StreamQualityProfile(720, 4000000, 30, "Default");
    }
}

DeviceConnectionPool::QualityTier DeviceConnectionPool::getQualityTier(int deviceCount)
{
    if (deviceCount <= 5) {
        return TIER_ULTRA;
    } else if (deviceCount <= 20) {
        return TIER_HIGH;
    } else if (deviceCount <= 50) {
        return TIER_MEDIUM;
    } else if (deviceCount <= 100) {
        return TIER_LOW;
    } else {
        return TIER_MINIMAL;
    }
}

void DeviceConnectionPool::applyQualityProfile(DeviceParams& params, const StreamQualityProfile& profile)
{
    params.maxSize = profile.maxSize;
    params.bitRate = profile.bitRate;
    params.maxFps = profile.maxFps;

    qDebug() << "DeviceConnectionPool: Applied quality profile:" << profile.description
             << "Resolution:" << profile.maxSize
             << "Bitrate:" << (profile.bitRate / 1000000.0) << "Mbps"
             << "FPS:" << profile.maxFps;
}

int DeviceConnectionPool::getActiveConnectionCount() const
{
    QMutexLocker locker(&m_mutex);

    int count = 0;
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        if (it.value()->inUse) {
            count++;
        }
    }
    return count;
}

int DeviceConnectionPool::getTotalConnectionCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_connections.size();
}

int DeviceConnectionPool::getIdleConnectionCount() const
{
    QMutexLocker locker(&m_mutex);

    int count = 0;
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        if (!it.value()->inUse) {
            count++;
        }
    }
    return count;
}

quint64 DeviceConnectionPool::getTotalUsageCount() const
{
    QMutexLocker locker(&m_mutex);

    quint64 total = 0;
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        total += it.value()->usageCount;
    }
    return total;
}

QStringList DeviceConnectionPool::getActiveSerials() const
{
    QMutexLocker locker(&m_mutex);

    QStringList serials;
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        if (it.value()->inUse) {
            serials.append(it.key());
        }
    }
    return serials;
}

bool DeviceConnectionPool::canAcquireNewConnection() const
{
    // Mutex must already be held by caller - this is an internal helper method
    // DO NOT lock here to avoid recursive deadlock
    return m_connections.size() < m_maxConnections;
}

void DeviceConnectionPool::setMaxConnections(int max)
{
    QMutexLocker locker(&m_mutex);
    qDebug() << "DeviceConnectionPool: Setting max connections to" << max;
    m_maxConnections = max;
}

void DeviceConnectionPool::setIdleTimeout(int timeoutMs)
{
    QMutexLocker locker(&m_mutex);
    qDebug() << "DeviceConnectionPool: Setting idle timeout to" << timeoutMs << "ms";
    m_idleTimeoutMs = timeoutMs;
}

void DeviceConnectionPool::onCleanupTimer()
{
    cleanup();
}

void DeviceConnectionPool::evictLRUConnection()
{
    // Must be called with mutex locked

    if (m_connections.isEmpty()) {
        return;
    }

    // Find the least recently used idle connection
    QString lruSerial;
    qint64 oldestTime = 0;

    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        if (!it.value()->inUse) {
            qint64 elapsed = it.value()->lastUsedTime.elapsed();
            if (elapsed > oldestTime) {
                oldestTime = elapsed;
                lruSerial = it.key();
            }
        }
    }

    if (!lruSerial.isEmpty()) {
        qDebug() << "DeviceConnectionPool: Evicting LRU connection:" << lruSerial
                 << "idle time:" << (oldestTime / 1000) << "seconds";
        m_connections.remove(lruSerial);
        emit connectionRemoved(lruSerial);
    } else {
        qWarning() << "DeviceConnectionPool: No idle connections to evict, all connections are active";
    }
}

bool DeviceConnectionPool::isConnectionIdle(const PooledConnection& conn) const
{
    // Must be called with mutex locked
    return !conn.inUse && conn.lastUsedTime.elapsed() > m_idleTimeoutMs;
}

quint64 DeviceConnectionPool::estimateMemoryUsage() const
{
    // Must be called with mutex locked

    // Rough estimate: Each connection uses approximately 2-5MB
    // (video buffers, decoder state, network buffers, etc.)
    const quint64 BYTES_PER_CONNECTION = 3 * 1024 * 1024; // 3MB average

    return m_connections.size() * BYTES_PER_CONNECTION;
}

} // namespace qsc
