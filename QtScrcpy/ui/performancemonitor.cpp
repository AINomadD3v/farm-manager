#include "performancemonitor.h"
#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QDebug>
#include <QRegularExpression>

PerformanceMonitor::PerformanceMonitor(QWidget *parent)
    : QWidget(parent)
    , m_deviceGroup(nullptr)
    , m_systemGroup(nullptr)
    , m_networkGroup(nullptr)
    , m_qualityGroup(nullptr)
    , m_deviceCountLabel(nullptr)
    , m_cpuUsageLabel(nullptr)
    , m_memoryUsageLabel(nullptr)
    , m_bandwidthLabel(nullptr)
    , m_avgFpsLabel(nullptr)
    , m_qualityTierLabel(nullptr)
    , m_refreshTimer(nullptr)
    , m_activeDevices(0)
    , m_totalDevices(0)
    , m_cpuPercent(0.0)
    , m_memoryUsed(0)
    , m_memoryTotal(0)
    , m_bandwidth(0)
    , m_avgFps(0.0)
    , m_qualityTier("Unknown")
{
    setupUI();

    // Create auto-refresh timer
    m_refreshTimer = new QTimer(this);
    connect(m_refreshTimer, &QTimer::timeout, this, &PerformanceMonitor::refreshMetrics);
}

PerformanceMonitor::~PerformanceMonitor()
{
    stopAutoRefresh();
}

void PerformanceMonitor::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    // Device Group
    m_deviceGroup = new QGroupBox("Devices", this);
    QGridLayout* deviceLayout = new QGridLayout(m_deviceGroup);

    deviceLayout->addWidget(new QLabel("Active:"), 0, 0);
    m_deviceCountLabel = new QLabel("0 / 0");
    m_deviceCountLabel->setStyleSheet("font-weight: bold; color: #2196f3;");
    deviceLayout->addWidget(m_deviceCountLabel, 0, 1);

    deviceLayout->addWidget(new QLabel("Avg FPS:"), 1, 0);
    m_avgFpsLabel = new QLabel("0.0");
    m_avgFpsLabel->setStyleSheet("font-weight: bold;");
    deviceLayout->addWidget(m_avgFpsLabel, 1, 1);

    // System Group
    m_systemGroup = new QGroupBox("System", this);
    QGridLayout* systemLayout = new QGridLayout(m_systemGroup);

    systemLayout->addWidget(new QLabel("CPU:"), 0, 0);
    m_cpuUsageLabel = new QLabel("0.0%");
    m_cpuUsageLabel->setStyleSheet("font-weight: bold;");
    systemLayout->addWidget(m_cpuUsageLabel, 0, 1);

    systemLayout->addWidget(new QLabel("Memory:"), 1, 0);
    m_memoryUsageLabel = new QLabel("0 MB");
    m_memoryUsageLabel->setStyleSheet("font-weight: bold;");
    systemLayout->addWidget(m_memoryUsageLabel, 1, 1);

    // Network Group
    m_networkGroup = new QGroupBox("Network", this);
    QGridLayout* networkLayout = new QGridLayout(m_networkGroup);

    networkLayout->addWidget(new QLabel("Bandwidth:"), 0, 0);
    m_bandwidthLabel = new QLabel("0 Mbps");
    m_bandwidthLabel->setStyleSheet("font-weight: bold; color: #4caf50;");
    networkLayout->addWidget(m_bandwidthLabel, 0, 1);

    // Quality Group
    m_qualityGroup = new QGroupBox("Stream Quality", this);
    QGridLayout* qualityLayout = new QGridLayout(m_qualityGroup);

    qualityLayout->addWidget(new QLabel("Tier:"), 0, 0);
    m_qualityTierLabel = new QLabel("Unknown");
    m_qualityTierLabel->setStyleSheet("font-weight: bold; color: #ff9800;");
    qualityLayout->addWidget(m_qualityTierLabel, 0, 1);

    // Add all groups to main layout
    mainLayout->addWidget(m_deviceGroup);
    mainLayout->addWidget(m_systemGroup);
    mainLayout->addWidget(m_networkGroup);
    mainLayout->addWidget(m_qualityGroup);
    mainLayout->addStretch();

    setLayout(mainLayout);
    setMinimumWidth(200);
}

void PerformanceMonitor::updateDeviceCount(int activeDevices, int totalDevices)
{
    m_activeDevices = activeDevices;
    m_totalDevices = totalDevices;
    m_deviceCountLabel->setText(QString("%1 / %2").arg(activeDevices).arg(totalDevices));

    // Color coding
    if (activeDevices == 0) {
        m_deviceCountLabel->setStyleSheet("font-weight: bold; color: #9e9e9e;");
    } else if (activeDevices < 20) {
        m_deviceCountLabel->setStyleSheet("font-weight: bold; color: #4caf50;");
    } else if (activeDevices < 50) {
        m_deviceCountLabel->setStyleSheet("font-weight: bold; color: #ff9800;");
    } else {
        m_deviceCountLabel->setStyleSheet("font-weight: bold; color: #f44336;");
    }
}

void PerformanceMonitor::updateCpuUsage(double percent)
{
    m_cpuPercent = percent;
    m_cpuUsageLabel->setText(QString("%1%").arg(percent, 0, 'f', 1));

    // Color coding based on usage
    if (percent < 50) {
        m_cpuUsageLabel->setStyleSheet("font-weight: bold; color: #4caf50;");
    } else if (percent < 80) {
        m_cpuUsageLabel->setStyleSheet("font-weight: bold; color: #ff9800;");
    } else {
        m_cpuUsageLabel->setStyleSheet("font-weight: bold; color: #f44336;");
    }
}

void PerformanceMonitor::updateMemoryUsage(quint64 bytesUsed, quint64 bytesTotal)
{
    m_memoryUsed = bytesUsed;
    m_memoryTotal = bytesTotal;

    QString memText = formatBytes(bytesUsed);
    if (bytesTotal > 0) {
        double percent = (bytesUsed * 100.0) / bytesTotal;
        memText += QString(" (%1%)").arg(percent, 0, 'f', 1);
    }

    m_memoryUsageLabel->setText(memText);
}

void PerformanceMonitor::updateNetworkBandwidth(quint64 bytesPerSecond)
{
    m_bandwidth = bytesPerSecond;
    m_bandwidthLabel->setText(formatBandwidth(bytesPerSecond));

    // Color coding based on bandwidth
    quint64 mbps = (bytesPerSecond * 8) / 1000000;
    if (mbps < 50) {
        m_bandwidthLabel->setStyleSheet("font-weight: bold; color: #4caf50;");
    } else if (mbps < 100) {
        m_bandwidthLabel->setStyleSheet("font-weight: bold; color: #ff9800;");
    } else {
        m_bandwidthLabel->setStyleSheet("font-weight: bold; color: #f44336;");
    }
}

void PerformanceMonitor::updateAverageFps(double fps)
{
    m_avgFps = fps;
    m_avgFpsLabel->setText(QString("%1").arg(fps, 0, 'f', 1));

    // Color coding based on FPS
    if (fps >= 30) {
        m_avgFpsLabel->setStyleSheet("font-weight: bold; color: #4caf50;");
    } else if (fps >= 15) {
        m_avgFpsLabel->setStyleSheet("font-weight: bold; color: #ff9800;");
    } else {
        m_avgFpsLabel->setStyleSheet("font-weight: bold; color: #f44336;");
    }
}

void PerformanceMonitor::updateQualityTier(const QString& tierName)
{
    m_qualityTier = tierName;
    m_qualityTierLabel->setText(tierName);

    // Color coding by tier
    if (tierName == "Ultra") {
        m_qualityTierLabel->setStyleSheet("font-weight: bold; color: #9c27b0;");
    } else if (tierName == "High") {
        m_qualityTierLabel->setStyleSheet("font-weight: bold; color: #2196f3;");
    } else if (tierName == "Medium") {
        m_qualityTierLabel->setStyleSheet("font-weight: bold; color: #ff9800;");
    } else {
        m_qualityTierLabel->setStyleSheet("font-weight: bold; color: #f44336;");
    }
}

void PerformanceMonitor::startAutoRefresh(int intervalMs)
{
    m_refreshTimer->start(intervalMs);
}

void PerformanceMonitor::stopAutoRefresh()
{
    if (m_refreshTimer) {
        m_refreshTimer->stop();
    }
}

void PerformanceMonitor::refreshMetrics()
{
    // Auto-refresh CPU and memory
    double cpu = getCpuUsage();
    if (cpu >= 0) {
        updateCpuUsage(cpu);
    }

    quint64 mem = getCurrentMemoryUsage();
    if (mem > 0) {
        updateMemoryUsage(mem, 0);  // Total is optional
    }
}

QString PerformanceMonitor::formatBytes(quint64 bytes) const
{
    const quint64 KB = 1024;
    const quint64 MB = KB * 1024;
    const quint64 GB = MB * 1024;

    if (bytes >= GB) {
        return QString("%1 GB").arg(bytes / (double)GB, 0, 'f', 2);
    } else if (bytes >= MB) {
        return QString("%1 MB").arg(bytes / (double)MB, 0, 'f', 1);
    } else if (bytes >= KB) {
        return QString("%1 KB").arg(bytes / (double)KB, 0, 'f', 1);
    } else {
        return QString("%1 B").arg(bytes);
    }
}

QString PerformanceMonitor::formatBandwidth(quint64 bytesPerSecond) const
{
    // Convert to Mbps (megabits per second)
    double mbps = (bytesPerSecond * 8.0) / 1000000.0;

    if (mbps >= 1000) {
        return QString("%1 Gbps").arg(mbps / 1000.0, 0, 'f', 2);
    } else if (mbps >= 1) {
        return QString("%1 Mbps").arg(mbps, 0, 'f', 1);
    } else {
        return QString("%1 Kbps").arg(mbps * 1000.0, 0, 'f', 0);
    }
}

double PerformanceMonitor::getCpuUsage()
{
#ifdef Q_OS_LINUX
    // Read /proc/stat for Linux
    static quint64 lastTotal = 0;
    static quint64 lastIdle = 0;

    QFile file("/proc/stat");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return -1.0;
    }

    QTextStream in(&file);
    QString line = in.readLine();
    file.close();

    if (!line.startsWith("cpu ")) {
        return -1.0;
    }

    QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (parts.size() < 5) {
        return -1.0;
    }

    quint64 user = parts[1].toULongLong();
    quint64 nice = parts[2].toULongLong();
    quint64 system = parts[3].toULongLong();
    quint64 idle = parts[4].toULongLong();

    quint64 total = user + nice + system + idle;

    if (lastTotal == 0) {
        lastTotal = total;
        lastIdle = idle;
        return 0.0;
    }

    quint64 totalDiff = total - lastTotal;
    quint64 idleDiff = idle - lastIdle;

    double cpuPercent = 0.0;
    if (totalDiff > 0) {
        cpuPercent = 100.0 * (totalDiff - idleDiff) / totalDiff;
    }

    lastTotal = total;
    lastIdle = idle;

    return cpuPercent;
#else
    // Fallback: use QProcess to get top/ps output
    return -1.0;  // Not implemented for other platforms
#endif
}

quint64 PerformanceMonitor::getCurrentMemoryUsage()
{
#ifdef Q_OS_LINUX
    // Read /proc/self/status for Linux
    QFile file("/proc/self/status");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return 0;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.startsWith("VmRSS:")) {
            QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                quint64 kb = parts[1].toULongLong();
                file.close();
                return kb * 1024;  // Convert KB to bytes
            }
        }
    }
    file.close();
#endif
    return 0;
}

void PerformanceMonitor::readSystemMemoryInfo(quint64& totalMem, quint64& availableMem)
{
    totalMem = 0;
    availableMem = 0;

#ifdef Q_OS_LINUX
    // Read /proc/meminfo for system-wide memory stats
    QFile file("/proc/meminfo");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "PerformanceMonitor: Failed to open /proc/meminfo";
        return;
    }

    QTextStream in(&file);
    quint64 memFree = 0;
    quint64 buffers = 0;
    quint64 cached = 0;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        // Split by whitespace and colons
        QStringList parts = line.split(QRegularExpression("[:\\s]+"), Qt::SkipEmptyParts);

        if (parts.size() >= 2) {
            QString key = parts[0];
            quint64 value = parts[1].toULongLong();

            if (key == "MemTotal") {
                totalMem = value * 1024;  // Convert KB to bytes
                qDebug() << "PerformanceMonitor: MemTotal =" << totalMem << "bytes (" << value << "KB)";
            } else if (key == "MemFree") {
                memFree = value * 1024;
            } else if (key == "Buffers") {
                buffers = value * 1024;
            } else if (key == "Cached") {
                cached = value * 1024;
            } else if (key == "MemAvailable") {
                // MemAvailable is the best estimate (available since kernel 3.14)
                availableMem = value * 1024;
                qDebug() << "PerformanceMonitor: MemAvailable =" << availableMem << "bytes (" << value << "KB)";
            }
        }
    }
    file.close();

    // If MemAvailable wasn't found, estimate it
    if (availableMem == 0) {
        availableMem = memFree + buffers + cached;
        qDebug() << "PerformanceMonitor: MemAvailable not found, estimated as" << availableMem << "bytes";
    }

    qDebug() << "PerformanceMonitor: Final - Total:" << (totalMem / 1024 / 1024) << "MB"
             << "Available:" << (availableMem / 1024 / 1024) << "MB"
             << "Percent:" << QString::number((availableMem * 100.0) / totalMem, 'f', 1) << "%";
#endif
}

double PerformanceMonitor::getSystemMemoryAvailablePercent()
{
    quint64 totalMem, availableMem;
    readSystemMemoryInfo(totalMem, availableMem);

    if (totalMem == 0) {
        return 0.0;
    }

    return (availableMem * 100.0) / totalMem;
}

quint64 PerformanceMonitor::getSystemMemoryAvailable()
{
    quint64 totalMem, availableMem;
    readSystemMemoryInfo(totalMem, availableMem);
    return availableMem;
}

quint64 PerformanceMonitor::getSystemMemoryTotal()
{
    quint64 totalMem, availableMem;
    readSystemMemoryInfo(totalMem, availableMem);
    return totalMem;
}

bool PerformanceMonitor::isMemoryPressureHigh()
{
    double availablePercent = getSystemMemoryAvailablePercent();
    return availablePercent < 20.0;  // High pressure if < 20% available
}
