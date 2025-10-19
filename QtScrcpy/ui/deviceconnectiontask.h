#ifndef DEVICECONNECTIONTASK_H
#define DEVICECONNECTIONTASK_H

#include <QObject>
#include <QRunnable>
#include <QString>
#include <QPointer>
#include "../util/config.h"
#include "QtScrcpyCore.h"

// Forward declarations
class FarmViewer;

/**
 * @brief Connection state for tracking device connection progress
 */
enum class ConnectionState {
    Queued,      // Waiting in queue
    Connecting,  // Connection in progress
    Connected,   // Successfully connected
    Failed,      // Connection failed
    Retrying     // Retrying after failure
};

/**
 * @brief DeviceConnectionTask - QRunnable task for parallel device connections
 *
 * This class handles individual device connections in a thread pool,
 * enabling parallel initialization of multiple devices.
 */
class DeviceConnectionTask : public QObject, public QRunnable
{
    Q_OBJECT

public:
    explicit DeviceConnectionTask(const QString& serial,
                                  FarmViewer* farmViewer,
                                  QObject* parent = nullptr);
    virtual ~DeviceConnectionTask();

    void run() override;

    QString getSerial() const { return m_serial; }
    ConnectionState getState() const { return m_state; }
    int getRetryCount() const { return m_retryCount; }
    QString getLastError() const { return m_lastError; }

signals:
    void connectionStateChanged(QString serial, ConnectionState state);
    void connectionProgress(QString serial, QString message);
    void connectionCompleted(QString serial, bool success, QString error);

private:
    void setState(ConnectionState state);
    void setError(const QString& error);
    qsc::DeviceParams createDeviceParams();

private:
    QString m_serial;
    QPointer<FarmViewer> m_farmViewer;
    ConnectionState m_state;
    int m_retryCount;
    QString m_lastError;
};

/**
 * @brief DeviceConnectionManager - Manages parallel device connections
 *
 * Coordinates the thread pool and tracks connection states for all devices.
 */
class DeviceConnectionManager : public QObject
{
    Q_OBJECT

public:
    static DeviceConnectionManager& instance();

    void connectDevices(const QStringList& serials, FarmViewer* farmViewer);
    void cancelAllConnections();

    int getActiveConnectionCount() const;
    int getQueuedConnectionCount() const;
    int getCompletedConnectionCount() const;
    int getFailedConnectionCount() const;

    void setMaxParallelConnections(int maxConnections);
    int getMaxParallelConnections() const { return m_maxParallelConnections; }

signals:
    void connectionBatchStarted(int totalDevices);
    void connectionBatchProgress(int completed, int total, int failed);
    void connectionBatchCompleted(int successful, int failed);

private slots:
    void onConnectionStateChanged(QString serial, ConnectionState state);
    void onConnectionCompleted(QString serial, bool success, QString error);

private:
    DeviceConnectionManager();
    ~DeviceConnectionManager();

    void updateThreadPoolConfiguration();
    int calculateOptimalThreadCount();

private:
    static DeviceConnectionManager* s_instance;

    QMap<QString, ConnectionState> m_connectionStates;
    int m_maxParallelConnections;
    int m_totalDevices;
    int m_completedDevices;
    int m_failedDevices;
};

#endif // DEVICECONNECTIONTASK_H
