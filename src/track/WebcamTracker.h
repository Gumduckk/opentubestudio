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
#include <QVector>

struct FaceReading
{
    bool  found = false;
    float yaw = 0, pitch = 0, roll = 0;   // graus
    float blinkL = 0, blinkR = 0;          // 0..1 (1 = fechado)
    float eyeSmileL = 0, eyeSmileR = 0;    // 0..1 (olho feliz/apertado)
    float tongueOut = 0;                    // 0..1
    float jaw = 0;                          // 0..1
    float smile = 0;                        // 0..1
    float brow = 0;                         // 0..1
    float eyeX = 0, eyeY = 0;               // deslocamento da íris (-1..1)
    // posição 3D (métrica, cm) + escala uniforme da matriz
    float faceX = 0, faceY = 0, faceZ = 0, faceScale = 1.0f;
    // expressões extras (0..1; jawSide é -1..1)
    float pucker = 0, funnel = 0, cheek = 0, browDown = 0;
    float jawSide = 0, frown = 0, stretch = 0;
    // canais ARKit extras
    float cheekSquint = 0, noseSneer = 0, mouthDimple = 0, mouthPress = 0;
};

struct FaceNeutral
{
    float yaw = 0, pitch = 0, roll = 0;
    float eyeX = 0, eyeY = 0, mouth = 0, brow = 0;
    float faceX = 0, faceY = 0, faceZ = 0, faceScale = 1.0f;
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
    bool running() const;                 // daemon já está rodando
    bool starting() const;                // processo abrindo a câmera
    bool active() const;                  // running() || starting()
    int cameraIndex() const { return _cameraIndex; }

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
    bool _stopping = false;

    // Configuração do processo atual. Serve para não reiniciar a câmera
    // quando start() é chamado de novo com os mesmos parâmetros.
    QString _pythonExe, _scriptPath, _modelPath;
    int _cameraIndex = -1;
    int _width = 0, _height = 0;

    bool _calibrating = false;
    QElapsedTimer _calTimer;
    // Amostras brutas da janela de calibração; o neutro é a MEDIANA de cada
    // canal (robusta a frames com o rosto parcialmente detectado).
    QVector<float> _calYaw, _calPitch, _calRoll;
    QVector<float> _calEyeX, _calEyeY, _calMouth, _calBrow;
    QVector<float> _calFaceX, _calFaceY, _calFaceZ, _calFaceScale;
    FaceNeutral _neutral;
};
