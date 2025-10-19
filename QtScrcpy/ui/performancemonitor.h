#ifndef PERFORMANCEMONITOR_H
#define PERFORMANCEMONITOR_H

#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QGroupBox>

/**
 * @brief Performance monitoring widget for Farm Manager
 *
 * Displays real-time performance metrics:
 * - CPU usage (total and per-device average)
 * - Memory usage (total and per-device average)
 * - Network bandwidth (total Mbps)
 * - Device count (active connections)
 * - FPS (average across all devices)
 * - Current quality tier
 */
class PerformanceMonitor : public QWidget
{
    Q_OBJECT

public:
    explicit PerformanceMonitor(QWidget *parent = nullptr);
    ~PerformanceMonitor();

    // Update metrics
    void updateDeviceCount(int activeDevices, int totalDevices);
    void updateCpuUsage(double percent);
    void updateMemoryUsage(quint64 bytesUsed, quint64 bytesTotal);
    void updateNetworkBandwidth(quint64 bytesPerSecond);
    void updateAverageFps(double fps);
    void updateQualityTier(const QString& tierName);

    // System resource queries
    double getSystemMemoryAvailablePercent();
    quint64 getSystemMemoryAvailable();
    quint64 getSystemMemoryTotal();
    bool isMemoryPressureHigh();  // Returns true if available memory < 20%

    // Auto-refresh control
    void startAutoRefresh(int intervalMs = 1000);
    void stopAutoRefresh();

private slots:
    void refreshMetrics();

private:
    void setupUI();
    QString formatBytes(quint64 bytes) const;
    QString formatBandwidth(quint64 bytesPerSecond) const;
    double getCpuUsage();
    quint64 getCurrentMemoryUsage();
    void readSystemMemoryInfo(quint64& totalMem, quint64& availableMem);

    // UI Components
    QGroupBox* m_deviceGroup;
    QGroupBox* m_systemGroup;
    QGroupBox* m_networkGroup;
    QGroupBox* m_qualityGroup;

    QLabel* m_deviceCountLabel;
    QLabel* m_cpuUsageLabel;
    QLabel* m_memoryUsageLabel;
    QLabel* m_bandwidthLabel;
    QLabel* m_avgFpsLabel;
    QLabel* m_qualityTierLabel;

    // Auto-refresh
    QTimer* m_refreshTimer;

    // Metrics
    int m_activeDevices;
    int m_totalDevices;
    double m_cpuPercent;
    quint64 m_memoryUsed;
    quint64 m_memoryTotal;
    quint64 m_bandwidth;
    double m_avgFps;
    QString m_qualityTier;
};

#endif // PERFORMANCEMONITOR_H
