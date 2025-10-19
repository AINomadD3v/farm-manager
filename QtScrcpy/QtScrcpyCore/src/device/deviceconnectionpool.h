#ifndef DEVICECONNECTIONPOOL_H
#define DEVICECONNECTIONPOOL_H

#include <QObject>
#include <QMap>
#include <QMutex>
#include <QMutexLocker>
#include <QTimer>
#include <QElapsedTimer>
#include <QSharedPointer>
#include "../../include/QtScrcpyCore.h"

namespace qsc {

// Forward declarations
class Device;

// Connection pool entry
struct PooledConnection {
    QSharedPointer<Device> device;
    QString serial;
    DeviceParams params;
    QElapsedTimer lastUsedTime;
    bool inUse;
    quint64 usageCount;

    PooledConnection()
        : inUse(false)
        , usageCount(0)
    {
        lastUsedTime.start();
    }
};

// Stream quality profile for adaptive quality management
struct StreamQualityProfile {
    quint16 maxSize;        // Resolution (max dimension)
    quint32 bitRate;        // Bits per second
    quint32 maxFps;         // Frames per second
    QString description;    // Human-readable description

    StreamQualityProfile(quint16 size = 720, quint32 rate = 4000000, quint32 fps = 30, const QString& desc = "Medium")
        : maxSize(size), bitRate(rate), maxFps(fps), description(desc) {}
};

/**
 * DeviceConnectionPool - Singleton connection pool for managing device connections
 *
 * Features:
 * - Connection reuse (don't recreate connections for same serial)
 * - Idle timeout management (5 minutes default)
 * - Max connection limit (200 default)
 * - LRU eviction for old connections
 * - Thread-safe operations with QMutex
 * - Adaptive quality profiles based on device count
 */
class DeviceConnectionPool : public QObject
{
    Q_OBJECT

public:
    // Singleton access
    static DeviceConnectionPool& instance();

    // Destructor
    ~DeviceConnectionPool();

    // Configuration constants
    static const int MAX_CONNECTIONS = 200;
    static const int IDLE_TIMEOUT_MS = 5 * 60 * 1000; // 5 minutes
    static const int CLEANUP_INTERVAL_MS = 60 * 1000;  // 1 minute

    // Quality tiers based on device count
    enum QualityTier {
        TIER_ULTRA,     // 1-5 devices: 1080p, 8Mbps, 60fps
        TIER_HIGH,      // 6-20 devices: 720p, 4Mbps, 30fps
        TIER_MEDIUM,    // 21-50 devices: 480p, 2Mbps, 30fps
        TIER_LOW,       // 51-100 devices: 360p, 1Mbps, 15fps
        TIER_MINIMAL    // 100+ devices: 240p, 500Kbps, 10fps
    };

    // Connection management
    QSharedPointer<Device> acquireConnection(const DeviceParams& params);
    void releaseConnection(const QString& serial);
    void removeConnection(const QString& serial);
    void cleanup();

    // Quality management
    StreamQualityProfile getOptimalStreamSettings(int totalDeviceCount);
    QualityTier getQualityTier(int deviceCount);
    void applyQualityProfile(DeviceParams& params, const StreamQualityProfile& profile);

    // Statistics and monitoring
    int getActiveConnectionCount() const;
    int getTotalConnectionCount() const;
    int getIdleConnectionCount() const;
    quint64 getTotalUsageCount() const;
    QStringList getActiveSerials() const;

    // Resource limits
    bool canAcquireNewConnection() const;
    void setMaxConnections(int max);
    void setIdleTimeout(int timeoutMs);

signals:
    void connectionAcquired(const QString& serial);
    void connectionReleased(const QString& serial);
    void connectionRemoved(const QString& serial);
    void connectionLimitReached();
    void memoryWarning(quint64 estimatedBytes);

private slots:
    void onCleanupTimer();

private:
    explicit DeviceConnectionPool(QObject* parent = nullptr);

    // Prevent copying
    DeviceConnectionPool(const DeviceConnectionPool&) = delete;
    DeviceConnectionPool& operator=(const DeviceConnectionPool&) = delete;

    // Internal helpers
    void evictLRUConnection();
    bool isConnectionIdle(const PooledConnection& conn) const;
    quint64 estimateMemoryUsage() const;

    // Data members
    QMap<QString, QSharedPointer<PooledConnection>> m_connections;
    mutable QMutex m_mutex;
    QTimer* m_cleanupTimer;
    int m_maxConnections;
    int m_idleTimeoutMs;

    static DeviceConnectionPool* s_instance;
};

} // namespace qsc

#endif // DEVICECONNECTIONPOOL_H
