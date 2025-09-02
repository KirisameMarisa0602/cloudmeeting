#pragma once
#include <QtCore>

// Qt 5.12.8 兼容包含：优先使用 Qt5 的模块化路径
#if __has_include(<QtMultimedia/QAudioInput>)
  #include <QtMultimedia/QAudioInput>
  #include <QtMultimedia/QAudioOutput>
  #include <QtMultimedia/QAudioFormat>
#else
  #include <QAudioInput>
  #include <QAudioOutput>
  #include <QAudioFormat>
#endif

#include "clientconn.h"
#include "protocol.h"

class AudioChat : public QObject {
    Q_OBJECT
public:
    explicit AudioChat(ClientConn* conn, QObject* parent = nullptr);

    void setIdentity(const QString& roomId, const QString& sender);

    void setEnabled(bool on);
    bool isEnabled() const { return enabled_; }

    void setPlaybackGain(float g) { playbackGain_ = qBound(0.0f, g, 2.0f); }
    float playbackGain() const { return playbackGain_; }

    void setMicGain(float g) { micGain_ = qBound(0.0f, g, 2.0f); }
    float micGain() const { return micGain_; }

    void setPeerGain(const QString& sender, float g) { peerGain_[sender] = qBound(0.0f, g, 2.0f); }
    float peerGain(const QString& sender) const { return peerGain_.value(sender, 1.0f); }
    void dropPeer(const QString& sender) { rxQueues_.remove(sender); peerGain_.remove(sender); }

public slots:
    void onPacket(Packet p);

signals:
    void micStateChanged(bool on);

private:
    static constexpr int   kSampleRate      = 8000;
    static constexpr int   kChannels        = 1;
    static constexpr int   kFrameMs         = 20;
    static constexpr int   kFrameSamples    = kSampleRate * kFrameMs / 1000;
    static constexpr int   kPcmBytesPerFrm  = kFrameSamples * 2;
    static constexpr int   kUlawBytesPerFrm = kFrameSamples;
    static constexpr int   kMaxQueueFrames  = 30;  // 减少到30帧(600ms)，更及时丢弃过期音频

    static quint8  linearToUlaw(qint16 pcm);
    static qint16  ulawToLinear(quint8 ul);

    void startInput();
    void stopInput();
    void ensureOutput();
    void onMicReadyRead();
    void mixTick();
    void shrinkQueueIfNeeded(QByteArray& q);

    ClientConn* conn_ = nullptr;
    QString roomId_;
    QString sender_;
    quint32 seq_ = 0;

    QAudioInput*  audioIn_  = nullptr;
    QIODevice*    inDev_    = nullptr;
    QAudioFormat  inFmt_;
    QByteArray    inBuf_;

    QAudioOutput* audioOut_ = nullptr;
    QIODevice*    outDev_   = nullptr;
    QAudioFormat  outFmt_;

    QTimer        mixTimer_;
    QHash<QString, QByteArray> rxQueues_;
    bool  enabled_       = false;
    float playbackGain_  = 1.0f;
    float micGain_       = 1.0f;
    QHash<QString, float> peerGain_;
};
