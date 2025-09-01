#pragma once

// 基础
#include <QtGlobal>
#include <QMainWindow>
#include <QImage>
#include <QSize>
#include <QElapsedTimer>
#include <QHash>
#include <QMap>

// Qt Multimedia 头文件兼容包含（Qt 5.12）
#if __has_include(<QCamera>)
  #include <QCamera>
#elif __has_include(<QtMultimedia/QCamera>)
  #include <QtMultimedia/QCamera>
#else
  #error "QCamera header not found. Please ensure 'QT += multimedia multimediawidgets' is set in client.pro and Qt Multimedia module is installed."
#endif

#if __has_include(<QVideoProbe>)
  #include <QVideoProbe>
#elif __has_include(<QtMultimedia/QVideoProbe>)
  #include <QtMultimedia/QVideoProbe>
#else
  #error "QVideoProbe header not found. Please ensure 'QT += multimedia multimediawidgets' is set in client.pro and Qt Multimedia module is installed."
#endif

#if __has_include(<QVideoFrame>)
  #include <QVideoFrame>
#elif __has_include(<QtMultimedia/QVideoFrame>)
  #include <QtMultimedia/QVideoFrame>
#else
  #error "QVideoFrame header not found. Please ensure 'QT += multimedia multimediawidgets' is set in client.pro and Qt Multimedia module is installed."
#endif

// 前置声明（指针成员用前置声明即可）
class AnnotCanvas;
class AnnotModel;
class QComboBox;
class QColor;
class QLineEdit;
class QPushButton;
class QLabel;
class QTextEdit;
class QGridLayout;
class QWidget;
class QStackedWidget;
class QToolButton;
class QTimer;

// 你项目内的组件（按值成员需要完整类型）
#include "clientconn.h"     // ClientConn 为按值成员，必须包含
#include "audiochat.h"      // AudioChat* 指针（如有需要可改为前置声明+在 .cpp 包含）
#include "screenshare.h"    // ScreenShare* 指针
#include "udpmedia.h"       // UdpMediaClient* 指针（若你的头文件名不同，请对应修改）

// 你的数据包类型（仅做声明即可）
struct Packet;

struct VideoTile {
    QWidget*     box = nullptr;
    QLabel*      name = nullptr;
    QLabel*      video = nullptr;
    QToolButton* volBtn = nullptr;
    QTimer*      timer = nullptr;
    QString      key;
    QImage       lastCam;
    QImage       lastScreen;
    int          volPercent = 100;
    bool         camPrimary = false;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    void startCamera();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* ev) override;

private slots:
    void onConnect();
    void onJoin();
    void onSendText();
    void onPkt(Packet p);

    void onToggleCamera();
    void onVideoFrame(const QVideoFrame &frame);

    void onLocalScreenFrame(QImage img);
    void onToggleShare();

private:
    // 标注相关
    QToolButton *btnAnnotOn_{};
    QComboBox   *cbAnnotTool_{};
    QComboBox   *cbAnnotWidth_{};
    QToolButton *btnAnnotColor_{};
    QToolButton *btnAnnotClear_{};
    QColor       annotColor_{Qt::red};
    AnnotCanvas *annotCanvas_{nullptr};
    QHash<QString, AnnotModel*> annotModels_;
    AnnotModel* modelFor(const QString& key);

    // 摄像头
    void stopCamera();
    void configureCamera(QCamera* cam);
    void hookCameraLogs(QCamera* cam);

    // 视频帧/图像处理
    QImage makeImageFromFrame(const QVideoFrame &frame);
    void updateLocalPreview(const QImage& img);
    void sendImage(const QImage& img);

    // 视图布局
    enum class ViewMode { Grid, Focus };
    ViewMode currentMode() const;

    void refreshGridOnly();
    void refreshFocusThumbs();
    void setMainKey(const QString& key);
    void updateMainFromTile(VideoTile* t);
    void updateMainFitted();

    VideoTile* ensureRemoteTile(const QString& sender);
    void removeRemoteTile(const QString& sender);
    void setTileWaiting(VideoTile* t, const QString& text = QStringLiteral("等待视频/屏幕..."));
    void kickRemoteAlive(VideoTile* t);
    void updateAllThumbFitted();

    void bindVolumeButton(VideoTile* t, bool isLocal);

    static QImage composeTileImage(const VideoTile* t, const QSize& target);
    void refreshTilePixmap(VideoTile* t);
    void togglePiP(VideoTile* t);

    void applyAdaptiveByMembers(int members);
    void applyShareQualityPreset();

private:
    // 顶部连接与输入
    QLineEdit *edHost{};
    QLineEdit *edPort{};
    QLineEdit *edUser{};
    QLineEdit *edRoom{};
    QLineEdit *edInput{};
    QTextEdit *txtLog{};

    // 底部按钮与共享设置
    QPushButton *btnCamera_{};
    QPushButton *btnMic_{};
    QPushButton *btnShare_{};
    QComboBox  *cbShareQ_{};

    // 中心栈与布局
    QStackedWidget* centerStack_{};
    QWidget*   gridPage_{};
    QGridLayout* gridLayout_{};

    QWidget*   focusPage_{};
    QWidget*   mainArea_{};
    QLabel*    mainVideo_{};
    QLabel*    mainName_{};
    QWidget*   focusThumbContainer_{};
    QGridLayout* focusThumbLayout_{};

    // 本地与远端画面
    VideoTile localTile_;
    QMap<QString, VideoTile*> remoteTiles_;
    const QString kLocalKey_ = QStringLiteral("__local__");
    QString mainKey_;

    // 网络与媒体
    ClientConn      conn_;
    AudioChat*      audio_{nullptr};
    ScreenShare*    share_{nullptr};
    UdpMediaClient* udp_{nullptr};

    // 相机与探针
    QCamera*      camera_{nullptr};
    QVideoProbe*  probe_{nullptr};

    // 发送参数
    int   targetFps_{12};
    int   jpegQuality_{60};
    QSize sendSize_{640, 480};
    QElapsedTimer lastSend_;
    QVideoFrame::PixelFormat lastLoggedFormat_{QVideoFrame::Format_Invalid};

    // 屏幕回退
    QHash<QString, QImage> screenBack_;
};
