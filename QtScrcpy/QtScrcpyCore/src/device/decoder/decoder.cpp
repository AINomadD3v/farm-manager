#include <QDebug>

extern "C"
{
#include "libavutil/pixdesc.h"
#include "libavutil/opt.h"
}

#include "compat.h"
#include "decoder.h"
#include "videobuffer.h"

Decoder::Decoder(std::function<void(int, int, uint8_t*, uint8_t*, uint8_t*, int, int, int)> onFrame, QObject *parent)
    : QObject(parent)
    , m_vb(new VideoBuffer())
    , m_onFrame(onFrame)
{
    m_vb->init();
    connect(this, &Decoder::newFrame, this, &Decoder::onNewFrame, Qt::QueuedConnection);
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
        m_codecCtx->thread_count = 2; // 2 threads per decoder
        m_codecCtx->thread_type = FF_THREAD_FRAME;
        av_opt_set(m_codecCtx->priv_data, "preset", "ultrafast", 0);
        av_opt_set_int(m_codecCtx->priv_data, "delay", 0, 0);

        // Try to open the codec
        ret = avcodec_open2(m_codecCtx, codec, nullptr);
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
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        qCritical("H.264 software decoder not found");
        return false;
    }

    m_codecCtx = avcodec_alloc_context3(codec);
    if (!m_codecCtx) {
        qCritical("Could not allocate software decoder context");
        return false;
    }

    // Configure for low latency
    m_codecCtx->thread_count = 2; // 2 threads per decoder
    m_codecCtx->thread_type = FF_THREAD_FRAME;
    m_codecCtx->flags |= AV_CODEC_FLAG_LOW_DELAY;
    m_codecCtx->flags2 |= AV_CODEC_FLAG2_FAST;

    if (avcodec_open2(m_codecCtx, codec, nullptr) < 0) {
        qCritical("Could not open H.264 software codec");
        avcodec_free_context(&m_codecCtx);
        return false;
    }

    m_isCodecCtxOpen = true;
    m_useHardwareDecoder = false;
    qInfo() << "Using software decoder (CPU fallback)";
    return true;
}

bool Decoder::open()
{
    // Try hardware decoders first
    if (openHardwareDecoder()) {
        return true;
    }

    // Fallback to software decoder
    qWarning() << "All hardware decoders failed, falling back to software decoder";
    return openSoftwareDecoder();
}

void Decoder::close()
{
    if (m_vb) {
        m_vb->interrupt();
    }

    if (!m_codecCtx) {
        return;
    }
    if (m_isCodecCtxOpen) {
        avcodec_close(m_codecCtx);
    }
    avcodec_free_context(&m_codecCtx);
}

bool Decoder::push(const AVPacket *packet)
{
    if (!m_codecCtx || !m_vb) {
        return false;
    }
    AVFrame *decodingFrame = m_vb->decodingFrame();
#ifdef QTSCRCPY_LAVF_HAS_NEW_ENCODING_DECODING_API
    int ret = -1;
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

        //emit getOneFrame(yuvDecoderFrame->data[0], yuvDecoderFrame->data[1], yuvDecoderFrame->data[2],
        //        yuvDecoderFrame->linesize[0], yuvDecoderFrame->linesize[1], yuvDecoderFrame->linesize[2]);

        /*
        // m_conver转换yuv为rgb是使用cpu转的，占用cpu太高，改用opengl渲染yuv
        // QImage的copy也非常占用内存，此方案不考虑
        if (!m_conver.isInit()) {
            qDebug() << "decoder frame format" << decodingFrame->format;
            m_conver.setSrcFrameInfo(codecCtx->width, codecCtx->height, AV_PIX_FMT_YUV420P);
            m_conver.setDstFrameInfo(codecCtx->width, codecCtx->height, AV_PIX_FMT_RGB32);
            m_conver.init();
        }
        if (!outBuffer) {
            outBuffer=new quint8[avpicture_get_size(AV_PIX_FMT_RGB32, codecCtx->width, codecCtx->height)];
            avpicture_fill((AVPicture *)rgbDecoderFrame, outBuffer, AV_PIX_FMT_RGB32, codecCtx->width, codecCtx->height);
        }
        m_conver.convert(decodingFrame, rgbDecoderFrame);
        //QImage tmpImg((uchar *)outBuffer, codecCtx->width, codecCtx->height, QImage::Format_RGB32);
        //QImage image = tmpImg.copy();
        //emit getOneImage(image);
        */
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
    if (!m_onFrame) {
        return;
    }

    m_vb->lock();
    const AVFrame *frame = m_vb->consumeRenderedFrame();
    m_onFrame(frame->width, frame->height, frame->data[0], frame->data[1], frame->data[2], frame->linesize[0], frame->linesize[1], frame->linesize[2]);
    m_vb->unLock();
}
