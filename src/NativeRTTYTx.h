#pragma once

#include <QObject>
#include <QString>

class TCIClient;

// Native FSK control only. Does not generate AFSK samples or switch MOX.
class NativeRTTYTx final : public QObject {
    Q_OBJECT
public:
    enum class State { Disconnected, Idle, Sending, Draining };
    explicit NativeRTTYTx(TCIClient *client, QObject *parent = nullptr);
    State state() const { return state_; }
    bool bufferEmpty() const { return bufferEmpty_; }
    void setNativeRttyConfirmed(bool confirmed) { nativeRttyConfirmed_ = confirmed; }
    bool send(const QString &text);
    bool start();
    bool drain();
    bool stop();

signals:
    void stateChanged(State state);
    void bufferEmptyReceived();
    void drainBufferEmpty();

private:
    void setState(State state);
    static QString escapePayload(const QString &text);
    TCIClient *client_;
    State state_ = State::Disconnected;
    bool bufferEmpty_ = true;
    bool nativeRttyConfirmed_ = false;
};
