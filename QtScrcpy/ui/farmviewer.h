#ifndef FARMVIEWER_H
#define FARMVIEWER_H

#include <QWidget>
#include <QGridLayout>
#include <QScrollArea>
#include <QLabel>
#include <QMap>
#include <QPointer>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include "adbprocess.h"

class VideoForm;

namespace Ui
{
    class FarmViewer;
}

class FarmViewer : public QWidget
{
    Q_OBJECT

public:
    static FarmViewer& instance();
    ~FarmViewer();

    void addDevice(const QString& serial, const QString& deviceName, const QSize& size);
    void removeDevice(const QString& serial);
    void setGridSize(int rows, int cols);
    void showFarmViewer();
    void hideFarmViewer();
    void autoDetectAndConnectDevices();
    bool isVisible() const;
    bool isManagingDevice(const QString& serial) const;

    static const QString &getServerPath();

private slots:
    void onScreenshotAllClicked();
    void onSyncActionClicked();
    void onGridSizeChanged();

private:
    explicit FarmViewer(QWidget *parent = nullptr);
    void setupUI();
    void updateGridLayout();
    void updateStatus();
    void createDeviceContainer(const QString& serial, const QString& deviceName);
    QWidget* createDeviceWidget(const QString& serial, const QString& deviceName);
    void connectToDevice(const QString& serial);
    void processDetectedDevices(const QStringList& devices);

    Ui::FarmViewer *ui;
    QScrollArea* m_scrollArea;
    QWidget* m_gridWidget;
    QGridLayout* m_gridLayout;
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_toolbarLayout;
    
    // Device management
    QMap<QString, QPointer<VideoForm>> m_deviceForms;
    QMap<QString, QPointer<QWidget>> m_deviceContainers;
    
    // Grid configuration
    int m_gridRows;
    int m_gridCols;
    
    // Controls
    QPushButton* m_screenshotAllBtn;
    QPushButton* m_syncActionBtn;
    QLabel* m_statusLabel;
    
    // Device detection
    qsc::AdbProcess m_deviceDetectionAdb;
    
    static FarmViewer* s_instance;
};

#endif // FARMVIEWER_H