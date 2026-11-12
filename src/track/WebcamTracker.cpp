#include "WebcamTracker.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QVector>
#include <QImage>
#include <algorithm>
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

bool WebcamTracker::starting() const
{
    return _proc && _proc->state() == QProcess::Starting;
}

bool WebcamTracker::active() const
{
    return _proc && _proc->state() != QProcess::NotRunning;
}

void WebcamTracker::start(const QString& pythonExe, const QString& scriptPath,
                          const QString& modelPath, int cameraIndex,
                          int width, int height)
{
    // Já está ligado com a mesma configuração: não derruba a câmera de novo.
    if (active() && _pythonExe == pythonExe && _scriptPath == scriptPath
        && _modelPath == modelPath && _cameraIndex == cameraIndex
        && _width == width && _height == height) {
        return;
    }

    stop();
    _stopping = false;
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
    connect(_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
        if (err == QProcess::FailedToStart) {
            emit failed(QStringLiteral(
                "Não foi possível executar o rastreador de webcam.\n"
                "Verifique se o Python e o MediaPipe estão instalados "
                "(rode ./setup.sh)."));
        }
    });

    _pythonExe = pythonExe;
    _scriptPath = scriptPath;
    _modelPath = modelPath;
    _cameraIndex = cameraIndex;
    _width = width;
    _height = height;

    QStringList args;
    args << scriptPath << QStringLiteral("--model") << modelPath
         << QStringLiteral("--camera") << QString::number(cameraIndex)
         << QStringLiteral("--width") << QString::number(width)
         << QStringLiteral("--height") << QString::number(height);
    _proc->start(pythonExe, args);
    if (_debugCam)
        _proc->write(QByteArray("{\"cmd\":\"debug_cam\",\"on\":1}\n"));
}

void WebcamTracker::setDebugCam(bool on)
{
    _debugCam = on;
    if (running())
        _proc->write(QByteArray(on ? "{\"cmd\":\"debug_cam\",\"on\":1}\n"
                                    : "{\"cmd\":\"debug_cam\",\"on\":0}\n"));
}

void WebcamTracker::stop()
{
    _stopping = true;
    if (_proc) {
        if (_proc->state() != QProcess::NotRunning) {
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
    _pythonExe.clear();
    _scriptPath.clear();
    _modelPath.clear();
    _cameraIndex = -1;
    _width = _height = 0;
    if (_lastFound) {
        _lastFound = false;
        emit faceFoundChanged(false);
    }
    _stopping = false;
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
        if (line.trimmed().isEmpty())
            continue;
        // Frame do debug (JPEG base64): vai direto pro QLabel da UI
        if (line.contains("\"img\"")) {
            const QJsonObject o = QJsonDocument::fromJson(line).object();
            if (o.contains(QStringLiteral("img"))) {
                const QByteArray jpeg = QByteArray::fromBase64(
                    o[QStringLiteral("img")].toString().toLatin1());
                QImage img;
                if (img.loadFromData(jpeg, "JPG"))
                    emit debugFrame(img);
                continue;
            }
        }
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

void WebcamTracker::onFinished(int exitCode, QProcess::ExitStatus status)
{
    const bool wasStopping = _stopping;
    if (!wasStopping) {
        if (exitCode == 3) {
            emit failed(QStringLiteral(
                "Nenhuma câmera entregou imagem.\n"
                "Se usa OBS Virtual Cam, inicie a câmera virtual no OBS; "
                "se usa Iriun/telefone, conecte o celular.\n"
                "Feche também VTube Studio/OpenSeeFace/OBS que estejam usando "
                "a mesma câmera e escolha a correta na aba Rastreio."));
        } else if (exitCode == 2) {
            emit failed(QStringLiteral("O rastreador de webcam não iniciou "
                                       "(verifique se mediapipe está instalado)."));
        } else if (status == QProcess::CrashExit || exitCode != 0) {
            emit failed(QStringLiteral(
                "O rastreador de webcam encerrou inesperadamente (código %1).\n"
                "Veja o terminal para detalhes do erro.").arg(exitCode));
        }
    }
    _face = FaceReading();
    _calibrating = false;
    if (_lastFound) {
        _lastFound = false;
        emit faceFoundChanged(false);
    }
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
        f.eyeSmileL = static_cast<float>(o[QStringLiteral("eyeSmileL")].toDouble());
        f.eyeSmileR = static_cast<float>(o[QStringLiteral("eyeSmileR")].toDouble());
        f.tongueOut = static_cast<float>(o[QStringLiteral("tongueOut")].toDouble());
        f.jaw   = static_cast<float>(o[QStringLiteral("jaw")].toDouble());
        f.smile = static_cast<float>(o[QStringLiteral("smile")].toDouble());
        f.brow  = static_cast<float>(o[QStringLiteral("brow")].toDouble());
        f.eyeX  = static_cast<float>(o[QStringLiteral("eyeX")].toDouble());
        f.eyeY  = static_cast<float>(o[QStringLiteral("eyeY")].toDouble());
        f.faceX = static_cast<float>(o[QStringLiteral("faceX")].toDouble());
        f.faceY = static_cast<float>(o[QStringLiteral("faceY")].toDouble());
        f.faceZ = static_cast<float>(o[QStringLiteral("faceZ")].toDouble());
        f.faceScale = static_cast<float>(o[QStringLiteral("faceScale")].toDouble(1.0));
        f.cheekSquint = static_cast<float>(o[QStringLiteral("cheekSquint")].toDouble());
        f.noseSneer   = static_cast<float>(o[QStringLiteral("noseSneer")].toDouble());
        f.mouthDimple = static_cast<float>(o[QStringLiteral("mouthDimple")].toDouble());
        f.mouthPress  = static_cast<float>(o[QStringLiteral("mouthPress")].toDouble());
        if (o.contains(QStringLiteral("earL"))) {
            f.earL = static_cast<float>(o[QStringLiteral("earL")].toDouble());
            f.earR = static_cast<float>(o[QStringLiteral("earR")].toDouble());
            f.mar  = static_cast<float>(o[QStringLiteral("mar")].toDouble());
            f.mouthX = static_cast<float>(o[QStringLiteral("mouthX")].toDouble());
            f.mouthY = static_cast<float>(o[QStringLiteral("mouthY")].toDouble());
            f.browL  = static_cast<float>(o[QStringLiteral("browL")].toDouble());
            f.browR  = static_cast<float>(o[QStringLiteral("browR")].toDouble());
        }
        f.pucker   = static_cast<float>(o[QStringLiteral("pucker")].toDouble());
        f.funnel   = static_cast<float>(o[QStringLiteral("funnel")].toDouble());
        f.cheek    = static_cast<float>(o[QStringLiteral("cheek")].toDouble());
        f.browDown = static_cast<float>(o[QStringLiteral("browDown")].toDouble());
        f.jawSide  = static_cast<float>(o[QStringLiteral("jawSide")].toDouble());
        f.frown    = static_cast<float>(o[QStringLiteral("frown")].toDouble());
        f.stretch  = static_cast<float>(o[QStringLiteral("stretch")].toDouble());
    }
    _face = f;

    if (f.found != _lastFound) {
        _lastFound = f.found;
        emit faceFoundChanged(f.found);
    }

    if (_calibrating && f.found) {
        _calYaw << f.yaw;   _calPitch << f.pitch; _calRoll << f.roll;
        _calEyeX << f.eyeX; _calEyeY << f.eyeY;
        _calMouth << f.jaw; _calBrow << f.brow;
        _calFaceX << f.faceX; _calFaceY << f.faceY; _calFaceZ << f.faceZ;
        _calFaceScale << f.faceScale;
        _calEarL << f.earL; _calEarR << f.earR; _calMar << f.mar;
        _calMouthX << f.mouthX; _calMouthY << f.mouthY;
    }

    if (_calibrating && _calTimer.hasExpired(1500)) {
        _calibrating = false;
        FaceNeutral n;
        const int samples = _calYaw.size();
        if (samples > 5) {
            auto median = [](QVector<float> v) {
                std::sort(v.begin(), v.end());
                const int m = v.size() / 2;
                return (v.size() % 2) ? v[m]
                                      : 0.5f * (v[m - 1] + v[m]);
            };
            n.yaw   = median(std::move(_calYaw));
            n.pitch = median(std::move(_calPitch));
            n.roll  = median(std::move(_calRoll));
            n.eyeX  = median(std::move(_calEyeX));
            n.eyeY  = median(std::move(_calEyeY));
            n.mouth = median(std::move(_calMouth));
            n.brow  = median(std::move(_calBrow));
            n.faceX = median(std::move(_calFaceX));
            n.faceY = median(std::move(_calFaceY));
            n.faceZ = median(std::move(_calFaceZ));
            n.faceScale = median(std::move(_calFaceScale));
            n.earL  = median(std::move(_calEarL));
            n.earR  = median(std::move(_calEarR));
            n.mar   = median(std::move(_calMar));
            n.mouthX = median(std::move(_calMouthX));
            n.mouthY = median(std::move(_calMouthY));
            n.valid = true;
        } else {
            _calYaw.clear(); _calPitch.clear(); _calRoll.clear();
            _calEyeX.clear(); _calEyeY.clear(); _calMouth.clear(); _calBrow.clear();
            _calFaceX.clear(); _calFaceY.clear(); _calFaceZ.clear(); _calFaceScale.clear();
            _calEarL.clear(); _calEarR.clear(); _calMar.clear();
            _calMouthX.clear(); _calMouthY.clear();
        }
        _neutral = n;
        emit neutralCaptured(n);
    }
}

void WebcamTracker::calibrate(int ms)
{
    if (!running())
        return;
    _calTimer.start();
    _calibrating = true;
    _calYaw.clear(); _calPitch.clear(); _calRoll.clear();
    _calEyeX.clear(); _calEyeY.clear(); _calMouth.clear(); _calBrow.clear();
    _calFaceX.clear(); _calFaceY.clear(); _calFaceZ.clear(); _calFaceScale.clear();
    _calEarL.clear(); _calEarR.clear(); _calMar.clear();
    _calMouthX.clear(); _calMouthY.clear();
    Q_UNUSED(ms); // janela fixa de 1500 ms (constante interna)
}
