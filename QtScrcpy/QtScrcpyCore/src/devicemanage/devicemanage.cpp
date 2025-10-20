#include <QDebug>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>

#include "devicemanage.h"
#include "device.h"
#include "demuxer.h"

namespace qsc {

#define DM_MAX_DEVICES_NUM 1000

IDeviceManage& IDeviceManage::getInstance() {
    static DeviceManage dm;
    return dm;
}

DeviceManage::DeviceManage() {
    Demuxer::init();
}

DeviceManage::~DeviceManage() {
    Demuxer::deInit();
}

QPointer<IDevice> DeviceManage::getDevice(const QString &serial)
{
    if (!m_devices.contains(serial)) {
        return QPointer<IDevice>();
    }
    return m_devices[serial];
}

QStringList DeviceManage::getAllConnectedSerials() const
{
    QStringList serials = m_devices.keys();
    qInfo() << "DeviceManage::getAllConnectedSerials() called";
    qInfo() << "  m_devices map size:" << m_devices.size();
    qInfo() << "  Connected serials:" << serials;
    for (const QString& serial : serials) {
        auto device = m_devices[serial];
        qInfo() << "    -" << serial << "device pointer:" << (device ? "valid" : "null");
    }
    return serials;
}

bool DeviceManage::connectDevice(qsc::DeviceParams params)
{
    qInfo() << "========================================";
    qInfo() << "DeviceManage::connectDevice() START:" << params.serial;
    qInfo() << "========================================";

    if (params.serial.trimmed().isEmpty()) {
        qWarning() << "DeviceManage: Serial is empty, aborting";
        return false;
    }
    if (m_devices.contains(params.serial)) {
        qWarning() << "DeviceManage: Device already exists in m_devices map:" << params.serial;
        return false;
    }
    if (DM_MAX_DEVICES_NUM < m_devices.size()) {
        qWarning() << "DeviceManage: Over the maximum number of connections";
        return false;
    }

    qInfo() << "DeviceManage: Pre-flight checks passed, creating Device object...";
    /*
    // 没有必要分配端口，都用27183即可，连接建立以后server会释放监听的
    quint16 port = 0;
    if (params.useReverse) {
         port = getFreePort();
        if (0 == port) {
            qInfo("no port available, automatically switch to forward");
            params.useReverse = false;
        } else {
            params.localPort = port;
            qInfo("free port %d", port);
        }
    }
    */

    // CRITICAL FIX: Add try-catch to prevent crashes during Device creation
    IDevice *device = nullptr;
    try {
        qInfo() << "DeviceManage: Creating Device object for:" << params.serial;
        qInfo() << "  Port:" << params.localPort;
        qInfo() << "  Resolution:" << params.maxSize;
        qInfo() << "  Bitrate:" << params.bitRate;
        qInfo() << "  FPS:" << params.maxFps;

        device = new Device(params);

        if (!device) {
            qCritical() << "DeviceManage: CRITICAL - Device constructor returned nullptr!";
            qInfo() << "========================================";
            return false;
        }

        qInfo() << "DeviceManage: Device object created successfully:" << device;

    } catch (const std::exception& e) {
        qCritical() << "DeviceManage: EXCEPTION during Device creation:" << e.what();
        qCritical() << "  Serial:" << params.serial;
        if (device) {
            delete device;
            device = nullptr;
        }
        qInfo() << "========================================";
        return false;
    } catch (...) {
        qCritical() << "DeviceManage: UNKNOWN EXCEPTION during Device creation";
        qCritical() << "  Serial:" << params.serial;
        if (device) {
            delete device;
            device = nullptr;
        }
        qInfo() << "========================================";
        return false;
    }

    qInfo() << "DeviceManage: Connecting Device signals...";
    connect(device, &Device::deviceConnected, this, &DeviceManage::onDeviceConnected);
    connect(device, &Device::deviceDisconnected, this, &DeviceManage::onDeviceDisconnected);

    // Add device to map BEFORE connecting to make it available for signal handlers
    qInfo() << "DeviceManage: Adding device to m_devices map";
    m_devices[params.serial] = device;

    qInfo() << "DeviceManage: Calling device->connectDevice()...";
    bool connectResult = false;
    try {
        connectResult = device->connectDevice();
    } catch (const std::exception& e) {
        qCritical() << "DeviceManage: EXCEPTION during device->connectDevice():" << e.what();
        connectResult = false;
    } catch (...) {
        qCritical() << "DeviceManage: UNKNOWN EXCEPTION during device->connectDevice()";
        connectResult = false;
    }

    if (!connectResult) {
        // Connection failed, remove from map and clean up
        qWarning() << "DeviceManage: device->connectDevice() returned false, cleaning up";
        m_devices.remove(params.serial);
        delete device;
        qInfo() << "========================================";
        return false;
    }

    qInfo() << "DeviceManage: device->connectDevice() returned true (async connection started)";
    qInfo() << "DeviceManage: Waiting for Device to emit deviceConnected signal...";
    qInfo() << "========================================";
    return true;
}

bool DeviceManage::disconnectDevice(const QString &serial)
{
    bool ret = false;
    if (!serial.isEmpty() && m_devices.contains(serial)) {
        auto it = m_devices.find(serial);
        if (it->data()) {
            delete it->data();
            ret = true;
        }
    }
    return ret;
}

void DeviceManage::disconnectAllDevice()
{
    QMapIterator<QString, QPointer<IDevice>> i(m_devices);
    while (i.hasNext()) {
        i.next();
        if (i.value()) {
            delete i.value();
        }
    }
}

void DeviceManage::onDeviceConnected(bool success, const QString &serial, const QString &deviceName, const QSize &size)
{
    qInfo() << "========================================";
    qInfo() << "DeviceManage::onDeviceConnected() - Signal received from Device";
    qInfo() << "  Serial:" << serial;
    qInfo() << "  Success:" << success;
    qInfo() << "  DeviceName:" << deviceName;
    qInfo() << "  Size:" << size;
    qInfo() << "========================================";

    qInfo() << "DeviceManage: Forwarding deviceConnected signal to FarmViewer...";
    emit deviceConnected(success, serial, deviceName, size);
    qInfo() << "DeviceManage: deviceConnected signal emitted";

    if (!success) {
        qWarning() << "DeviceManage: Connection failed, removing device:" << serial;
        removeDevice(serial);
    }

    qInfo() << "========================================";
}

void DeviceManage::onDeviceDisconnected(QString serial)
{
    emit deviceDisconnected(serial);
    removeDevice(serial);
}

quint16 DeviceManage::getFreePort()
{
    quint16 port = m_localPortStart;
    while (port < m_localPortStart + DM_MAX_DEVICES_NUM) {
        bool used = false;
        QMapIterator<QString, QPointer<IDevice>> i(m_devices);
        while (i.hasNext()) {
            i.next();
            auto device = i.value();
            if (device && device->isReversePort(port)) {
                used = true;
                break;
            }
        }
        if (!used) {
            return port;
        }
        port++;
    }
    return 0;
}

void DeviceManage::removeDevice(const QString &serial)
{
    if (!serial.isEmpty() && m_devices.contains(serial)) {
        m_devices[serial]->deleteLater();
        m_devices.remove(serial);
    }
}

}
