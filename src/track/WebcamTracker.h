#pragma once
/*
 * WebcamTracker — rastreio facial real (cabeça, olhos, boca, sobrancelhas)
 * usando MediaPipe (Google, grátis, roda 100% offline).
 *
 * MediaPipe é Python, então o app abre um "daemon" (tools/tracker_daemon.py)
 * via QProcess e conversa por JSON-lines no stdout. Se o Python/MediaPipe não
 * estiver disponível, o app continua funcionando normalmente com
 * mouse + microfone e só avisa na UI.
 */
#include <QObject>
#include <QProcess>
#include <QElapsedTimer>

struct FaceReading
{
    bool  found = false;
    float yaw = 0, pitch = 0, roll = 0;   // graus
    float blinkL = 0, blinkR = 0;          // 0..1 (1 = fechado)
    float jaw = 0;                          // 0..1
    float smile = 0;                        // 0..1
    float brow = 0;                         // 0..1
    float eyeX = 0, eyeY = 0;               // deslocamento da íris (-1..1)
};

struct FaceNeutral
{
    float yaw = 0, pitch = 0, roll = 0;
    float eyeX = 0, eyeY = 0, mouth = 0, brow = 0;
    bool  valid = false;
};

class WebcamTracker : public QObject
{
    Q_OBJECT
public:
    explicit WebcamTracker(QObject* parent = nullptr);
    ~WebcamTracker() override;

    // scriptPath: openseeface_daemon.py / tracker_daemon.py;
    // modelPath: face_landmarker.task (só usado pelo MediaPipe);
    // cameraIndex: índice v4l2 (/dev/videoN) para o OpenSeeFace.
    void start(const QString& pythonExe, const QString& scriptPath,
               const QString& modelPath, int cameraIndex,
               int width = 1280, int height = 720);
    void stop();
    bool running() const;

    const FaceReading& face() const { return _face; }
    FaceNeutral neutral() const { return _neutral; }

    // Calibração: média dos valores durante ~1.5s com a pessoa olhando
    // reto pra câmera. Emite neutralCaptured() ao terminar.
    void calibrate(int ms = 1500);
    bool calibrating() const { return _calibrating; }

signals:
    void failed(const QString& reason);   // não conseguiu rodar o daemon
    void logLine(const QString& line);    // saída de stderr do daemon
    void neutralCaptured(const FaceNeutral& n);
    void faceFoundChanged(bool found);

private slots:
    void onReadyReadStdout();
    void onReadyReadStderr();
    void onFinished(int exitCode, QProcess::ExitStatus status);

private:
    void parseLine(const QByteArray& line);

    QProcess* _proc = nullptr;
    QByteArray _buf;
    FaceReading _face;
    bool _lastFound = false;

    bool _calibrating = false;
    QElapsedTimer _calTimer;
    double _sumYaw = 0, _sumPitch = 0, _sumRoll = 0;
    double _sumEyeX = 0, _sumEyeY = 0, _sumMouth = 0, _sumBrow = 0;
    int    _calSamples = 0;
    FaceNeutral _neutral;
};
