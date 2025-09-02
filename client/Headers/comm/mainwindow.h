#pragma once

#include <QMainWindow>
#include <QMap>
#include <QHash>
#include <QImage>
#include <QSize>
#include <QElapsedTimer>
#include <QVideoFrame>

class QWidget;
class QLabel;
class QToolButton;
class QTimer;
class QLineEdit;
class QTextEdit;
class QStackedWidget;
class QGridLayout;
class QComboBox;
class QPushButton;
class QScrollArea;
class QEvent;
class QResizeEvent;
class QCamera;
class QVideoProbe;

class AnnotCanvas;
class AnnotModel;
class AudioChat;
class ScreenShare;
class UdpMediaClient;

#include "clientconn.h" // ClientConn 为值成员，需要完整类型
#include "protocol.h"   // 使用 Packet

// 单个视频窗口（本地或远端）
struct VideoTile {
    QString      key;
    QWidget*     box       = nullptr;
    QLabel*      name      = nullptr;
    QLabel*      video     = nullptr;
    QToolButton* volBtn    = nullptr;
    QTimer*      timer     = nullptr;

    QImage       lastCam;
    QImage       lastScreen;
    bool         camPrimary = false;  // true: 相机为大图；false: 屏幕为大图
    int          volPercent = 100;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    enum class ViewMode { Grid, Focus };

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* ev) override;

private slots:
    void onConnect();
    void onJoin();
    void onLeave();               // 退出房间（断开连接）
    void onSendText();

    void onToggleCamera();
    void onToggleShare();

    void onVideoFrame(const QVideoFrame &frame);
    void onLocalScreenFrame(QImage img);

    void onPkt(Packet p);         // 只保留这一种签名
    void onHeartbeat();           // 心跳定时器槽

private:
    // 视图与排布
    ViewMode currentMode() const;
    VideoTile* ensureRemoteTile(const QString& sender);
    void removeRemoteTile(const QString& sender);
    void refreshGridOnly();
    void refreshFocusThumbs();
    void setTileWaiting(VideoTile* t, const QString& text = QStringLiteral("等待视频/屏幕..."));
    void kickRemoteAlive(VideoTile* t);
    void setMainKey(const QString& key);
    void updateMainFromTile(VideoTile* t);
    void updateAllThumbFitted();
    void updateMainFitted();
    void refreshTilePixmap(VideoTile* t);
    void togglePiP(VideoTile* t);
    QImage composeTileImage(const VideoTile* t, const QSize& target);

    // 摄像头/屏幕共享
    void startCamera();
    void stopCamera();
    void configureCamera(QCamera* cam);
    void hookCameraLogs(QCamera* cam);

    // 帧处理与发送
    QImage makeImageFromFrame(const QVideoFrame &frame);
    void updateLocalPreview(const QImage& img);
    void sendImage(const QImage& img);

    // 共享画质
    void applyShareQualityPreset();
    void applyAdaptiveByMembers(int members);

    // 心跳与拥塞控制
    void startHeartbeat();
    void stopHeartbeat();
    void sendPing();
    void handleCongestion(qint64 backlogBytes);
    void recoverStep();
    void showNetHint(const QString& message);

    // 音量弹窗 / 标注
    void bindVolumeButton(VideoTile* t, bool isLocal);
    AnnotModel* modelFor(const QString& key);

private:
    // 顶部输入
    QLineEdit* edHost = nullptr;
    QLineEdit* edPort = nullptr;
    QLineEdit* edUser = nullptr;
    QLineEdit* edRoom = nullptr;

    // 文本消息行（隐藏但保留对象）
    QLineEdit* edInput = nullptr;
    QTextEdit* txtLog  = nullptr;

    // 中央视图
    QStackedWidget* centerStack_ = nullptr;

    // Grid 模式
    QWidget*     gridPage_   = nullptr;
    QGridLayout* gridLayout_ = nullptr;

    // Focus 模式
    QWidget*     focusPage_           = nullptr;
    QWidget*     mainArea_            = nullptr;
    QLabel*      mainVideo_           = nullptr;
    QLabel*      mainName_            = nullptr;
    QWidget*     focusThumbContainer_ = nullptr;
    QGridLayout* focusThumbLayout_    = nullptr;

    // 底部按钮/下拉
    QPushButton* btnCamera_ = nullptr;
    QPushButton* btnMic_    = nullptr;
    QPushButton* btnShare_  = nullptr;
    QComboBox*   cbShareQ_  = nullptr;

    // 标注工具条
    QToolButton* btnAnnotOn_    = nullptr;
    QComboBox*   cbAnnotTool_   = nullptr;
    QComboBox*   cbAnnotWidth_  = nullptr;
    QToolButton* btnAnnotColor_ = nullptr;
    QToolButton* btnAnnotClear_ = nullptr;
    AnnotCanvas* annotCanvas_   = nullptr;
    QColor       annotColor_    = Qt::red;
    QMap<QString, AnnotModel*> annotModels_;

    // Tiles
    VideoTile                  localTile_;
    QMap<QString, VideoTile*>  remoteTiles_;
    QString                    mainKey_;
    static constexpr const char* kLocalKey_ = "__local__";

    // 屏幕增量还原背板
    QMap<QString, QImage>      screenBack_;

    // 媒体/网络
    ClientConn     conn_;
    AudioChat*     audio_ = nullptr;
    ScreenShare*   share_ = nullptr;
    UdpMediaClient* udp_  = nullptr;

    // 摄像头
    QCamera*                     camera_ = nullptr;
    QVideoProbe*                 probe_  = nullptr;
    QVideoFrame::PixelFormat     lastLoggedFormat_ = QVideoFrame::Format_Invalid;

    // 发送参数
    QSize                        sendSize_ = QSize(640, 480);
    int                          targetFps_ = 10;
    int                          jpegQuality_ = 55;
    QElapsedTimer                lastSend_;

    // 心跳与拥塞控制
    QTimer                       heartbeatTimer_;
    QElapsedTimer                lastCongestion_;
    bool                         inCongestion_ = false;
};
