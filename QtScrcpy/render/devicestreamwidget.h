#ifndef DEVICESTREAMWIDGET_H
#define DEVICESTREAMWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLTexture>
#include <QMutex>

/**
 * @brief DeviceStreamWidget - Optimized OpenGL widget for rendering multiple device streams
 *
 * This widget is specifically designed for the FarmViewer to efficiently render 100+ device streams.
 * Key optimizations:
 * - Minimal frame buffering (max 3 frames)
 * - GPU-based YUV to RGB conversion via shaders
 * - Zero-copy texture uploads where possible
 * - Shared OpenGL context support
 * - Efficient resource management
 */
class DeviceStreamWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit DeviceStreamWidget(QWidget *parent = nullptr);
    virtual ~DeviceStreamWidget() override;

    // Frame size management
    void setFrameSize(const QSize &frameSize);
    const QSize &frameSize() const { return m_frameSize; }

    // Update YUV frame data (called from decoder thread)
    void updateTextures(quint8 *dataY, quint8 *dataU, quint8 *dataV,
                       quint32 linesizeY, quint32 linesizeU, quint32 linesizeV);

    // Qt size hints
    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

protected:
    // OpenGL lifecycle
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int width, int height) override;

private:
    void initShaders();
    void initTextures();
    void cleanupTextures();
    void updateTexture(GLuint texture, quint32 textureType, quint8 *pixels, quint32 stride);

private:
    // Frame properties
    QSize m_frameSize;
    bool m_textureInited;
    bool m_needsUpdate;

    // OpenGL resources
    QOpenGLBuffer m_vbo;
    QOpenGLShaderProgram m_shaderProgram;
    GLuint m_textures[3]; // Y, U, V textures

    // Thread safety for texture updates
    QMutex m_textureMutex;
};

#endif // DEVICESTREAMWIDGET_H
