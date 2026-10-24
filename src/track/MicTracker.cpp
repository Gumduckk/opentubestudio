#include "MicTracker.h"

#include <QAudioFormat>
#include <QMediaDevices>
#include <cmath>
#include <vector>
#include <cstring>
#include <cstdio>

MicTracker::MicTracker(QObject* parent)
    : QObject(parent)
{
}

MicTracker::~MicTracker()
{
    stop();
}

void MicTracker::setParams(float threshold, float gain)
{
    _threshold = threshold;
    _gain = gain;
}

void MicTracker::start(const QString& deviceId)
{
    stop();

    QAudioDevice dev = QMediaDevices::defaultAudioInput();
    if (!deviceId.isEmpty()) {
        const auto inputs = QMediaDevices::audioInputs();
        for (const QAudioDevice& d : inputs) {
            if (d.id() == deviceId.toUtf8()) {
                dev = d;
                break;
            }
        }
    }
    if (dev.isNull()) {
        emit failed(QStringLiteral("Nenhum microfone encontrado"));
        return;
    }

    if (qEnvironmentVariableIsSet("GUMM_DEBUG")) {
        fprintf(stderr, "[mic] escolhido='%s' id=%s (pedido='%s')\n",
                qPrintable(dev.description()), dev.id().constData(),
                qPrintable(deviceId));
        for (const QAudioDevice& d : QMediaDevices::audioInputs())
            fprintf(stderr, "[mic]   input: '%s' id=%s%s\n",
                    qPrintable(d.description()), d.id().constData(),
                    (d == dev ? "  <= usado" : ""));
    }

    QAudioFormat want;
    want.setSampleRate(48000);
    want.setChannelCount(1);
    want.setSampleFormat(QAudioFormat::Float);

    QAudioFormat format = dev.isFormatSupported(want)
                              ? want
                              : dev.preferredFormat();
    if (format.sampleFormat() != QAudioFormat::Float) {
        // Converte float -> o formato preferido é feito abaixo lendo como
        // int16 também; aceitamos Float ou Int16.
        if (format.sampleFormat() != QAudioFormat::Int16) {
            emit failed(QStringLiteral("Formato de áudio não suportado no microfone"));
            return;
        }
    }

    _source = new QAudioSource(dev, format, this);
    _device = _source->start();
    if (!_device) {
        delete _source;
        _source = nullptr;
        emit failed(QStringLiteral("Não foi possível abrir o microfone (%1)").arg(dev.description()));
        return;
    }
    connect(_device, &QIODevice::readyRead, this, &MicTracker::onReadyRead);

    if (qEnvironmentVariableIsSet("GUMM_DEBUG"))
        fprintf(stderr, "[mic] aberto '%s' sr=%d ch=%d fmt=%d\n",
                qPrintable(dev.description()), format.sampleRate(),
                format.channelCount(), int(format.sampleFormat()));
}

void MicTracker::stop()
{
    if (_source) {
        _source->stop();
        delete _source;
        _source = nullptr;
        _device = nullptr;
    }
    _env = 0.0f;
    _mouth = 0.0f;
}

void MicTracker::onReadyRead()
{
    if (!_device || !_source)
        return;
    const QByteArray data = _device->readAll();
    if (data.isEmpty())
        return;

    const QAudioFormat fmt = _source->format();
    double sumSq = 0.0;
    int samples = 0;

    if (fmt.sampleFormat() == QAudioFormat::Float) {
        const int n = data.size() / static_cast<int>(sizeof(float));
        const float* p = reinterpret_cast<const float*>(data.constData());
        for (int i = 0; i < n; ++i) {
            sumSq += static_cast<double>(p[i]) * p[i];
            ++samples;
        }
    } else if (fmt.sampleFormat() == QAudioFormat::Int16) {
        const int n = data.size() / static_cast<int>(sizeof(qint16));
        const qint16* p = reinterpret_cast<const qint16*>(data.constData());
        for (int i = 0; i < n; ++i) {
            const double v = p[i] / 32768.0;
            sumSq += v * v;
            ++samples;
        }
    }
    if (samples == 0)
        return;

    const float rms = static_cast<float>(std::sqrt(sumSq / samples));

    // Detector de envelope: ataque rápido, queda lenta (boca acompanha a voz)
    if (rms > _env)
        _env = rms;
    else
        _env *= 0.86f;

    const float m = (_env - _threshold) * _gain;
    _mouth = m < 0.0f ? 0.0f : (m > 1.0f ? 1.0f : m);

    if (qEnvironmentVariableIsSet("GUMM_DEBUG")) {
        static int n = 0;
        if (n++ % 60 == 0)
            fprintf(stderr, "[mic] rms=%.4f env=%.4f mouth=%.3f thr=%.3f gain=%.1f\n",
                    double(rms), double(_env), double(_mouth),
                    double(_threshold), double(_gain));
    }
}
