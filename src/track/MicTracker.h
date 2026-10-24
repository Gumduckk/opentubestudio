#pragma once
/*
 * MicTracker — lê o microfone (Qt6 Multimedia) e produz um valor 0..1 de
 * "boca aberta" a partir do volume falado (lip sync simples, offline).
 */
#include <QObject>
#include <QMediaDevices>
#include <QAudioSource>
#include <QIODevice>

class MicTracker : public QObject
{
    Q_OBJECT
public:
    explicit MicTracker(QObject* parent = nullptr);
    ~MicTracker() override;

    void start(const QString& deviceId);
    void stop();
    bool running() const { return _source != nullptr; }

    void setParams(float threshold, float gain);
    float mouthOpen() const { return _mouth; }

signals:
    void failed(const QString& reason);

private slots:
    void onReadyRead();

private:
    QAudioSource* _source = nullptr;
    QIODevice* _device = nullptr;
    float _threshold = 0.025f;
    float _gain = 6.0f;
    float _env = 0.0f;
    float _mouth = 0.0f;
};
