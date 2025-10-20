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
#include "performancemonitor.h"
#include "../QtScrcpyCore/src/device/deviceconnectionpool.h"

class VideoForm;

class FarmViewer : public QWidget
{
    Q_OBJECT

public:
    static FarmViewer& instance();
    ~FarmViewer();

    // Public access to singleton instance pointer (for safe checks before instance())
    static FarmViewer* s_instance;

    // Display-only methods (no connection logic)
    void addDevice(const QString& serial, const QString& deviceName, const QSize& size);
    void removeDevice(const QString& serial);
    void setGridSize(int rows, int cols);
    void showFarmViewer();
    void hideFarmViewer();
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
    void onGridSizeChanged();

    // Unix signal handler slot (called via socket notifier)
    void handleUnixSignal();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    explicit FarmViewer(QWidget *parent = nullptr);
    void setupUI();
    void updateGridLayout();
    void updateStatus();
    QWidget* createDeviceWidget(const QString& serial, const QString& deviceName);

    // Helper methods for grid calculation
    QSize getOptimalTileSize(int deviceCount, const QSize& windowSize) const;
    int calculateColumns(int deviceCount, const QSize& windowSize) const;

    // Cleanup and shutdown
    void cleanupAndExit();

    // Unix signal handlers (async-signal-safe)
    static void unixSignalHandler(int signalNumber);
    static void setupSocketPair();

    QScrollArea* m_scrollArea;
    QWidget* m_gridWidget;
    QGridLayout* m_gridLayout;
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_toolbarLayout;

    // Device management (DISPLAY ONLY - no connection logic)
    QMap<QString, QPointer<VideoForm>> m_deviceForms;
    QMap<QString, QPointer<QWidget>> m_deviceContainers;

    // Grid configuration - dynamically calculated
    int m_gridRows;
    int m_gridCols;
    qsc::StreamQualityProfile m_currentQualityProfile;
    qsc::DeviceConnectionPool::QualityTier m_currentQualityTier;

    // Controls
    QPushButton* m_screenshotAllBtn;
    QPushButton* m_syncActionBtn;
    QLabel* m_statusLabel;

    // Unix signal handling using socketpair pattern
    // This is the ONLY async-signal-safe way to handle signals in Qt
    // Signals write to signalFd[0], QSocketNotifier reads from signalFd[1]
    static int s_signalFd[2];
    QSocketNotifier* m_signalNotifier;
    bool m_isShuttingDown;
};

#endif // FARMVIEWER_H
