#ifndef DECODER_H
#define DECODER_H
#include <QObject>

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavutil/hwcontext.h"
}

#include <functional>
#include <QSize>

class VideoBuffer;
class Decoder : public QObject
{
    Q_OBJECT
public:
    Decoder(std::function<void(int width, int height, uint8_t* dataY, uint8_t* dataU, uint8_t* dataV, int linesizeY, int linesizeU, int linesizeV)> onFrame, QObject *parent = Q_NULLPTR);
    virtual ~Decoder();

    bool open();
    void close();
    bool push(const AVPacket *packet);
    void peekFrame(std::function<void(int width, int height, uint8_t* dataRGB32)> onFrame);
    void setFrameSize(const QSize& frameSize);

signals:
    void updateFPS(quint32 fps);

private slots:
    void onNewFrame();

signals:
    void newFrame();

private:
    void pushFrame();
    bool openHardwareDecoder();
    bool openSoftwareDecoder();
    const char* getHardwareDecoderName(AVHWDeviceType type);

private:
    VideoBuffer *m_vb = Q_NULLPTR;
    AVCodecContext *m_codecCtx = Q_NULLPTR;
    AVBufferRef *m_hwDeviceCtx = Q_NULLPTR;
    AVFrame *m_hwFrame = Q_NULLPTR;
    bool m_isCodecCtxOpen = false;
    bool m_useHardwareDecoder = false;
    bool m_needsInitialization = true;  // Decoder needs open() to be called
    QSize m_frameSize;  // Frame dimensions from server
    std::function<void(int, int, uint8_t*, uint8_t*, uint8_t*, int, int, int)> m_onFrame = Q_NULLPTR;
};

#endif // DECODER_H
