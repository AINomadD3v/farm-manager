#include <QDebug>
#include <QMutexLocker>
#include <QThread>

extern "C"
{
#include "libavutil/pixdesc.h"
#include "libavutil/opt.h"
}

#include "compat.h"
#include "decoder.h"
#include "videobuffer.h"

// CRITICAL: Global mutex to serialize avcodec_open2() and avcodec_close() calls
// FFmpeg 7.x requires external synchronization for these functions when called
// from multiple threads, as they access global codec initialization state.
// Without this mutex, simultaneous decoder initialization (21+ devices) causes
// race conditions and SIGSEGV crashes inside FFmpeg's internal codec tables.
static QMutex g_avcodecMutex;

Decoder::Decoder(std::function<void(int, int, uint8_t*, uint8_t*, uint8_t*, int, int, int)> onFrame, QObject *parent)
    : QObject(parent)
    , m_vb(new VideoBuffer())
    , m_onFrame(onFrame)
{
    m_vb->init();
    // CRITICAL: Use DirectConnection because Decoder runs in Demuxer thread which has no event loop
    // QueuedConnection would never deliver the signal
    connect(this, &Decoder::newFrame, this, &Decoder::onNewFrame, Qt::DirectConnection);
    connect(m_vb, &VideoBuffer::updateFPS, this, &Decoder::updateFPS);
}

Decoder::~Decoder() {
    m_vb->deInit();
    delete m_vb;
    if (m_hwFrame) {
        av_frame_free(&m_hwFrame);
    }
    if (m_hwDeviceCtx) {
        av_buffer_unref(&m_hwDeviceCtx);
    }
}

void Decoder::setFrameSize(const QSize& frameSize)
{
    qInfo() << "Decoder::setFrameSize() called with size:" << frameSize;
    m_frameSize = frameSize;
}

const char* Decoder::getHardwareDecoderName(AVHWDeviceType type)
{
    switch (type) {
        case AV_HWDEVICE_TYPE_VAAPI:
            return "h264_vaapi";
        case AV_HWDEVICE_TYPE_QSV:
            return "h264_qsv";
        case AV_HWDEVICE_TYPE_CUDA:
            return "h264_cuvid";
        default:
            return nullptr;
    }
}

bool Decoder::openHardwareDecoder()
{
    // Priority list: VAAPI (best for Linux/Intel) -> QSV -> CUDA/NVDEC
    AVHWDeviceType hardwareTypes[] = {
        AV_HWDEVICE_TYPE_VAAPI,
        AV_HWDEVICE_TYPE_QSV,
        AV_HWDEVICE_TYPE_CUDA
    };

    for (AVHWDeviceType hwType : hardwareTypes) {
        const char* hwDecoderName = getHardwareDecoderName(hwType);
        if (!hwDecoderName) {
            continue;
        }

        const AVCodec* codec = avcodec_find_decoder_by_name(hwDecoderName);
        if (!codec) {
            qDebug() << "Hardware decoder not available:" << hwDecoderName;
            continue;
        }

        // Allocate codec context
        m_codecCtx = avcodec_alloc_context3(codec);
        if (!m_codecCtx) {
            qWarning() << "Could not allocate hardware decoder context for:" << hwDecoderName;
            continue;
        }

        // Create hardware device context
        int ret = av_hwdevice_ctx_create(&m_hwDeviceCtx, hwType, nullptr, nullptr, 0);
        if (ret < 0) {
            char errBuf[256];
            av_strerror(ret, errBuf, sizeof(errBuf));
            qDebug() << "Failed to create hardware device context for" << hwDecoderName << ":" << errBuf;
            avcodec_free_context(&m_codecCtx);
            continue;
        }

        // Set hardware device context
        m_codecCtx->hw_device_ctx = av_buffer_ref(m_hwDeviceCtx);

        // Configure for low latency
        // Reduce thread count for high device counts to prevent thread exhaustion
        m_codecCtx->thread_count = 1; // 1 thread per decoder (optimized for 78+ devices)
        // DISABLED thread_type - causes crash in avcodec_open2() with FFmpeg 7.x
        // m_codecCtx->thread_type = FF_THREAD_FRAME;
        av_opt_set(m_codecCtx->priv_data, "preset", "ultrafast", 0);
        av_opt_set_int(m_codecCtx->priv_data, "delay", 0, 0);

        // Try to open the codec (MUST be serialized across all threads)
        {
            QMutexLocker locker(&g_avcodecMutex);
            ret = avcodec_open2(m_codecCtx, codec, nullptr);
        }
        if (ret < 0) {
            char errBuf[256];
            av_strerror(ret, errBuf, sizeof(errBuf));
            qWarning() << "Could not open hardware codec" << hwDecoderName << ":" << errBuf;
            av_buffer_unref(&m_hwDeviceCtx);
            avcodec_free_context(&m_codecCtx);
            continue;
        }

        // Allocate hardware frame for receiving decoded data
        m_hwFrame = av_frame_alloc();
        if (!m_hwFrame) {
            qCritical("Could not allocate hardware frame");
            av_buffer_unref(&m_hwDeviceCtx);
            avcodec_free_context(&m_codecCtx);
            continue;
        }

        m_isCodecCtxOpen = true;
        m_useHardwareDecoder = true;
        qInfo() << "Successfully opened hardware decoder:" << hwDecoderName;
        return true;
    }

    return false;
}

bool Decoder::openSoftwareDecoder()
{
    qInfo() << "========================================";
    qInfo() << "openSoftwareDecoder: START";

    qInfo() << "openSoftwareDecoder: Finding H.264 codec...";
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        qCritical("H.264 software decoder not found");
        return false;
    }
    qInfo() << "openSoftwareDecoder: H.264 codec found:" << codec->name;
    qInfo() << "  Codec long_name:" << (codec->long_name ? codec->long_name : "N/A");
    qInfo() << "  Codec type:" << codec->type;
    qInfo() << "  Codec capabilities:" << codec->capabilities;

    qInfo() << "openSoftwareDecoder: Allocating codec context...";
    m_codecCtx = avcodec_alloc_context3(codec);
    if (!m_codecCtx) {
        qCritical("Could not allocate software decoder context");
        return false;
    }
    qInfo() << "openSoftwareDecoder: Codec context allocated at:" << (void*)m_codecCtx;

    // CRITICAL: Validate codec context before configuration
    if (!m_codecCtx->codec) {
        qInfo() << "  Note: m_codecCtx->codec is NULL (expected before avcodec_open2)";
    }

    // Configure for low latency
    // Reduce thread count for high device counts to prevent thread exhaustion
    qInfo() << "openSoftwareDecoder: Configuring codec context fields...";

    // CRITICAL: Use EXACT same configuration as startup test (which works)
    // Set basic codec parameters (some decoders require these before open)
    m_codecCtx->pix_fmt = AV_PIX_FMT_YUV420P;  // Expected output format

    // IMPORTANT: DO NOT set width/height before avcodec_open2()
    // FFmpeg H.264 decoder extracts dimensions from SPS/PPS in the bitstream
    // Pre-setting dimensions can cause conflicts and crashes
    qInfo() << "openSoftwareDecoder: NOT setting dimensions - FFmpeg will extract from SPS/PPS";

    m_codecCtx->thread_count = 1; // 1 thread per decoder (optimized for 78+ devices)
    // DISABLED thread_type - causes crash in avcodec_open2() with FFmpeg 7.x
    // m_codecCtx->thread_type = FF_THREAD_FRAME;
    m_codecCtx->flags |= AV_CODEC_FLAG_LOW_DELAY;
    // DISABLED flags2 - test if this causes the crash
    // m_codecCtx->flags2 |= AV_CODEC_FLAG2_FAST;

    qInfo() << "openSoftwareDecoder: Codec context configuration:";
    qInfo() << "  pix_fmt:" << m_codecCtx->pix_fmt << "(should be" << AV_PIX_FMT_YUV420P << ")";
    qInfo() << "  thread_count:" << m_codecCtx->thread_count;
    qInfo() << "  thread_type:" << m_codecCtx->thread_type;
    qInfo() << "  flags:" << m_codecCtx->flags;
    qInfo() << "  flags2:" << m_codecCtx->flags2;

    // Verify codec is valid before attempting to open
    if (!codec->name) {
        qCritical() << "openSoftwareDecoder: CRITICAL - codec->name is NULL!";
        avcodec_free_context(&m_codecCtx);
        return false;
    }

    qInfo() << "openSoftwareDecoder: Pre-open validation complete, ready to open codec";
    qInfo() << "openSoftwareDecoder: Codec being opened in thread:" << QThread::currentThreadId();
    qInfo() << "openSoftwareDecoder: ACQUIRING GLOBAL MUTEX...";
    int ret = -1;
    {
        QMutexLocker locker(&g_avcodecMutex);
        qInfo() << "openSoftwareDecoder: MUTEX ACQUIRED";
        qInfo() << "openSoftwareDecoder: About to call avcodec_open2()...";
        qInfo() << "  codec pointer:" << (void*)codec;
        qInfo() << "  codec_ctx pointer:" << (void*)m_codecCtx;
        qInfo() << "  options: nullptr";

        // The crash happens here - let's see if we even get past this line
        ret = avcodec_open2(m_codecCtx, codec, nullptr);

        qInfo() << "openSoftwareDecoder: avcodec_open2() RETURNED with ret:" << ret;
    }
    qInfo() << "openSoftwareDecoder: MUTEX RELEASED";

    if (ret < 0) {
        char errBuf[256];
        av_strerror(ret, errBuf, sizeof(errBuf));
        qCritical() << "Could not open H.264 software codec:" << errBuf << "ret:" << ret;
        avcodec_free_context(&m_codecCtx);
        return false;
    }
    qInfo() << "openSoftwareDecoder: Codec opened successfully";

    m_isCodecCtxOpen = true;
    m_useHardwareDecoder = false;
    qInfo() << "openSoftwareDecoder: Software decoder ready (CPU fallback)";
    qInfo() << "========================================";
    return true;
}

bool Decoder::open()
{
    qInfo() << "========================================";
    qInfo() << "Decoder::open() START in thread:" << QThread::currentThreadId();

    // Check for forced software decoding (for cross-machine compatibility)
    bool forceSoftware = qEnvironmentVariableIsSet("QTSCRCPY_SOFTWARE_DECODER") ||
                         qEnvironmentVariableIsSet("LIBGL_ALWAYS_SOFTWARE");

    if (forceSoftware) {
        qInfo() << "Decoder::open() - Software decoding forced via environment variable";
        qInfo() << "Decoder::open() - Skipping hardware decoder attempts";
    } else {
        // Try hardware decoders first
        if (openHardwareDecoder()) {
            // CRITICAL: Add settle time for hardware decoder
            // avcodec_open2() returns immediately but FFmpeg's internal state needs time to stabilize
            // Without this delay, immediate calls to avcodec_send_packet() can crash
            qInfo() << "Decoder::open() - Hardware decoder opened, adding settle time...";
            QThread::msleep(50);
            qInfo() << "Decoder::open() - Hardware decoder ready after settle time";
            qInfo() << "========================================";
            return true;
        }
    }

    // Fallback to software decoder
    if (!forceSoftware) {
        qWarning() << "All hardware decoders failed, falling back to software decoder";
    }
    bool result = openSoftwareDecoder();

    if (result) {
        // CRITICAL: Add settle time for software decoder
        // This is ESSENTIAL - avcodec_open2() returns but internal codec state may not be ready
        // The crash occurred because we called avcodec_send_packet() immediately after open
        qInfo() << "Decoder::open() - Software decoder opened, adding settle time...";
        QThread::msleep(100);
        qInfo() << "Decoder::open() - Software decoder ready after settle time";
    }

    qInfo() << "Decoder::open() COMPLETE - result:" << result;
    qInfo() << "========================================";
    return result;
}

void Decoder::close()
{
    if (m_vb) {
        m_vb->interrupt();
    }

    if (!m_codecCtx) {
        return;
    }
    // In FFmpeg 7.x, avcodec_free_context() automatically closes the codec
    // avcodec_close() is deprecated and no longer needed
    avcodec_free_context(&m_codecCtx);
    m_isCodecCtxOpen = false;
}

bool Decoder::push(const AVPacket *packet)
{
    // PERFORMANCE OPTIMIZATION: Removed excessive logging from hot path
    // Previous: ~45 log lines per packet = massive overhead with 78 devices
    // Now: Only log errors and first-time initialization

    if (!packet) {
        qCritical() << "Decoder::push() - NULL packet!";
        return false;
    }

    // CRITICAL: Initialize decoder on first packet (in Demuxer thread)
    // Can't use QMetaObject::invokeMethod because Demuxer::run() doesn't have event loop
    // So we initialize lazily on first push() call, which IS in the Demuxer thread
    if (m_needsInitialization) {
        qInfo() << "Decoder::push() - First packet, initializing decoder in thread:" << QThread::currentThreadId();
        m_needsInitialization = false;
        if (!open()) {
            qCritical() << "Decoder::push() - CRITICAL: open() failed!";
            return false;
        }
        qInfo() << "Decoder::push() - Decoder initialized successfully";
    }

    if (!m_codecCtx) {
        qCritical() << "Decoder::push() - NULL codec context!";
        return false;
    }

    if (!m_vb) {
        qCritical() << "Decoder::push() - NULL video buffer!";
        return false;
    }

    if (!m_codecCtx->codec) {
        qCritical() << "Decoder::push() - Codec context has NULL codec!";
        return false;
    }

    if (!m_isCodecCtxOpen) {
        qCritical() << "Decoder::push() - Codec context is not open!";
        return false;
    }

    if (!packet->data || packet->size <= 0) {
        qCritical() << "Decoder::push() - Invalid packet data!";
        return false;
    }

    AVFrame *decodingFrame = m_vb->decodingFrame();

    if (!decodingFrame) {
        qCritical() << "Decoder::push() - NULL decoding frame!";
        return false;
    }

    // CRITICAL: Ensure frame is in clean state for FFmpeg to fill
    // With the new FFmpeg API (send/receive), we do NOT pre-set frame dimensions.
    // The decoder fills in width/height/format after processing SPS/PPS data.
    if (decodingFrame->format != AV_PIX_FMT_NONE ||
        decodingFrame->width > 0 ||
        decodingFrame->height > 0) {
        av_frame_unref(decodingFrame);
    }

#ifdef QTSCRCPY_LAVF_HAS_NEW_ENCODING_DECODING_API
    int ret = -1;

    // CRITICAL: Use packet directly - NO cloning needed
    // Qt::DirectConnection ensures this function executes synchronously in the Demuxer thread,
    // meaning the packet is GUARANTEED valid during the entire function execution.
    if ((ret = avcodec_send_packet(m_codecCtx, packet)) < 0) {
        char errorbuf[255] = { 0 };
        av_strerror(ret, errorbuf, 254);
        qCritical("Could not send video packet: %s", errorbuf);
        return false;
    }

    // For hardware decoders, we need to transfer data from GPU to CPU
    if (m_useHardwareDecoder && decodingFrame && m_hwFrame) {
        ret = avcodec_receive_frame(m_codecCtx, m_hwFrame);
        if (!ret) {
            // Transfer data from GPU to CPU memory
            ret = av_hwframe_transfer_data(decodingFrame, m_hwFrame, 0);
            if (ret < 0) {
                char errorbuf[255] = { 0 };
                av_strerror(ret, errorbuf, 254);
                qCritical("Error transferring hardware frame to system memory: %s", errorbuf);
                return false;
            }
            // Copy frame properties
            av_frame_copy_props(decodingFrame, m_hwFrame);
            pushFrame();
        } else if (ret != AVERROR(EAGAIN)) {
            qCritical("Could not receive hardware video frame: %d", ret);
            return false;
        }
    } else if (decodingFrame) {
        // Software decoder path
        ret = avcodec_receive_frame(m_codecCtx, decodingFrame);

        if (!ret) {
            // a frame was received
            pushFrame();
        } else if (ret != AVERROR(EAGAIN)) {
            qCritical("Could not receive video frame: %d", ret);
            return false;
        }
    }
#else
    int gotPicture = 0;
    int len = -1;
    if (decodingFrame) {
        len = avcodec_decode_video2(m_codecCtx, decodingFrame, &gotPicture, packet);
    }
    if (len < 0) {
        qCritical("Could not decode video packet: %d", len);
        return false;
    }
    if (gotPicture) {
        pushFrame();
    }
#endif
    return true;
}

void Decoder::peekFrame(std::function<void (int, int, uint8_t *)> onFrame)
{
    if (!m_vb) {
        return;
    }
    m_vb->peekRenderedFrame(onFrame);
}

void Decoder::pushFrame()
{
    if (!m_vb) {
        return;
    }
    bool previousFrameSkipped = true;
    m_vb->offerDecodedFrame(previousFrameSkipped);
    if (previousFrameSkipped) {
        // the previous newFrame will consume this frame
        return;
    }
    emit newFrame();
}

void Decoder::onNewFrame() {
    // PERFORMANCE OPTIMIZATION: Removed excessive logging from hot path
    // Previous: 11 log lines per frame (~91,260 log ops/sec with 78 devices @ 15 FPS)
    // Now: Only log errors

    if (!m_onFrame) {
        qWarning() << "Decoder::onNewFrame() - m_onFrame callback is NULL!";
        return;
    }

    if (!m_vb) {
        qCritical() << "Decoder::onNewFrame() - m_vb is NULL!";
        return;
    }

    try {
        m_vb->lock();
    } catch (...) {
        qCritical() << "Decoder::onNewFrame() - EXCEPTION while locking video buffer!";
        throw;
    }

    const AVFrame *frame = m_vb->consumeRenderedFrame();

    if (!frame) {
        qCritical() << "Decoder::onNewFrame() - Frame is NULL!";
        m_vb->unLock();
        return;
    }

    try {
        m_onFrame(frame->width, frame->height, frame->data[0], frame->data[1], frame->data[2], frame->linesize[0], frame->linesize[1], frame->linesize[2]);
    } catch (const std::exception& e) {
        qCritical() << "Decoder::onNewFrame() - EXCEPTION in m_onFrame callback:" << e.what();
    } catch (...) {
        qCritical() << "Decoder::onNewFrame() - UNKNOWN EXCEPTION in m_onFrame callback";
    }

    m_vb->unLock();
}
