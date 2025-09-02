#include "audiochat.h"
#include <QDateTime>
#include <cmath>

// Qt 5.12.8 兼容的设备信息头
#if __has_include(<QtMultimedia/QAudioDeviceInfo>)
  #include <QtMultimedia/QAudioDeviceInfo>
#else
  #include <QAudioDeviceInfo>
#endif

static inline qint16 clamp16(int v) {
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return static_cast<qint16>(v);
}

// µ-law 实现（G.711）
quint8 AudioChat::linearToUlaw(qint16 pcm) {
    const int BIAS = 0x84;
    const int CLIP = 32635;
    int sign = (pcm >> 8) & 0x80;
    if (sign) pcm = -pcm;
    if (pcm > CLIP) pcm = CLIP;
    pcm += BIAS;
    int exponent = 7;
    for (int expMask = 0x4000; (pcm & expMask) == 0 && exponent > 0; expMask >>= 1) {
        --exponent;
    }
    int mantissa = (pcm >> (exponent + 3)) & 0x0F;
    return static_cast<quint8>(~(sign | (exponent << 4) | mantissa));
}

qint16 AudioChat::ulawToLinear(quint8 u) {
    u = ~u;
    int t = ((u & 0x0F) << 3) + 0x84;
    t <<= ((u & 0x70) >> 4);
    return (u & 0x80) ? (0x84 - t) : (t - 0x84);
}

AudioChat::AudioChat(ClientConn* conn, QObject* parent)
    : QObject(parent), conn_(conn)
{
    // 混音定时器：按帧长输出
    mixTimer_.setInterval(kFrameMs);
    connect(&mixTimer_, &QTimer::timeout, this, &AudioChat::mixTick);
    mixTimer_.start();

    // 确保输出设备可用
    ensureOutput();
}

void AudioChat::setIdentity(const QString& roomId, const QString& sender) {
    roomId_ = roomId;
    sender_ = sender;
}

void AudioChat::setEnabled(bool on) {
    if (enabled_ == on) return;
    enabled_ = on;
    if (enabled_) startInput();
    else          stopInput();
    emit micStateChanged(enabled_);
}

void AudioChat::startInput() {
    if (audioIn_) return;

    QAudioFormat fmt;
    fmt.setSampleRate(kSampleRate);
    fmt.setChannelCount(kChannels);
    fmt.setSampleSize(16);
    fmt.setSampleType(QAudioFormat::SignedInt);
    fmt.setByteOrder(QAudioFormat::LittleEndian);
    fmt.setCodec("audio/pcm");
    inFmt_ = fmt;

    QAudioDeviceInfo devInfo = QAudioDeviceInfo::defaultInputDevice();
    if (!devInfo.isFormatSupported(fmt)) {
        inFmt_ = devInfo.nearestFormat(fmt);
        inFmt_.setCodec("audio/pcm");
    }
    audioIn_ = new QAudioInput(devInfo, inFmt_, this);
    audioIn_->setBufferSize(kPcmBytesPerFrm * 4);

    inDev_ = audioIn_->start();
    if (!inDev_) {
        qWarning() << "AudioInput start failed";
        delete audioIn_; audioIn_ = nullptr;
        return;
    }
    connect(inDev_, &QIODevice::readyRead, this, &AudioChat::onMicReadyRead);
}

void AudioChat::stopInput() {
    if (!audioIn_) return;
    disconnect(inDev_, &QIODevice::readyRead, this, &AudioChat::onMicReadyRead);
    audioIn_->stop();
    audioIn_->deleteLater();
    audioIn_ = nullptr;
    inDev_ = nullptr;
    inBuf_.clear();
}

void AudioChat::ensureOutput() {
    if (audioOut_) return;

    QAudioFormat fmt;
    fmt.setSampleRate(kSampleRate);
    fmt.setChannelCount(kChannels);
    fmt.setSampleSize(16);
    fmt.setSampleType(QAudioFormat::SignedInt);
    fmt.setByteOrder(QAudioFormat::LittleEndian);
    fmt.setCodec("audio/pcm");
    outFmt_ = fmt;

    QAudioDeviceInfo devInfo = QAudioDeviceInfo::defaultOutputDevice();
    if (!devInfo.isFormatSupported(fmt)) {
        outFmt_ = devInfo.nearestFormat(fmt);
        outFmt_.setCodec("audio/pcm");
    }
    audioOut_ = new QAudioOutput(devInfo, outFmt_, this);
    audioOut_->setBufferSize(kPcmBytesPerFrm * 20);
    outDev_ = audioOut_->start();
    if (!outDev_) {
        qWarning() << "AudioOutput start failed";
        delete audioOut_; audioOut_ = nullptr;
        return;
    }
}

void AudioChat::onMicReadyRead() {
    if (!inDev_ || roomId_.isEmpty() || sender_.isEmpty()) { if (inDev_) inDev_->readAll(); return; }

    inBuf_.append(inDev_->readAll());

    // 每帧按 20ms 发送
    while (inBuf_.size() >= kPcmBytesPerFrm) {
        QByteArray pcm = inBuf_.left(kPcmBytesPerFrm);
        inBuf_.remove(0, kPcmBytesPerFrm);

        // 应用本地麦克风增益并限幅（在编码前）
        qint16* s = reinterpret_cast<qint16*>(pcm.data());
        if (micGain_ != 1.0f) {
            for (int i = 0; i < kFrameSamples; ++i) {
                int v = static_cast<int>(s[i] * micGain_);
                s[i] = clamp16(v);
            }
        }

        // PCM16 -> µ-law
        QByteArray ulaw; ulaw.resize(kUlawBytesPerFrm);
        for (int i = 0; i < kFrameSamples; ++i) {
            ulaw[i] = static_cast<char>(linearToUlaw(s[i]));
        }

        // 组包并发送
        QJsonObject j{
            {"roomId", roomId_},
            {"sender", sender_},
            {"codec",  "mulaw"},
            {"sr",     kSampleRate},
            {"ch",     kChannels},
            {"seq",    static_cast<int>(seq_++)},
            {"ts",     QDateTime::currentMSecsSinceEpoch()}
        };
        if (conn_) conn_->send(MSG_AUDIO_FRAME, j, ulaw);
    }
}

void AudioChat::shrinkQueueIfNeeded(QByteArray& q) {
    const int softMaxBytes = kMaxQueueFrames * kPcmBytesPerFrm;      // 50帧软上限
    const int hardMaxBytes = softMaxBytes * 2;                      // 100帧硬上限，防止runaway latency
    
    if (q.size() > hardMaxBytes) {
        // 硬上限：直接截断到软上限
        q.remove(0, q.size() - softMaxBytes);
        qWarning() << "[AudioChat] Hard cap triggered, queue truncated to" << softMaxBytes << "bytes";
    } else if (q.size() > softMaxBytes) {
        // 软上限：温和缩减，移除最老的25%
        int removeBytes = (q.size() - softMaxBytes) / 4 * 4; // 对齐到4字节边界
        if (removeBytes > 0) {
            q.remove(0, removeBytes);
        }
    }
}

// 接收并播放
void AudioChat::onPacket(Packet p) {
    if (p.type != MSG_AUDIO_FRAME) return;

    const QString roomId = p.json.value("roomId").toString();
    const QString sender = p.json.value("sender").toString();
    if (roomId.isEmpty() || sender.isEmpty()) return;
    if (!roomId_.isEmpty() && roomId != roomId_) return;
    if (!sender_.isEmpty() && sender == sender_) return;

    const QString codec = p.json.value("codec").toString("mulaw").toLower();
    const int sr = p.json.value("sr").toInt(kSampleRate);
    const int ch = p.json.value("ch").toInt(kChannels);
    if (sr != kSampleRate || ch != kChannels) {
        return;
    }

    QByteArray& q = rxQueues_[sender];
    if (codec == "mulaw") {
        const int n = p.bin.size();
        if (n <= 0) return;
        const uchar* u = reinterpret_cast<const uchar*>(p.bin.constData());
        QByteArray pcm; pcm.resize(n * 2);
        qint16* d = reinterpret_cast<qint16*>(pcm.data());
        for (int i = 0; i < n; ++i) d[i] = ulawToLinear(u[i]);
        q.append(pcm);
    } else if (codec == "pcm16") {
        q.append(p.bin);
    } else {
        return;
    }
    shrinkQueueIfNeeded(q);
}

void AudioChat::mixTick() {
    if (!audioOut_ || !outDev_) return;

    // 将多个对端的 PCM 合并并应用各自增益
    const int need = kPcmBytesPerFrm;
    QByteArray mix; mix.resize(need);
    qint16* md = reinterpret_cast<qint16*>(mix.data());
    for (int i = 0; i < kFrameSamples; ++i) md[i] = 0;

    // 简单VAD检测：记录当前最活跃的说话者（仅内部使用）
    QString activeSpeaker;
    float maxEnergy = 0.0f;
    const float energyThreshold = 1000.0f; // 保守阈值，避免误触发

    for (auto it = rxQueues_.begin(); it != rxQueues_.end(); ++it) {
        QByteArray& q = it.value();
        if (q.size() < need) continue;
        const qint16* s = reinterpret_cast<const qint16*>(q.constData());
        const float g = peerGain_.value(it.key(), 1.0f) * playbackGain_;
        
        // 计算当前帧的能量（RMS）
        float energy = 0.0f;
        for (int i = 0; i < kFrameSamples; ++i) {
            float sample = static_cast<float>(s[i]);
            energy += sample * sample;
        }
        energy = std::sqrt(energy / kFrameSamples);
        
        // VAD检测：记录最活跃说话者
        if (energy > energyThreshold && energy > maxEnergy) {
            maxEnergy = energy;
            activeSpeaker = it.key();
        }
        
        // 混音
        for (int i = 0; i < kFrameSamples; ++i) {
            int v = md[i] + static_cast<int>(s[i] * g);
            if (v > 32767) v = 32767; else if (v < -32768) v = -32768;
            md[i] = static_cast<qint16>(v);
        }
        q.remove(0, need);
    }

    // VAD结果可以在未来用于UI指示，目前保持内部
    if (!activeSpeaker.isEmpty()) {
        // 预留：可发射信号或记录活跃说话者状态
        // emit activeSpeakerChanged(activeSpeaker, maxEnergy);
    }

    outDev_->write(mix);
}
