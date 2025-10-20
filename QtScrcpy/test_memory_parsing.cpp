#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>
#include <QCoreApplication>

void testMemoryParsing() {
    QFile file("/proc/meminfo");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open /proc/meminfo";
        return;
    }

    QTextStream in(&file);
    quint64 totalMem = 0;
    quint64 availableMem = 0;
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
                totalMem = value * 1024;
                qInfo() << "MemTotal =" << (totalMem / 1024 / 1024) << "MB";
            } else if (key == "MemFree") {
                memFree = value * 1024;
            } else if (key == "Buffers") {
                buffers = value * 1024;
            } else if (key == "Cached") {
                cached = value * 1024;
            } else if (key == "MemAvailable") {
                availableMem = value * 1024;
                qInfo() << "MemAvailable =" << (availableMem / 1024 / 1024) << "MB";
            }
        }
    }
    file.close();

    if (availableMem == 0) {
        availableMem = memFree + buffers + cached;
        qInfo() << "MemAvailable not found, estimated as" << (availableMem / 1024 / 1024) << "MB";
    }

    double percent = (totalMem > 0) ? (availableMem * 100.0) / totalMem : 0.0;
    qInfo() << "Final - Total:" << (totalMem / 1024 / 1024) << "MB"
             << "Available:" << (availableMem / 1024 / 1024) << "MB"
             << "Percent:" << QString::number(percent, 'f', 1) << "%";

    if (percent > 0.0) {
        qInfo() << "SUCCESS: Memory parsing works correctly!";
    } else {
        qWarning() << "FAILED: Memory parsing returned 0%";
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    testMemoryParsing();
    return 0;
}
