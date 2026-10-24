#pragma once
/*
 * ModelView — widget OpenGL que renderiza o modelo Live2D sobre o fundo
 * escolhido (cor sólida tipo tela verde, ou transparente para o OBS)
 * e compõe os canais de rastreio (mouse + microfone + webcam).
 */
#include <QOpenGLWidget>
#include <QElapsedTimer>
#include <QTimer>
#include <memory>

#include "config/AppConfig.h"
#include "live2d/VTuberModel.h"
#include "track/MouseTracker.h"
#include "track/EvdevMouse.h"
#include "track/MicTracker.h"
#include "track/WebcamTracker.h"

class ModelView : public QOpenGLWidget
{
    Q_OBJECT
public:
    explicit ModelView(QWidget* parent = nullptr);
    ~ModelView() override;

    // Carrega o modelo (caminho do .model3.json). Retorna "" em sucesso.
    QString loadModel(const QString& model3Path);
    VTuberModel* model() { return _model.get(); }
    bool hasModel() const { return _model != nullptr; }
    QString lastError() const { return _lastError; }

    void updateSettings(const AppSettings& s);
    AppSettings settings() const { return _settings; }

    void resetView();
    void toggleExpressionByName(const QString& name);

    WebcamTracker* webcam() { return &_cam; }
    MicTracker* mic() { return &_mic; }

signals:
    void viewAdjusted(float zoom, float panX, float panY); // zoom/pan do usuário
    void fpsUpdated(float fps);
    void webcamStatus(const QString& text);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void leaveEvent(QEvent* e) override;

private:
    Channels composeChannels(float dt);
    void updateBlink(float dt);
    // Lê a posição global do cursor (mesmo fora da janela) e alimenta o
    // MouseTracker, normalizada em relação ao centro da janela.
    void pollGlobalMouse();
    // Garante que o renderer GL do modelo foi criado (precisa do contexto
    // corrente; chamado dentro do paintGL).
    bool ensureModelGl();

    std::unique_ptr<VTuberModel> _model;
    // true quando o modelo foi carregado mas o renderer GL (que exige o
    // contexto corrente) ainda não foi inicializado.
    bool _needsGlSetup = false;
    QString _lastError;
    AppSettings _settings;

    MouseTracker  _mouse;
    EvdevMouse    _evdev;
    MicTracker    _mic;
    WebcamTracker _cam;

    QTimer        _timer;
    QElapsedTimer _clock;
    float _lastT = -1.0f;
    float _fps = 0.0f;
    float _time = 0.0f;

    // piscada automática (quando a webcam não controla os olhos)
    float _blinkCountdown = 2.0f;
    float _blinkValue = 0.0f;

    // fade quando o rosto some da câmera
    float _camFade = 0.0f;

    // arrastar para mover o modelo
    bool  _dragging = false;
    QPoint _lastDragPos;

    bool _glInitialized = false;
};
