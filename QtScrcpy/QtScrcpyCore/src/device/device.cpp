#include <QDir>
#include <QMessageBox>
#include <QTimer>

#include "controller.h"
#include "devicemsg.h"
#include "decoder.h"
#include "device.h"
#include "filehandler.h"
#include "recorder.h"
#include "server.h"
#include "demuxer.h"

namespace qsc {

Device::Device(DeviceParams params, QObject *parent) : IDevice(parent), m_params(params)
{
    qInfo() << "========================================";
    qInfo() << "Device::Device() CONSTRUCTOR START";
    qInfo() << "  Serial:" << params.serial;
    qInfo() << "  Display:" << params.display;
    qInfo() << "  RecordFile:" << params.recordFile;
    qInfo() << "========================================";

    if (!params.display && !m_params.recordFile) {
        qCritical("not display must be recorded");
        return;
    }

    qInfo() << "Device: Creating Demuxer...";
    m_stream = new Demuxer(this);
    qInfo() << "Device: Demuxer created successfully";

    if (params.display) {
        qInfo() << "Device: Creating Decoder WITHOUT parent for moveToThread()...";
        // CRITICAL: Create Decoder WITHOUT parent so it can be moved to another thread
        // QObject::moveToThread() requires the object to have NO parent
        m_decoder = new Decoder([this](int width, int height, uint8_t* dataY, uint8_t* dataU, uint8_t* dataV, int linesizeY, int linesizeU, int linesizeV) {
            // Log first frame only to avoid spam (per-device, NOT static)
            if (!m_firstFrameDecoded) {
                qInfo() << "========================================";
                qInfo() << "Device: Decoder callback - FIRST FRAME DECODED!";
                qInfo() << "  Serial:" << m_params.serial;
                qInfo() << "  Frame size:" << width << "x" << height;
                qInfo() << "========================================";
                m_firstFrameDecoded = true;
            }

            // Thread-safe access to observers using mutex
            QMutexLocker locker(&m_observersMutex);

            if (m_deviceObservers.empty()) {
                qWarning() << "Device: No observers registered for video frames (serial:" << m_params.serial << ")";
                return;
            }

            qInfo() << "Device: Dispatching frame to" << m_deviceObservers.size() << "observers for:" << m_params.serial;

            // Dispatch frame to all observers
            for (const auto& item : m_deviceObservers) {
                item->onFrame(width, height, dataY, dataU, dataV, linesizeY, linesizeU, linesizeV);
            }
        }, nullptr); // NO PARENT - allows moveToThread()
        qInfo() << "Device: Decoder created successfully (no parent)";

        // CRITICAL: Move Decoder to Demuxer's thread for FFmpeg thread affinity
        // FFmpeg codec contexts must be used from the same thread they were created in.
        // Since Demuxer runs in its own thread and emits packets, Decoder must run there too.
        qInfo() << "Device: Moving Decoder to Demuxer's thread for FFmpeg thread safety...";
        qInfo() << "  Decoder parent before move:" << (void*)m_decoder->parent();
        m_decoder->moveToThread(m_stream);
        qInfo() << "Device: Decoder moved to Demuxer thread successfully";
        qInfo() << "  Decoder thread after move:" << (void*)m_decoder->thread();

        qInfo() << "Device: Creating FileHandler...";
        m_fileHandler = new FileHandler(this);
        qInfo() << "Device: FileHandler created successfully";

        qInfo() << "Device: Creating Controller...";
        m_controller = new Controller([this](const QByteArray& buffer) -> qint64 {
            if (!m_server || !m_server->getControlSocket()) {
                return 0;
            }

            return m_server->getControlSocket()->write(buffer.data(), buffer.length());
        }, params.gameScript, this);
        qInfo() << "Device: Controller created successfully";
    }

    qInfo() << "Device: Creating Server...";
    m_server = new Server(this);
    qInfo() << "Device: Server created successfully:" << m_server;

    if (m_params.recordFile && !m_params.recordPath.trimmed().isEmpty()) {
        qInfo() << "Device: Setting up recording...";
        QString absFilePath;
        QString fileDir(m_params.recordPath);
        if (!fileDir.isEmpty()) {
            QDateTime dateTime = QDateTime::currentDateTime();
            QString fileName = dateTime.toString("_yyyyMMdd_hhmmss_zzz");
            fileName = m_params.serial + fileName;
            fileName.replace(":", "_");
            fileName.replace(".", "_");
            fileName += ("." + m_params.recordFileFormat);
            QDir dir(fileDir);
            if (!dir.exists()) {
                if (!dir.mkpath(fileDir)) {
                    qCritical() << QString("Failed to create the save folder: %1").arg(fileDir);
                }
            }
            absFilePath = dir.absoluteFilePath(fileName);
        }
        qInfo() << "Device: Creating Recorder...";
        m_recorder = new Recorder(absFilePath, this);
        qInfo() << "Device: Recorder created successfully";
    }

    qInfo() << "Device: Calling initSignals()...";
    initSignals();
    qInfo() << "Device: initSignals() completed";

    qInfo() << "========================================";
    qInfo() << "Device::Device() CONSTRUCTOR COMPLETE";
    qInfo() << "  Serial:" << params.serial;
    qInfo() << "========================================";
}

Device::~Device()
{
    Device::disconnectDevice();
}

void Device::setUserData(void *data)
{
    m_userData = data;
}

void *Device::getUserData()
{
    return m_userData;
}

void Device::registerDeviceObserver(DeviceObserver *observer)
{
    qInfo() << "========================================";
    qInfo() << "Device::registerDeviceObserver() called";
    qInfo() << "  Serial:" << m_params.serial;
    qInfo() << "  Observer pointer:" << observer;

    // Thread-safe registration
    QMutexLocker locker(&m_observersMutex);
    qInfo() << "  Total observers before insert:" << m_deviceObservers.size();

    m_deviceObservers.insert(observer);

    qInfo() << "Device: Observer registered successfully";
    qInfo() << "  Total observers after insert:" << m_deviceObservers.size();
    qInfo() << "========================================";
}

void Device::deRegisterDeviceObserver(DeviceObserver *observer)
{
    // Thread-safe deregistration
    QMutexLocker locker(&m_observersMutex);
    m_deviceObservers.erase(observer);
}

const QString &Device::getSerial()
{
    return m_params.serial;
}

void Device::updateScript(QString script)
{
    if (m_controller) {
        m_controller->updateScript(script);
    }
}

void Device::screenshot()
{
    if (!m_decoder) {
        return;
    }

    // screenshot
    m_decoder->peekFrame([this](int width, int height, uint8_t* dataRGB32) {
       saveFrame(width, height, dataRGB32);
    });
}

void Device::showTouch(bool show)
{
    AdbProcess *adb = new qsc::AdbProcess();
    if (!adb) {
        return;
    }
    connect(adb, &qsc::AdbProcess::adbProcessResult, this, [this](qsc::AdbProcess::ADB_EXEC_RESULT processResult) {
        if (AdbProcess::AER_SUCCESS_START != processResult) {
            sender()->deleteLater();
        }
    });
    adb->setShowTouchesEnabled(getSerial(), show);

    qInfo() << getSerial() << " show touch " << (show ? "enable" : "disable");
}

bool Device::isReversePort(quint16 port)
{
    if (m_server && m_server->isReverse() && port == m_server->getParams().localPort) {
        return true;
    }

    return false;
}

void Device::initSignals()
{
    if (m_controller) {
        connect(m_controller, &Controller::grabCursor, this, [this](bool grab){
            QMutexLocker locker(&m_observersMutex);
            for (const auto& item : m_deviceObservers) {
                item->grabCursor(grab);
            }
        });
    }
    if (m_fileHandler) {
        connect(m_fileHandler, &FileHandler::fileHandlerResult, this, [this](FileHandler::FILE_HANDLER_RESULT processResult, bool isApk) {
            QString tipsType = "";
            if (isApk) {
                tipsType = "install apk";
            } else {
                tipsType = "file transfer";
            }
            QString tips;
            if (FileHandler::FAR_IS_RUNNING == processResult) {
                tips = QString("wait current %1 to complete").arg(tipsType);
            }
            if (FileHandler::FAR_SUCCESS_EXEC == processResult) {
                tips = QString("%1 complete, save in %2").arg(tipsType).arg(m_params.pushFilePath);
            }
            if (FileHandler::FAR_ERROR_EXEC == processResult) {
                tips = QString("%1 failed").arg(tipsType);
            }
            qInfo() << tips;
        });
    }

    if (m_server) {
        connect(m_server, &Server::serverStarted, this, [this](bool success, const QString &deviceName, const QSize &size) {
            qInfo() << "========================================";
            qInfo() << "Device: serverStarted signal received from Server";
            qInfo() << "  Serial:" << m_params.serial;
            qInfo() << "  Success:" << success;
            qInfo() << "  DeviceName:" << deviceName;
            qInfo() << "  Size:" << size;
            qInfo() << "========================================";

            m_serverStartSuccess = success;

            qInfo() << "Device: Emitting deviceConnected signal to DeviceManage...";
            emit deviceConnected(success, m_params.serial, deviceName, size);
            qInfo() << "Device: deviceConnected signal emitted";

            if (success) {
                qInfo() << "Device: Server started successfully, initializing decoders and stream...";
                double diff = m_startTimeCount.elapsed() / 1000.0;
                qInfo() << QString("server start finish in %1s").arg(diff).toStdString().c_str();

                // init recorder
                if (m_recorder) {
                    m_recorder->setFrameSize(size);
                    if (!m_recorder->open()) {
                        qCritical("Could not open recorder");
                    }

                    if (!m_recorder->startRecorder()) {
                        qCritical("Could not start recorder");
                    }
                }

                // CRITICAL: Set decoder frame size BEFORE starting decode
                // The decoder needs to know the dimensions to properly initialize its codec context
                // Without this, the codec context remains 0x0 and FFmpeg crashes
                if (m_decoder) {
                    qInfo() << "Device: Setting decoder frame size to:" << size;
                    m_decoder->setFrameSize(size);
                }

                // init stream FIRST (starts Demuxer thread)
                m_stream->installVideoSocket(m_server->removeVideoSocket());
                m_stream->setFrameSize(size);
                m_stream->startDecode();

                // CRITICAL: Don't call decoder->open() here!
                // Demuxer::run() doesn't have Qt event loop, so queued calls never execute
                // Instead, decoder will initialize LAZILY on first push() call (in Demuxer thread)
                if (m_decoder) {
                    qInfo() << "Device: Decoder will initialize lazily on first packet";
                }

                // recv device msg
                connect(m_server->getControlSocket(), &QTcpSocket::readyRead, this, [this](){
                    if (!m_controller) {
                        return;
                    }

                    auto controlSocket = m_server->getControlSocket();
                    while (controlSocket->bytesAvailable()) {
                        QByteArray byteArray = controlSocket->peek(controlSocket->bytesAvailable());
                        DeviceMsg deviceMsg;
                        qint32 consume = deviceMsg.deserialize(byteArray);
                        if (0 >= consume) {
                            break;
                        }
                        controlSocket->read(consume);
                        m_controller->recvDeviceMsg(&deviceMsg);
                    }
                });

                // 显示界面时才自动息屏（m_params.display）
                if (m_params.closeScreen && m_params.display && m_controller) {
                    m_controller->setDisplayPower(false);
                }
            } else {
                m_server->stop();
            }
        });
        connect(m_server, &Server::serverStoped, this, [this]() {
            disconnectDevice();
            qDebug() << "server process stop";
        });
    }

    if (m_stream) {
        connect(m_stream, &Demuxer::onStreamStop, this, [this]() {
            disconnectDevice();
            qDebug() << "stream thread stop";
        });
        // CRITICAL: Use DirectConnection since Decoder now runs in Demuxer's thread
        // Decoder was moved to Demuxer's thread via moveToThread(), so they share the same thread.
        // This ensures FFmpeg codec operations and packet access happen in the correct thread.
        connect(m_stream, &Demuxer::getFrame, this, [this](AVPacket *packet) {
            qInfo() << "Device: getFrame signal received, calling decoder->push() from thread:" << QThread::currentThreadId();
            if (m_decoder && !m_decoder->push(packet)) {
                qCritical("Could not send packet to decoder");
            }

            if (m_recorder && !m_recorder->push(packet)) {
                qCritical("Could not send packet to recorder");
            }
        }, Qt::DirectConnection); // DirectConnection is safe now - Decoder is in Demuxer's thread!
        connect(m_stream, &Demuxer::getConfigFrame, this, [this](AVPacket *packet) {
            // Config packets are for recorder only (file header)
            // The decoder receives SPS/PPS concatenated with first frame via getFrame signal
            qInfo() << "Device: getConfigFrame signal received, sending to recorder only...";
            if (m_recorder && !m_recorder->push(packet)) {
                qCritical("Could not send config packet to recorder");
            }
        }, Qt::DirectConnection);
    }

    if (m_decoder) {
        connect(m_decoder, &Decoder::updateFPS, this, [this](quint32 fps) {
            QMutexLocker locker(&m_observersMutex);
            for (const auto& item : m_deviceObservers) {
                item->updateFPS(fps);
            }
        });
    }
}

bool Device::connectDevice()
{
    qInfo() << "========================================";
    qInfo() << "Device::connectDevice() START:" << m_params.serial;
    qInfo() << "========================================";

    if (!m_server) {
        qWarning() << "Device: m_server is null, cannot connect";
        return false;
    }

    if (m_serverStartSuccess) {
        qWarning() << "Device: Server already started successfully";
        return false;
    }

    qInfo() << "Device: Pre-flight checks passed, scheduling server start via QTimer...";

    // fix: macos cant recv finished signel, timer is ok
    QTimer::singleShot(0, this, [this]() {
        qInfo() << "========================================";
        qInfo() << "Device: QTimer callback - Starting server for:" << m_params.serial;
        qInfo() << "========================================";
        m_startTimeCount.start();
        // max size support 480p 720p 1080p 设备原生分辨率
        // support wireless connect, example:
        //m_server->start("192.168.0.174:5555", 27183, m_maxSize, m_bitRate, "");
        // only one devices, serial can be null
        // mark: crop input format: "width:height:x:y" or "" for no crop, for example: "100:200:0:0"
        Server::ServerParams params;
        params.serverLocalPath = m_params.serverLocalPath;
        params.serverRemotePath = m_params.serverRemotePath;
        params.serial = m_params.serial;
        params.localPort = m_params.localPort;
        params.maxSize = m_params.maxSize;
        params.bitRate = m_params.bitRate;
        params.maxFps = m_params.maxFps;
        params.useReverse = m_params.useReverse;
        params.captureOrientationLock = m_params.captureOrientationLock;
        params.captureOrientation = m_params.captureOrientation;
        params.stayAwake = m_params.stayAwake;
        params.serverVersion = m_params.serverVersion;
        params.logLevel = m_params.logLevel;
        params.codecOptions = m_params.codecOptions;
        params.codecName = m_params.codecName;
        params.scid = m_params.scid;

        params.crop = "";
        params.control = true;

        qInfo() << "Device: Calling m_server->start()...";
        qInfo() << "  Serial:" << params.serial;
        qInfo() << "  LocalPort:" << params.localPort;
        qInfo() << "  MaxSize:" << params.maxSize;
        qInfo() << "  BitRate:" << params.bitRate;
        qInfo() << "  MaxFps:" << params.maxFps;
        qInfo() << "  UseReverse:" << params.useReverse;

        m_server->start(params);

        qInfo() << "Device: m_server->start() called, waiting for serverStarted signal...";
        qInfo() << "========================================";
    });

    qInfo() << "Device::connectDevice() returning true (async start scheduled)";
    qInfo() << "========================================";
    return true;
}

void Device::disconnectDevice()
{
    if (!m_server) {
        return;
    }
    m_server->stop();
    m_server = Q_NULLPTR;

    if (m_stream) {
        m_stream->stopDecode();
    }

    // server must stop before decoder, because decoder block main thread
    if (m_decoder) {
        m_decoder->close();
    }

    if (m_recorder) {
        if (m_recorder->isRunning()) {
            m_recorder->stopRecorder();
            m_recorder->wait();
        }
        m_recorder->close();
    }

    if (m_serverStartSuccess) {
        emit deviceDisconnected(m_params.serial);
    }
    m_serverStartSuccess = false;
}

void Device::postGoBack()
{
    if (!m_controller) {
        return;
    }
    m_controller->postGoBack();

    QMutexLocker locker(&m_observersMutex);
    for (const auto& item : m_deviceObservers) {
        item->postGoBack();
    }
}

void Device::postGoHome()
{
    if (!m_controller) {
        return;
    }
    m_controller->postGoHome();

    QMutexLocker locker(&m_observersMutex);
    for (const auto& item : m_deviceObservers) {
        item->postGoHome();
    }
}

void Device::postGoMenu()
{
    if (!m_controller) {
        return;
    }
    m_controller->postGoMenu();

    QMutexLocker locker(&m_observersMutex);
    for (const auto& item : m_deviceObservers) {
        item->postGoMenu();
    }
}

void Device::postAppSwitch()
{
    if (!m_controller) {
        return;
    }
    m_controller->postAppSwitch();

    QMutexLocker locker(&m_observersMutex);
    for (const auto& item : m_deviceObservers) {
        item->postAppSwitch();
    }
}

void Device::postPower()
{
    if (!m_controller) {
        return;
    }
    m_controller->postPower();

    QMutexLocker locker(&m_observersMutex);
    for (const auto& item : m_deviceObservers) {
        item->postPower();
    }
}

void Device::postVolumeUp()
{
    if (!m_controller) {
        return;
    }
    m_controller->postVolumeUp();

    QMutexLocker locker(&m_observersMutex);
    for (const auto& item : m_deviceObservers) {
        item->postVolumeUp();
    }
}

void Device::postVolumeDown()
{
    if (!m_controller) {
        return;
    }
    m_controller->postVolumeDown();

    QMutexLocker locker(&m_observersMutex);
    for (const auto& item : m_deviceObservers) {
        item->postVolumeDown();
    }
}

void Device::postCopy()
{
    if (!m_controller) {
        return;
    }
    m_controller->copy();

    QMutexLocker locker(&m_observersMutex);
    for (const auto& item : m_deviceObservers) {
        item->postCopy();
    }
}

void Device::postCut()
{
    if (!m_controller) {
        return;
    }
    m_controller->cut();

    QMutexLocker locker(&m_observersMutex);
    for (const auto& item : m_deviceObservers) {
        item->postCut();
    }
}

void Device::setDisplayPower(bool on)
{
    if (!m_controller) {
        return;
    }
    m_controller->setDisplayPower(on);

    QMutexLocker locker(&m_observersMutex);
    for (const auto& item : m_deviceObservers) {
        item->setDisplayPower(on);
    }
}

void Device::expandNotificationPanel()
{
    if (!m_controller) {
        return;
    }
    m_controller->expandNotificationPanel();

    QMutexLocker locker(&m_observersMutex);
    for (const auto& item : m_deviceObservers) {
        item->expandNotificationPanel();
    }
}

void Device::collapsePanel()
{
    if (!m_controller) {
        return;
    }
    m_controller->collapsePanel();

    QMutexLocker locker(&m_observersMutex);
    for (const auto& item : m_deviceObservers) {
        item->collapsePanel();
    }
}

void Device::postBackOrScreenOn(bool down)
{
    if (!m_controller) {
        return;
    }
    m_controller->postBackOrScreenOn(down);

    QMutexLocker locker(&m_observersMutex);
    for (const auto& item : m_deviceObservers) {
        item->postBackOrScreenOn(down);
    }
}

void Device::postTextInput(QString &text)
{
    if (!m_controller) {
        return;
    }
    m_controller->postTextInput(text);

    QMutexLocker locker(&m_observersMutex);
    for (const auto& item : m_deviceObservers) {
        item->postTextInput(text);
    }
}

void Device::requestDeviceClipboard()
{
    if (!m_controller) {
        return;
    }
    m_controller->requestDeviceClipboard();

    QMutexLocker locker(&m_observersMutex);
    for (const auto& item : m_deviceObservers) {
        item->requestDeviceClipboard();
    }
}

void Device::setDeviceClipboard(bool pause)
{
    if (!m_controller) {
        return;
    }
    m_controller->setDeviceClipboard(pause);

    QMutexLocker locker(&m_observersMutex);
    for (const auto& item : m_deviceObservers) {
        item->setDeviceClipboard(pause);
    }
}

void Device::clipboardPaste()
{
    if (!m_controller) {
        return;
    }
    m_controller->clipboardPaste();

    QMutexLocker locker(&m_observersMutex);
    for (const auto& item : m_deviceObservers) {
        item->clipboardPaste();
    }
}

void Device::pushFileRequest(const QString &file, const QString &devicePath)
{
    if (!m_fileHandler) {
        return;
    }
    m_fileHandler->onPushFileRequest(getSerial(), file, devicePath);

    QMutexLocker locker(&m_observersMutex);
    for (const auto& item : m_deviceObservers) {
        item->pushFileRequest(file, devicePath);
    }
}

void Device::installApkRequest(const QString &apkFile)
{
    if (!m_fileHandler) {
        return;
    }
    m_fileHandler->onInstallApkRequest(getSerial(), apkFile);

    QMutexLocker locker(&m_observersMutex);
    for (const auto& item : m_deviceObservers) {
        item->installApkRequest(apkFile);
    }
}

void Device::mouseEvent(const QMouseEvent *from, const QSize &frameSize, const QSize &showSize)
{
    if (!m_controller) {
        return;
    }
    m_controller->mouseEvent(from, frameSize, showSize);

    QMutexLocker locker(&m_observersMutex);
    for (const auto& item : m_deviceObservers) {
        item->mouseEvent(from, frameSize, showSize);
    }
}

void Device::wheelEvent(const QWheelEvent *from, const QSize &frameSize, const QSize &showSize)
{
    if (!m_controller) {
        return;
    }
    m_controller->wheelEvent(from, frameSize, showSize);

    QMutexLocker locker(&m_observersMutex);
    for (const auto& item : m_deviceObservers) {
        item->wheelEvent(from, frameSize, showSize);
    }
}

void Device::keyEvent(const QKeyEvent *from, const QSize &frameSize, const QSize &showSize)
{
    if (!m_controller) {
        return;
    }
    m_controller->keyEvent(from, frameSize, showSize);

    QMutexLocker locker(&m_observersMutex);
    for (const auto& item : m_deviceObservers) {
        item->keyEvent(from, frameSize, showSize);
    }
}

bool Device::isCurrentCustomKeymap()
{
    if (!m_controller) {
        return false;
    }
    return m_controller->isCurrentCustomKeymap();
}

bool Device::saveFrame(int width, int height, uint8_t* dataRGB32)
{
    if (!dataRGB32) {
        return false;
    }

    QImage rgbImage(dataRGB32, width, height, QImage::Format_RGB32);

    // save
    QString absFilePath;
    QString fileDir(m_params.recordPath);
    if (fileDir.isEmpty()) {
        qWarning() << "please select record save path!!!";
        return false;
    }
    QDateTime dateTime = QDateTime::currentDateTime();
    QString fileName = dateTime.toString("_yyyyMMdd_hhmmss_zzz");
    fileName = m_params.serial + fileName;
    fileName.replace(":", "_");
    fileName.replace(".", "_");
    fileName += ".png";
    QDir dir(fileDir);
    absFilePath = dir.absoluteFilePath(fileName);
    int ret = rgbImage.save(absFilePath, "PNG", 100);
    if (!ret) {
        return false;
    }

    qInfo() << "screenshot save to " << absFilePath;
    return true;
}

}
