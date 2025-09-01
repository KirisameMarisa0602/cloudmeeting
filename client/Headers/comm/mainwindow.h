#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QCamera>
#include <QVideoProbe>
#include <QVideoFrame>
#include <QElapsedTimer>
#include <QTimer>
#include <QMap>
#include <QImage>
#include "annot.h"
#include "clientconn.h"
#include "audiochat.h"
#include "screenshare.h"

class AnnotCanvas;
class QComboBox;
class QColorDialog;
class QLineEdit;
class QPushButton;
class QLabel;
class QTextEdit;
class QGridLayout;
class QWidget;
class QStackedWidget;
class QToolButton;
class QComboBox;        // 新增
class UdpMediaClient;

struct VideoTile {
    QWidget* box = nullptr;
    QLabel*  name = nullptr;
    QLabel*  video = nullptr;
    QToolButton* volBtn = nullptr;
    QTimer*  timer = nullptr;
    QString  key;
    QImage   lastCam;
    QImage   lastScreen;
    int      volPercent = 100;
    bool     camPrimary = false;
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
    QToolButton *btnAnnotOn_{};
        QComboBox   *cbAnnotTool_{};
        QComboBox   *cbAnnotWidth_{};
        QToolButton *btnAnnotColor_{};
        QToolButton *btnAnnotClear_{};
        QColor       annotColor_{Qt::red};

        // 画布（叠在主画面上）
        AnnotCanvas *annotCanvas_{nullptr};

        // 每个目标（tile key）一份标注模型
        QHash<QString, AnnotModel*> annotModels_;

        AnnotModel* modelFor(const QString& key);

    void stopCamera();

    void configureCamera(QCamera* cam);
    void hookCameraLogs(QCamera* cam);

    QImage makeImageFromFrame(const QVideoFrame &frame);
    void updateLocalPreview(const QImage& img);
    void sendImage(const QImage& img);

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
    QLineEdit *edHost{};
    QLineEdit *edPort{};
    QLineEdit *edUser{};
    QLineEdit *edRoom{};
    QLineEdit *edInput{};
    QTextEdit *txtLog{};
    QPushButton *btnCamera_{};
    QPushButton *btnMic_{};
    QPushButton *btnShare_{};
    QComboBox  *cbShareQ_{};

    QStackedWidget* centerStack_{};
    QWidget*   gridPage_{};
    QGridLayout* gridLayout_{};

    QWidget*   focusPage_{};
    QWidget*   mainArea_{};
    QLabel*    mainVideo_{};
    QLabel*    mainName_{};
    QWidget*   focusThumbContainer_{};
    QGridLayout* focusThumbLayout_{};

    VideoTile localTile_;
    QMap<QString, VideoTile*> remoteTiles_;
    const QString kLocalKey_ = QStringLiteral("__local__");
    QString mainKey_;

    ClientConn conn_;

    AudioChat*     audio_{nullptr};
    ScreenShare*   share_{nullptr};
    UdpMediaClient* udp_{nullptr};

    QCamera *camera_{nullptr};
    QVideoProbe *probe_{nullptr};

    int targetFps_{12};
    int jpegQuality_{60};
    QSize sendSize_{640, 480};
    QElapsedTimer lastSend_;
    QVideoFrame::PixelFormat lastLoggedFormat_{QVideoFrame::Format_Invalid};

    QHash<QString, QImage> screenBack_;
};

#endif // MAINWINDOW_H