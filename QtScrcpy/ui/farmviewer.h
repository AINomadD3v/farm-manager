#ifndef FARMVIEWER_H
#define FARMVIEWER_H

#include <QWidget>
#include <QGridLayout>
#include <QScrollArea>
#include <QLabel>
#include <QMap>
#include <QPointer>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QSocketNotifier>
#include <QResizeEvent>
#include <QProgressBar>
#include <QTimer>
#include "adbprocess.h"
#include "deviceconnectiontask.h"
#include "performancemonitor.h"
#include "../QtScrcpyCore/src/device/deviceconnectionpool.h"

// Custom QScrollArea that forwards paint events to all viewport widgets
// This prevents Qt from filtering paint events for QOpenGLWidgets outside the visible area
// which was causing devices 26+ to show black/white screens even though data was streaming
class FarmScrollArea : public QScrollArea {
    Q_OBJECT
public:
    explicit FarmScrollArea(QWidget* parent = nullptr) : QScrollArea(parent) {}

protected:
    bool viewportEvent(QEvent* event) override {
        // Forward paint events to viewport - don't filter them!
        // This ensures paintGL() is called for all QOpenGLWidget instances
        // even if they're outside the currently visible scroll area
        if (event->type() == QEvent::Paint) {
            // Propagate paint event to viewport and all child widgets
            if (viewport()) {
                viewport()->update();
            }
            return true;
        }
        return QScrollArea::viewportEvent(event);
    }
};

class VideoForm;

class FarmViewer : public QWidget
{
    Q_OBJECT

public:
    static FarmViewer& instance();
    ~FarmViewer();

    // Public access to singleton instance pointer (for safe checks before instance())
    static FarmViewer* s_instance;

    void addDevice(const QString& serial, const QString& deviceName, const QSize& size);
    void removeDevice(const QString& serial);
    void setGridSize(int rows, int cols);
    void showFarmViewer();
    void hideFarmViewer();
    void autoDetectAndConnectDevices();
    bool isVisible() const;
    bool isManagingDevice(const QString& serial) const;

    // Dynamic grid and quality management
    void calculateOptimalGrid(int deviceCount, const QSize& windowSize);
    qsc::StreamQualityProfile getOptimalStreamSettings(int totalDeviceCount);
    void applyQualityToAllDevices();
    void updateDeviceQuality(const QString& serial, const qsc::StreamQualityProfile& profile);

    static const QString &getServerPath();

    // Signal handling setup (must be called from main before event loop)
    static void setupUnixSignalHandlers();

private slots:
    void onScreenshotAllClicked();
    void onSyncActionClicked();
    void onStreamAllClicked();  // CLICK-TO-STREAM: Handler for Stream All button
    void onGridSizeChanged();

    // Connection management slots
    void onConnectionBatchStarted(int totalDevices);
    void onConnectionBatchProgress(int completed, int total, int failed);
    void onConnectionBatchCompleted(int successful, int failed);

    // Click-to-connect slots
    void onDeviceTileClicked(QString serial);

    // Unix signal handler slot (called via socket notifier)
    void handleUnixSignal();

    // Resource monitoring slots
    void onResourceCheckTimer();
    void onConnectionThrottleTimer();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    explicit FarmViewer(QWidget *parent = nullptr);
    void setupUI();
    void updateGridLayout();
    void updateStatus();
    void createDeviceContainer(const QString& serial, const QString& deviceName);
    QWidget* createDeviceWidget(const QString& serial, const QString& deviceName);
    void connectToDevice(const QString& serial);
    void disconnectDevice(const QString& serial);
    void processDetectedDevices(const QStringList& devices);
    void processConnectionQueue();
    void connectDevicesInBatches(const QStringList& devices, int batchIndex);
    void onConnectionComplete(const QString& serial, bool success);
    bool isDeviceConnected(const QString& serial) const;

    // Helper methods for grid calculation
    QSize getOptimalTileSize(int deviceCount, const QSize& windowSize) const;
    int calculateColumns(int deviceCount, const QSize& windowSize) const;

    // Resource management helpers
    bool checkMemoryAvailable();
    void logResourceUsage(const QString& context);
    int calculateConnectionDelay(int queueSize);
    int getMaxParallelForDeviceCount(int totalDeviceCount);

    // Cleanup and shutdown
    void cleanupAndExit();

    // Unix signal handlers (async-signal-safe)
    static void unixSignalHandler(int signalNumber);
    static void setupSocketPair();

    FarmScrollArea* m_scrollArea;
    QWidget* m_gridWidget;
    QGridLayout* m_gridLayout;
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_toolbarLayout;

    // Device management
    QMap<QString, QPointer<VideoForm>> m_deviceForms;
    QMap<QString, QPointer<QWidget>> m_deviceContainers;

    // Click-to-connect state tracking
    QSet<QString> m_connectedDevices;  // Track which devices are currently connected
    int m_activeConnections;           // Count of active streaming connections
    static const int MAX_CONCURRENT_STREAMS = 200;  // Match DeviceConnectionPool::MAX_CONNECTIONS (supports up to 200 devices)

    // Batch connection state for Phase 1
    int m_batchConnectionIndex;        // Current batch being processed
    int m_batchSize;                   // Size of current batch based on quality tier
    int m_batchDelayMs;                // Delay between batches

    // SIMPLIFIED: No connection queue or resource management in display-only mode
    QTimer* m_connectionThrottleTimer;  // Kept for compatibility (unused)
    QTimer* m_resourceCheckTimer;  // Kept for compatibility (unused)

    // Grid configuration - dynamically calculated
    int m_gridRows;
    int m_gridCols;
    qsc::StreamQualityProfile m_currentQualityProfile;
    qsc::DeviceConnectionPool::QualityTier m_currentQualityTier;

    // Controls
    QPushButton* m_screenshotAllBtn;
    QPushButton* m_syncActionBtn;
    QPushButton* m_streamAllBtn;  // CLICK-TO-STREAM: Button to connect all devices at once
    QLabel* m_statusLabel;
    QProgressBar* m_connectionProgressBar;

    // Device detection
    qsc::AdbProcess m_deviceDetectionAdb;
    QTimer* m_deviceDetectionTimer;  // Periodic device polling

    // Connection state
    bool m_isConnecting;
    bool m_autoDetectionTriggered = false;

    // Unix signal handling using socketpair pattern
    // This is the ONLY async-signal-safe way to handle signals in Qt
    // Signals write to signalFd[0], QSocketNotifier reads from signalFd[1]
    static int s_signalFd[2];
    QSocketNotifier* m_signalNotifier;
    bool m_isShuttingDown;
};

#endif // FARMVIEWER_H
