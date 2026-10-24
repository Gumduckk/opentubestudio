#include "WebcamTracker.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <cmath>

WebcamTracker::WebcamTracker(QObject* parent)
    : QObject(parent)
{
}

WebcamTracker::~WebcamTracker()
{
    stop();
}

bool WebcamTracker::running() const
{
    return _proc && _proc->state() == QProcess::Running;
}

void WebcamTracker::start(const QString& pythonExe, const QString& scriptPath,
                          const QString& modelPath, int cameraIndex,
                          int width, int height)
{
    stop();
    if (!QFileInfo::exists(scriptPath)) {
        emit failed(QStringLiteral("Script do rastreador não encontrado: %1").arg(scriptPath));
        return;
    }

    _proc = new QProcess(this);
    _proc->setProcessChannelMode(QProcess::SeparateChannels);
    connect(_proc, &QProcess::readyReadStandardOutput,
            this, &WebcamTracker::onReadyReadStdout);
    connect(_proc, &QProcess::readyReadStandardError,
            this, &WebcamTracker::onReadyReadStderr);
    connect(_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &WebcamTracker::onFinished);

    QStringList args;
    args << scriptPath << QStringLiteral("--model") << modelPath
         << QStringLiteral("--camera") << QString::number(cameraIndex)
         << QStringLiteral("--width") << QString::number(width)
         << QStringLiteral("--height") << QString::number(height);
    _proc->start(pythonExe, args);
}

void WebcamTracker::stop()
{
    if (_proc) {
        if (_proc->state() == QProcess::Running) {
            _proc->write("{\"cmd\":\"quit\"}\n");
            _proc->closeWriteChannel();
            if (!_proc->waitForFinished(1200)) {
                _proc->kill();
                _proc->waitForFinished(800);
            }
        }
        _proc->deleteLater();
        _proc = nullptr;
    }
    _buf.clear();
    _face = FaceReading();
    _calibrating = false;
}

void WebcamTracker::onReadyReadStdout()
{
    if (!_proc)
        return;
    _buf.append(_proc->readAllStandardOutput());
    int idx;
    while ((idx = _buf.indexOf('\n')) >= 0) {
        const QByteArray line = _buf.left(idx);
        _buf.remove(0, idx + 1);
        if (!line.trimmed().isEmpty())
            parseLine(line);
    }
}

void WebcamTracker::onReadyReadStderr()
{
    if (!_proc)
        return;
    const QString text = QString::fromUtf8(_proc->readAllStandardError());
    for (const QString& l : text.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
        emit logLine(l);
}

void WebcamTracker::onFinished(int exitCode, QProcess::ExitStatus)
{
    const bool failedStart = (exitCode == 2 || exitCode == 3);
    if (failedStart) {
        emit failed(QStringLiteral("O rastreador de webcam não iniciou "
                                   "(verifique se mediapipe está instalado)."));
    }
    _face = FaceReading();
    _calibrating = false;
}

void WebcamTracker::parseLine(const QByteArray& line)
{
    const QJsonObject o = QJsonDocument::fromJson(line).object();
    if (o.contains(QStringLiteral("ok")))   // handshake do daemon
        return;

    FaceReading f;
    f.found = o[QStringLiteral("found")].toInt(0) != 0;
    if (f.found) {
        f.yaw   = static_cast<float>(o[QStringLiteral("yaw")].toDouble());
        f.pitch = static_cast<float>(o[QStringLiteral("pitch")].toDouble());
        f.roll  = static_cast<float>(o[QStringLiteral("roll")].toDouble());
        f.blinkL= static_cast<float>(o[QStringLiteral("blinkL")].toDouble());
        f.blinkR= static_cast<float>(o[QStringLiteral("blinkR")].toDouble());
        f.jaw   = static_cast<float>(o[QStringLiteral("jaw")].toDouble());
        f.smile = static_cast<float>(o[QStringLiteral("smile")].toDouble());
        f.brow  = static_cast<float>(o[QStringLiteral("brow")].toDouble());
        f.eyeX  = static_cast<float>(o[QStringLiteral("eyeX")].toDouble());
        f.eyeY  = static_cast<float>(o[QStringLiteral("eyeY")].toDouble());
    }
    _face = f;

    if (f.found != _lastFound) {
        _lastFound = f.found;
        emit faceFoundChanged(f.found);
    }

    if (_calibrating && f.found) {
        _sumYaw += f.yaw;   _sumPitch += f.pitch; _sumRoll += f.roll;
        _sumEyeX += f.eyeX; _sumEyeY += f.eyeY;
        _sumMouth += f.jaw; _sumBrow += f.brow;
        ++_calSamples;
    }

    if (_calibrating && _calTimer.hasExpired(1500)) {
        _calibrating = false;
        FaceNeutral n;
        if (_calSamples > 5) {
            const double inv = 1.0 / _calSamples;
            n.yaw   = static_cast<float>(_sumYaw * inv);
            n.pitch = static_cast<float>(_sumPitch * inv);
            n.roll  = static_cast<float>(_sumRoll * inv);
            n.eyeX  = static_cast<float>(_sumEyeX * inv);
            n.eyeY  = static_cast<float>(_sumEyeY * inv);
            n.mouth = static_cast<float>(_sumMouth * inv);
            n.brow  = static_cast<float>(_sumBrow * inv);
            n.valid = true;
        }
        _neutral = n;
        _sumYaw = _sumPitch = _sumRoll = _sumEyeX = _sumEyeY = _sumMouth = _sumBrow = 0;
        _calSamples = 0;
        emit neutralCaptured(n);
    }
}

void WebcamTracker::calibrate(int ms)
{
    if (!running())
        return;
    _calTimer.start();
    _calibrating = true;
    _sumYaw = _sumPitch = _sumRoll = _sumEyeX = _sumEyeY = _sumMouth = _sumBrow = 0;
    _calSamples = 0;
    Q_UNUSED(ms); // janela fixa de 1500 ms (constante interna)
}
