#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QThread>
#include <QTimer>
#include <QTimerEvent>

#include "server.h"

#define DEVICE_NAME_FIELD_LENGTH 64
#define SOCKET_NAME_PREFIX "scrcpy"
#define MAX_CONNECT_COUNT 30
#define MAX_RESTART_COUNT 1

static quint32 bufferRead32be(quint8 *buf)
{
    return static_cast<quint32>((buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3]);
}

Server::Server(QObject *parent) : QObject(parent)
{
    connect(&m_workProcess, &qsc::AdbProcess::adbProcessResult, this, &Server::onWorkProcessResult);
    connect(&m_serverProcess, &qsc::AdbProcess::adbProcessResult, this, &Server::onWorkProcessResult);

    connect(&m_serverSocket, &QTcpServer::newConnection, this, [this]() {
        QTcpSocket *tmp = m_serverSocket.nextPendingConnection();
        if (dynamic_cast<VideoSocket *>(tmp)) {
            m_videoSocket = dynamic_cast<VideoSocket *>(tmp);
            if (!m_videoSocket->isValid()) {
                qWarning("Video socket is invalid");
                stop();
                emit serverStarted(false);
                return;
            }
            // Use async reading instead of synchronous readInfo() to avoid race condition
            qInfo("Video socket connected, starting async read of device info...");
            startAsyncReadInfo(m_videoSocket);
        } else {
            m_controlSocket = tmp;
            if (m_controlSocket && m_controlSocket->isValid()) {
                // Check if we already have device info from async read
                if (!m_deviceName.isEmpty()) {
                    // we don't need the server socket anymore
                    // just m_videoSocket is ok
                    m_serverSocket.close();
                    // we don't need the adb tunnel anymore
                    disableTunnelReverse();
                    m_tunnelEnabled = false;
                    emit serverStarted(true, m_deviceName, m_deviceSize);
                }
            } else {
                stop();
                emit serverStarted(false);
            }
            stopAcceptTimeoutTimer();
        }
    });
}

Server::~Server() {}

bool Server::pushServer()
{
    if (m_workProcess.isRuning()) {
        m_workProcess.kill();
    }
    m_workProcess.push(m_params.serial, m_params.serverLocalPath, m_params.serverRemotePath);
    return true;
}

bool Server::enableTunnelReverse()
{
    if (m_workProcess.isRuning()) {
        m_workProcess.kill();
    }
    m_workProcess.reverse(m_params.serial, QString(SOCKET_NAME_PREFIX "_%1").arg(m_params.scid, 8, 16, QChar('0')), m_params.localPort);
    return true;
}

bool Server::disableTunnelReverse()
{
    qsc::AdbProcess *adb = new qsc::AdbProcess();
    if (!adb) {
        return false;
    }
    connect(adb, &qsc::AdbProcess::adbProcessResult, this, [this](qsc::AdbProcess::ADB_EXEC_RESULT processResult) {
        if (qsc::AdbProcess::AER_SUCCESS_START != processResult) {
            sender()->deleteLater();
        }
    });
    adb->reverseRemove(m_params.serial, QString(SOCKET_NAME_PREFIX "_%1").arg(m_params.scid, 8, 16, QChar('0')));
    return true;
}

bool Server::enableTunnelForward()
{
    if (m_workProcess.isRuning()) {
        m_workProcess.kill();
    }
    m_workProcess.forward(m_params.serial, m_params.localPort, QString(SOCKET_NAME_PREFIX "_%1").arg(m_params.scid, 8, 16, QChar('0')));
    return true;
}
bool Server::disableTunnelForward()
{
    qsc::AdbProcess *adb = new qsc::AdbProcess();
    if (!adb) {
        return false;
    }
    connect(adb, &qsc::AdbProcess::adbProcessResult, this, [this](qsc::AdbProcess::ADB_EXEC_RESULT processResult) {
        if (qsc::AdbProcess::AER_SUCCESS_START != processResult) {
            sender()->deleteLater();
        }
    });
    adb->forwardRemove(m_params.serial, m_params.localPort);
    return true;
}

bool Server::execute()
{
    if (m_serverProcess.isRuning()) {
        m_serverProcess.kill();
    }
    QStringList args;
    args << "shell";
    args << QString("CLASSPATH=%1").arg(m_params.serverRemotePath);
    args << "app_process";

#ifdef SERVER_DEBUGGER
#define SERVER_DEBUGGER_PORT "5005"

    args <<
#ifdef SERVER_DEBUGGER_METHOD_NEW
        /* Android 9 and above */
        "-XjdwpProvider:internal -XjdwpOptions:transport=dt_socket,suspend=y,server=y,address="
#else
        /* Android 8 and below */
        "-agentlib:jdwp=transport=dt_socket,suspend=y,server=y,address="
#endif
        SERVER_DEBUGGER_PORT,
#endif

        args << "/"; // unused;
    args << "com.genymobile.scrcpy.Server";
    args << m_params.serverVersion;

    args << QString("video_bit_rate=%1").arg(QString::number(m_params.bitRate));
    if (!m_params.logLevel.isEmpty()) {
        args << QString("log_level=%1").arg(m_params.logLevel);
    }
    if (m_params.maxSize > 0) {
        args << QString("max_size=%1").arg(QString::number(m_params.maxSize));
    }
    if (m_params.maxFps > 0) {
        args << QString("max_fps=%1").arg(QString::number(m_params.maxFps));
    }

    // capture_orientation=@90
    // 有@表示锁定，没@不锁定
    // 有值表示指定方向，没值表示原始方向
    if (1 == m_params.captureOrientationLock) {
        args << QString("capture_orientation=@%1").arg(m_params.captureOrientation);
    } else if (2 == m_params.captureOrientationLock) {
        args << QString("capture_orientation=@");
    } else {
        args << QString("capture_orientation=%1").arg(m_params.captureOrientation);
    }
    if (m_tunnelForward) {
        args << QString("tunnel_forward=true");
    }
    if (!m_params.crop.isEmpty()) {
        args << QString("crop=%1").arg(m_params.crop);
    }
    if (!m_params.control) {
        args << QString("control=false");
    }
    // 默认是0，不需要设置
    // args << "display_id=0";
    // 默认是false，不需要设置
    // args << "show_touches=false";
    if (m_params.stayAwake) {
        args << QString("stay_awake=true");
    }
    // code option
    // https://github.com/Genymobile/scrcpy/commit/080a4ee3654a9b7e96c8ffe37474b5c21c02852a
    // <https://d.android.com/reference/android/media/MediaFormat>
    if (!m_params.codecOptions.isEmpty()) {
        args << QString("codec_options=%1").arg(m_params.codecOptions);
    }
    if (!m_params.codecName.isEmpty()) {
        args << QString("encoder_name=%1").arg(m_params.codecName);
    }
    args << "audio=false";
    // 服务端默认-1，可不传
    if (-1 != m_params.scid) {
        args << QString("scid=%1").arg(m_params.scid, 8, 16, QChar('0'));
    }

    // 默认是false，不需要设置
    // args << "power_off_on_close=false";

    // 下面的参数都用服务端默认值即可，尽量减少参数传递，传参太长导致三星手机报错：stack corruption detected (-fstack-protector)
    /*
    args << "clipboard_autosync=true";    
    args << "downsize_on_error=true";
    args << "cleanup=true";
    args << "power_on=true";
    
    args << "send_device_meta=true";
    args << "send_frame_meta=true";
    args << "send_dummy_byte=true";
    args << "raw_video_stream=false";
    */

#ifdef SERVER_DEBUGGER
    qInfo("Server debugger waiting for a client on device port " SERVER_DEBUGGER_PORT "...");
    // From the computer, run
    //     adb forward tcp:5005 tcp:5005
    // Then, from Android Studio: Run > Debug > Edit configurations...
    // On the left, click on '+', "Remote", with:
    //     Host: localhost
    //     Port: 5005
    // Then click on "Debug"
#endif

    // adb -s P7C0218510000537 shell CLASSPATH=/data/local/tmp/scrcpy-server app_process / com.genymobile.scrcpy.Server 0 8000000 false
    // mark: crop input format: "width:height:x:y" or "" for no crop, for example: "100:200:0:0"
    // 这条adb命令是阻塞运行的，m_serverProcess进程不会退出了
    m_serverProcess.execute(m_params.serial, args);
    return true;
}

bool Server::start(Server::ServerParams params)
{
    qInfo() << "========================================";
    qInfo() << "Server::start() called";
    qInfo() << "  Serial:" << params.serial;
    qInfo() << "  LocalPort:" << params.localPort;
    qInfo() << "  MaxSize:" << params.maxSize;
    qInfo() << "  BitRate:" << params.bitRate;
    qInfo() << "  UseReverse:" << params.useReverse;
    qInfo() << "========================================";

    m_params = params;
    m_serverStartStep = SSS_PUSH;

    qInfo() << "Server: Starting server by step (starting with SSS_PUSH)...";
    return startServerByStep();
}

bool Server::connectTo()
{
    if (SSS_RUNNING != m_serverStartStep) {
        qWarning("server not run");
        return false;
    }

    if (!m_tunnelForward && !m_videoSocket) {
        startAcceptTimeoutTimer();
        return true;
    }

    startConnectTimeoutTimer();
    return true;
}

bool Server::isReverse()
{
    return !m_tunnelForward;
}

Server::ServerParams Server::getParams()
{
    return m_params;
}

void Server::timerEvent(QTimerEvent *event)
{
    if (event && m_acceptTimeoutTimer == event->timerId()) {
        stopAcceptTimeoutTimer();
        emit serverStarted(false);
    } else if (event && m_connectTimeoutTimer == event->timerId()) {
        onConnectTimer();
    }
}

VideoSocket* Server::removeVideoSocket()
{
    VideoSocket* socket = m_videoSocket;
    m_videoSocket = Q_NULLPTR;
    return socket;
}

QTcpSocket *Server::getControlSocket()
{
    return m_controlSocket;
}

void Server::stop()
{
    if (m_tunnelForward) {
        stopConnectTimeoutTimer();
    } else {
        stopAcceptTimeoutTimer();
    }

    if (m_controlSocket) {
        m_controlSocket->close();
        m_controlSocket->deleteLater();
    }
    // ignore failure
    m_serverProcess.kill();
    if (m_tunnelEnabled) {
        if (m_tunnelForward) {
            disableTunnelForward();
        } else {
            disableTunnelReverse();
        }
        m_tunnelForward = false;
        m_tunnelEnabled = false;
    }
    m_serverSocket.close();
}

bool Server::startServerByStep()
{
    bool stepSuccess = false;
    // push, enable tunnel et start the server
    if (SSS_NULL != m_serverStartStep) {
        switch (m_serverStartStep) {
        case SSS_PUSH:
            stepSuccess = pushServer();
            break;
        case SSS_ENABLE_TUNNEL_REVERSE:
            stepSuccess = enableTunnelReverse();
            break;
        case SSS_ENABLE_TUNNEL_FORWARD:
            stepSuccess = enableTunnelForward();
            break;
        case SSS_EXECUTE_SERVER:
            // server will connect to our server socket
            stepSuccess = execute();
            break;
        default:
            break;
        }
    }

    if (!stepSuccess) {
        emit serverStarted(false);
    }
    return stepSuccess;
}

bool Server::readInfo(VideoSocket *videoSocket, QString &deviceName, QSize &size)
{
    // Synchronous version kept for compatibility, but uses async internally
    unsigned char buf[DEVICE_NAME_FIELD_LENGTH + 12];

    if (videoSocket->bytesAvailable() < (DEVICE_NAME_FIELD_LENGTH + 12)) {
        qInfo("readInfo: Not enough data available");
        return false;
    }

    qint64 len = videoSocket->read((char *)buf, sizeof(buf));
    if (len < DEVICE_NAME_FIELD_LENGTH + 12) {
        qInfo("Could not retrieve device information");
        return false;
    }
    buf[DEVICE_NAME_FIELD_LENGTH - 1] = '\0'; // in case the client sends garbage
    deviceName = QString::fromUtf8((const char *)buf);

    // 前4个字节是AVCodecID,当前只支持H264,所以先不解析
    size.setWidth(bufferRead32be(&buf[DEVICE_NAME_FIELD_LENGTH + 4]));
    size.setHeight(bufferRead32be(&buf[DEVICE_NAME_FIELD_LENGTH + 8]));

    return true;
}

void Server::startAsyncReadInfo(VideoSocket *videoSocket)
{
    if (!videoSocket) {
        qWarning("startAsyncReadInfo: null videoSocket");
        emit serverStarted(false);
        return;
    }

    m_readBuffer.clear();

    // Connect readyRead signal for async reading
    connect(videoSocket, &VideoSocket::readyRead,
            this, &Server::onVideoSocketReadyRead,
            Qt::UniqueConnection);

    // Check if data is already available
    if (videoSocket->bytesAvailable() >= (DEVICE_NAME_FIELD_LENGTH + 12)) {
        onVideoSocketReadyRead();
    }
}

void Server::startAcceptTimeoutTimer()
{
    stopAcceptTimeoutTimer();
    m_acceptTimeoutTimer = startTimer(1000);
}

void Server::stopAcceptTimeoutTimer()
{
    if (m_acceptTimeoutTimer) {
        killTimer(m_acceptTimeoutTimer);
        m_acceptTimeoutTimer = 0;
    }
}

void Server::startConnectTimeoutTimer()
{
    stopConnectTimeoutTimer();
    m_connectTimeoutTimer = startTimer(300);
}

void Server::stopConnectTimeoutTimer()
{
    if (m_connectTimeoutTimer) {
        killTimer(m_connectTimeoutTimer);
        m_connectTimeoutTimer = 0;
    }
    m_connectCount = 0;
}

void Server::onConnectTimer()
{
    // Use async connection instead of blocking waitForConnected
    if (m_connectCount >= MAX_CONNECT_COUNT) {
        stopConnectTimeoutTimer();
        stop();
        if (MAX_RESTART_COUNT > m_restartCount++) {
            qWarning("restart server auto");
            start(m_params);
        } else {
            m_restartCount = 0;
            emit serverStarted(false);
        }
        return;
    }

    m_connectCount++;
    startAsyncConnect();
}

void Server::startAsyncConnect()
{
    qDebug() << "Server::startAsyncConnect() attempt" << m_connectCount;

    // Clean up previous pending sockets if any
    if (m_pendingVideoSocket) {
        m_pendingVideoSocket->disconnect();
        m_pendingVideoSocket->deleteLater();
        m_pendingVideoSocket = Q_NULLPTR;
    }
    if (m_pendingControlSocket) {
        m_pendingControlSocket->disconnect();
        m_pendingControlSocket->deleteLater();
        m_pendingControlSocket = Q_NULLPTR;
    }

    m_videoSocketReady = false;
    m_controlSocketReady = false;

    // Create new sockets
    VideoSocket *videoSocket = new VideoSocket(this);
    QTcpSocket *controlSocket = new QTcpSocket(this);

    m_pendingVideoSocket = videoSocket;
    m_pendingControlSocket = controlSocket;

    // Connect video socket signals
    connect(videoSocket, &VideoSocket::connected,
            this, &Server::onVideoSocketConnected,
            Qt::UniqueConnection);
    connect(videoSocket, QOverload<QAbstractSocket::SocketError>::of(&VideoSocket::errorOccurred),
            this, &Server::onVideoSocketError,
            Qt::UniqueConnection);

    // Connect control socket signals
    connect(controlSocket, &QTcpSocket::connected,
            this, &Server::onControlSocketConnected,
            Qt::UniqueConnection);
    connect(controlSocket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, &Server::onControlSocketError,
            Qt::UniqueConnection);

    // Start async connections
    qDebug() << "Connecting to localhost:" << m_params.localPort;
    videoSocket->connectToHost(QHostAddress::LocalHost, m_params.localPort);
    controlSocket->connectToHost(QHostAddress::LocalHost, m_params.localPort);
}

void Server::onVideoSocketConnected()
{
    qDebug() << "Server::onVideoSocketConnected()";

    if (!m_pendingVideoSocket) {
        qWarning("onVideoSocketConnected: pending video socket is null");
        return;
    }

    // devices will send 1 byte first on tunnel forward mode
    if (m_pendingVideoSocket->bytesAvailable() > 0) {
        m_pendingVideoSocket->read(1);
    } else {
        // Wait for the first byte via readyRead signal
        connect(m_pendingVideoSocket, &VideoSocket::readyRead,
                this, [this]() {
                    if (m_pendingVideoSocket && m_pendingVideoSocket->bytesAvailable() > 0) {
                        m_pendingVideoSocket->read(1);
                    }
                },
                Qt::UniqueConnection);
    }

    m_videoSocketReady = true;

    // Start async reading of device info
    startAsyncReadInfo(m_pendingVideoSocket);
}

void Server::onControlSocketConnected()
{
    qDebug() << "Server::onControlSocketConnected()";

    if (!m_pendingControlSocket) {
        qWarning("onControlSocketConnected: pending control socket is null");
        return;
    }

    // devices will send 1 byte first on tunnel forward mode
    if (m_pendingControlSocket->bytesAvailable() > 0) {
        m_pendingControlSocket->read(1);
    } else {
        // Wait for the first byte via readyRead signal
        connect(m_pendingControlSocket, &QTcpSocket::readyRead,
                this, [this]() {
                    if (m_pendingControlSocket && m_pendingControlSocket->bytesAvailable() > 0) {
                        m_pendingControlSocket->read(1);
                    }
                },
                Qt::UniqueConnection);
    }

    m_controlSocketReady = true;

    // Check if we can finalize connection
    if (m_videoSocketReady && !m_deviceName.isEmpty()) {
        // Both sockets ready and device info received
        stopConnectTimeoutTimer();
        m_videoSocket = m_pendingVideoSocket;
        m_controlSocket = m_pendingControlSocket;
        m_pendingVideoSocket = Q_NULLPTR;
        m_pendingControlSocket = Q_NULLPTR;

        // we don't need the adb tunnel anymore
        disableTunnelForward();
        m_tunnelEnabled = false;
        m_restartCount = 0;
        emit serverStarted(true, m_deviceName, m_deviceSize);

        // Clear device info for next connection
        m_deviceName = "";
        m_deviceSize = QSize();
    }
}

void Server::onVideoSocketReadyRead()
{
    // Handle both reverse tunnel mode (m_videoSocket) and forward mode (m_pendingVideoSocket)
    VideoSocket *videoSocket = m_videoSocket ? m_videoSocket : m_pendingVideoSocket;

    if (!videoSocket) {
        qWarning("onVideoSocketReadyRead: no video socket available");
        return;
    }

    const int requiredBytes = DEVICE_NAME_FIELD_LENGTH + 12;

    // Accumulate data in buffer
    m_readBuffer.append(videoSocket->read(videoSocket->bytesAvailable()));

    // Check if we have enough data
    if (m_readBuffer.size() < requiredBytes) {
        qDebug() << "onVideoSocketReadyRead: waiting for more data."
                 << "Current:" << m_readBuffer.size()
                 << "Required:" << requiredBytes;
        return;
    }

    // Parse device info
    unsigned char *buf = reinterpret_cast<unsigned char*>(m_readBuffer.data());
    buf[DEVICE_NAME_FIELD_LENGTH - 1] = '\0'; // in case the client sends garbage
    m_deviceName = QString::fromUtf8((const char *)buf);

    // 前4个字节是AVCodecID,当前只支持H264,所以先不解析
    m_deviceSize.setWidth(bufferRead32be(&buf[DEVICE_NAME_FIELD_LENGTH + 4]));
    m_deviceSize.setHeight(bufferRead32be(&buf[DEVICE_NAME_FIELD_LENGTH + 8]));

    qDebug() << "Device info received:" << m_deviceName << m_deviceSize;

    // Remove processed data from buffer
    m_readBuffer.remove(0, requiredBytes);

    // Disconnect readyRead to avoid multiple calls
    disconnect(videoSocket, &VideoSocket::readyRead,
               this, &Server::onVideoSocketReadyRead);

    // Check if we can finalize connection based on mode
    if (m_pendingVideoSocket) {
        // Forward tunnel mode - wait for control socket
        if (m_controlSocketReady) {
            stopConnectTimeoutTimer();
            m_videoSocket = m_pendingVideoSocket;
            m_controlSocket = m_pendingControlSocket;
            m_pendingVideoSocket = Q_NULLPTR;
            m_pendingControlSocket = Q_NULLPTR;

            // we don't need the adb tunnel anymore
            disableTunnelForward();
            m_tunnelEnabled = false;
            m_restartCount = 0;
            emit serverStarted(true, m_deviceName, m_deviceSize);

            // Clear device info for next connection
            m_deviceName = "";
            m_deviceSize = QSize();
        }
    } else {
        // Reverse tunnel mode - device info received
        // Check if control socket is also ready
        qInfo("Device info received in reverse mode, checking for control socket...");
        if (m_controlSocket && m_controlSocket->isValid()) {
            // Both sockets ready - emit success!
            qInfo("Control socket already connected, emitting serverStarted(true)");
            m_serverSocket.close();
            disableTunnelReverse();
            m_tunnelEnabled = false;
            emit serverStarted(true, m_deviceName, m_deviceSize);
        } else {
            // Control socket not ready yet, wait for it to connect
            qInfo("Waiting for control socket to connect...");
        }
    }
}

void Server::onVideoSocketError(QAbstractSocket::SocketError error)
{
    qWarning() << "Video socket error:" << error;

    if (m_pendingVideoSocket) {
        qWarning() << "Video socket error string:" << m_pendingVideoSocket->errorString();
    }

    // Don't retry on connection errors - let the timer handle retry
    if (error == QAbstractSocket::ConnectionRefusedError ||
        error == QAbstractSocket::HostNotFoundError ||
        error == QAbstractSocket::NetworkError) {
        qWarning("Video socket connection error, will retry");
    }
}

void Server::onControlSocketError(QAbstractSocket::SocketError error)
{
    qWarning() << "Control socket error:" << error;

    if (m_pendingControlSocket) {
        qWarning() << "Control socket error string:" << m_pendingControlSocket->errorString();
    }

    // Don't retry on connection errors - let the timer handle retry
    if (error == QAbstractSocket::ConnectionRefusedError ||
        error == QAbstractSocket::HostNotFoundError ||
        error == QAbstractSocket::NetworkError) {
        qWarning("Control socket connection error, will retry");
    }
}

void Server::onWorkProcessResult(qsc::AdbProcess::ADB_EXEC_RESULT processResult)
{
    if (sender() == &m_workProcess) {
        if (SSS_NULL != m_serverStartStep) {
            switch (m_serverStartStep) {
            case SSS_PUSH:
                if (qsc::AdbProcess::AER_SUCCESS_EXEC == processResult) {
                    if (m_params.useReverse) {
                        m_serverStartStep = SSS_ENABLE_TUNNEL_REVERSE;
                    } else {
                        m_tunnelForward = true;
                        m_serverStartStep = SSS_ENABLE_TUNNEL_FORWARD;
                    }
                    startServerByStep();
                } else if (qsc::AdbProcess::AER_SUCCESS_START != processResult) {
                    qCritical("adb push failed");
                    m_serverStartStep = SSS_NULL;
                    emit serverStarted(false);
                }
                break;
            case SSS_ENABLE_TUNNEL_REVERSE:
                if (qsc::AdbProcess::AER_SUCCESS_EXEC == processResult) {
                    // At the application level, the device part is "the server" because it
                    // serves video stream and control. However, at the network level, the
                    // client listens and the server connects to the client. That way, the
                    // client can listen before starting the server app, so there is no need to
                    // try to connect until the server socket is listening on the device.
                    m_serverSocket.setMaxPendingConnections(2);
                    if (!m_serverSocket.listen(QHostAddress::LocalHost, m_params.localPort)) {
                        qCritical() << QString("Could not listen on port %1").arg(m_params.localPort).toStdString().c_str();
                        m_serverStartStep = SSS_NULL;
                        disableTunnelReverse();
                        emit serverStarted(false);
                        break;
                    }

                    m_serverStartStep = SSS_EXECUTE_SERVER;
                    startServerByStep();
                } else if (qsc::AdbProcess::AER_SUCCESS_START != processResult) {
                    // 有一些设备reverse会报错more than o'ne device，adb的bug
                    // https://github.com/Genymobile/scrcpy/issues/5
                    qCritical("adb reverse failed");
                    m_tunnelForward = true;
                    m_serverStartStep = SSS_ENABLE_TUNNEL_FORWARD;
                    startServerByStep();
                }
                break;
            case SSS_ENABLE_TUNNEL_FORWARD:
                if (qsc::AdbProcess::AER_SUCCESS_EXEC == processResult) {
                    m_serverStartStep = SSS_EXECUTE_SERVER;
                    startServerByStep();
                } else if (qsc::AdbProcess::AER_SUCCESS_START != processResult) {
                    qCritical("adb forward failed");
                    m_serverStartStep = SSS_NULL;
                    emit serverStarted(false);
                }
                break;
            default:
                break;
            }
        }
    }
    if (sender() == &m_serverProcess) {
        if (SSS_EXECUTE_SERVER == m_serverStartStep) {
            if (qsc::AdbProcess::AER_SUCCESS_START == processResult) {
                m_serverStartStep = SSS_RUNNING;
                m_tunnelEnabled = true;
                connectTo();
            } else if (qsc::AdbProcess::AER_ERROR_START == processResult) {
                if (!m_tunnelForward) {
                    m_serverSocket.close();
                    disableTunnelReverse();
                } else {
                    disableTunnelForward();
                }
                qCritical("adb shell start server failed");
                m_serverStartStep = SSS_NULL;
                emit serverStarted(false);
            }
        } else if (SSS_RUNNING == m_serverStartStep) {
            m_serverStartStep = SSS_NULL;
            emit serverStoped();
        }
    }
}
