#include "ModelView.h"
#include "../track/FaceTrackerInput.h"

#include <QOpenGLContext>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QDir>
#include <QCoreApplication>
#include <QRandomGenerator>
#include <QCursor>
#include <QGuiApplication>
#include <QScreen>
#include <cmath>
#include <cstdio>

namespace {
float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}
} // namespace

ModelView::ModelView(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    _mic.setParams(_settings.micThreshold, _settings.micGain);

    // Leitura global do cursor via evdev (funciona fora da janela no Wayland).
    if (_evdev.open() > 0) {
        if (qEnvironmentVariableIsSet("GUMM_DEBUG"))
            fprintf(stderr, "[evdev] mouses: %s\n",
                    qPrintable(_evdev.deviceNames().join(QStringLiteral(", "))));
    } else if (qEnvironmentVariableIsSet("GUMM_DEBUG")) {
        fprintf(stderr, "[evdev] indisponivel: %s\n", qPrintable(_evdev.lastError()));
    }

    connect(&_timer, &QTimer::timeout, this, [this]() { update(); });
    _timer.start(16); // ~60 fps

    connect(&_cam, &WebcamTracker::logLine, this, [](const QString& l) {
        qInfo("[webcam] %s", qPrintable(l));
    });
}

ModelView::~ModelView()
{
    _timer.stop();
    _cam.stop();
    _mic.stop();
    makeCurrent();
    _model.reset();
    doneCurrent();
}

// ---------------------------------------------------------------------------
// Configuração
// ---------------------------------------------------------------------------
void ModelView::updateSettings(const AppSettings& s)
{
    const bool webcamWasOn = _settings.webcamEnabled;
    const QString oldCamDev = _settings.webcamDevice;
    _settings = s;
    _mic.setParams(s.micThreshold, s.micGain);

    // Modo transparente: o QOpenGLWidget precisa de fundo translúcido para o
    // alpha do clear aparecer.
    setAttribute(Qt::WA_TranslucentBackground, s.transparentBackground);
    update();

    // Microfone: liga/desliga conforme configuração
    if (s.micEnabled && !_mic.running())
        _mic.start(s.micDevice);
    if (!s.micEnabled && _mic.running())
        _mic.stop();

    // Webcam: (re)inicia só se ligada e dispositivo mudou — o start é feito
    // principalmente pela MainWindow (que conhece o índice do dispositivo).
    if (!s.webcamEnabled && webcamWasOn && _cam.running())
        _cam.stop();
    Q_UNUSED(oldCamDev);
}

void ModelView::resetView()
{
    _settings.zoom = 1.0f;
    _settings.panX = 0.0f;
    _settings.panY = 0.0f;
    emit viewAdjusted(_settings.zoom, _settings.panX, _settings.panY);
}

void ModelView::toggleExpressionByName(const QString& name)
{
    if (_model)
        _model->toggleExpression(name);
}

// ---------------------------------------------------------------------------
// Modelo
// ---------------------------------------------------------------------------
QString ModelView::loadModel(const QString& model3Path)
{
    _lastError.clear();

    // Cria e carrega o modelo (parte que NÃO precisa de contexto GL).
    auto m = std::make_unique<VTuberModel>();
    QString err;
    if (!m->Load(model3Path, &err)) {
        _lastError = err;
        if (qEnvironmentVariableIsSet("GUMM_DEBUG"))
            fprintf(stderr, "[model] falha ao carregar '%s': %s\n",
                    qPrintable(model3Path), qPrintable(err));
        return err;
    }

    // O modelo fica disponível imediatamente (a UI já pode consultar
    // parâmetros/movimentos). O SetupGl precisa do contexto OpenGL atual,
    // que só é garantido dentro de initializeGL/paintGL: se ainda não temos
    // contexto, marcamos para inicializar no próximo frame.
    _model = std::move(m);

    if (context() && context()->isValid()) {
        makeCurrent();
        const bool ok = _model->SetupGl(qMax(1, int(width() * devicePixelRatioF())),
                                        qMax(1, int(height() * devicePixelRatioF())), &err);
        doneCurrent();
        if (!ok) {
            _lastError = err;
            _model.reset();
            return err;
        }
        _needsGlSetup = false;
    } else {
        _needsGlSetup = true;
    }

    _camFade = 0.0f;
    update();
    return QString();
}

bool ModelView::ensureModelGl()
{
    if (!_model || !_needsGlSetup)
        return _model != nullptr;

    QString err;
    if (!_model->SetupGl(qMax(1, int(width() * devicePixelRatioF())),
                         qMax(1, int(height() * devicePixelRatioF())), &err)) {
        qWarning("[GummVTuber] %s", qPrintable(err));
        _lastError = err;
        _model.reset();
        return false;
    }
    _needsGlSetup = false;
    _camFade = 0.0f;
    return true;
}

// ---------------------------------------------------------------------------
// GL
// ---------------------------------------------------------------------------
void ModelView::initializeGL()
{
    _glInitialized = true;
    _clock.start();

    // O modelo pendente é inicializado no paintGL (contexto já corrente).

    if (qEnvironmentVariableIsSet("GUMM_DEBUG")) {
        const QSurfaceFormat f = QOpenGLContext::currentContext()->format();
        qInfo("[gl] version=%s renderer=%s  fmt: samples=%d stencil=%d alpha=%d swap=%d",
              qPrintable(QString::fromLatin1(reinterpret_cast<const char*>(glGetString(GL_VERSION)))),
              qPrintable(QString::fromLatin1(reinterpret_cast<const char*>(glGetString(GL_RENDERER)))),
              f.samples(), f.stencilBufferSize(), f.alphaBufferSize(),
              int(f.swapBehavior()));
    }
}

void ModelView::resizeGL(int, int)
{
}

void ModelView::paintGL()
{
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    // O renderer do modelo define o blend por drawable (glBlendFuncSeparate).
    // Aqui só precisamos de um blend válido para o fundo/limpeza. O modelo
    // NÃO é premultiplied por padrão, então usamos o blend alfa clássico.
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (_settings.transparentBackground)
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    else
        glClearColor(static_cast<float>(_settings.backgroundColor.redF()),
                     static_cast<float>(_settings.backgroundColor.greenF()),
                     static_cast<float>(_settings.backgroundColor.blueF()),
                     1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    if (qEnvironmentVariableIsSet("GUMM_DEBUG")) {
        static int q = 0;
        if (q++ % 240 == 7) {
            unsigned char c[4] = {0,0,0,0};
            glReadPixels(width()/4, height()/4, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, c);
            qInfo("[gl] pos-clear pixel=%d,%d,%d,%d", c[0], c[1], c[2], c[3]);
        }
    }

    // Diagnóstico: com GUMM_NO_MODEL o fundo é limpo mas o modelo não é
    // desenhado. Se o verde aparecer assim, é o desenho do modelo que cobre
    // a tela; se continuar preto, o problema é o clear/apresentação.
    if (qEnvironmentVariableIsSet("GUMM_NO_MODEL"))
        return;

    VTuberModel* mdl = ensureModelGl() ? _model.get() : nullptr;
    if (!mdl)
        return;

    if (qEnvironmentVariableIsSet("GUMM_DEBUG")) {
        GLint vp[4] = { 0, 0, 0, 0 }, fbo = 0;
        glGetIntegerv(GL_VIEWPORT, vp);
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
        static int n = 0;
        if (n++ % 120 == 0)
            qInfo("[gl] paint w=%d h=%d dpr=%g viewport=%d,%d %dx%d fbo=%d blend=%s",
                  width(), height(), double(devicePixelRatioF()),
                  vp[0], vp[1], vp[2], vp[3], fbo,
                  glIsEnabled(GL_BLEND) ? "on" : "off");
    }

    const qint64 ms = _clock.elapsed();
    float t = static_cast<float>(ms) / 1000.0f;
    float dt = (_lastT < 0.0f) ? (1.0f / 60.0f) : (t - _lastT);
    _lastT = t;
    dt = clampf(dt, 0.0005f, 0.1f);
    _time += dt;

    // fps (média exponencial)
    const float inst = 1.0f / dt;
    _fps += (inst - _fps) * 0.05f;
    emit fpsUpdated(_fps);

    const Channels ch = composeChannels(dt);
    mdl->Update(ch, _settings.mappings, dt);
    // O QOpenGLWidget desenha num FBO em pixels físicos (device pixels); o
    // glViewport setado pelo Qt usa esse tamanho. Passar width()/height()
    // lógicos com dpr != 1 faz o Cubism renderizar em espaço errado.
    const float fbW = static_cast<float>(width()) * static_cast<float>(devicePixelRatioF());
    const float fbH = static_cast<float>(height()) * static_cast<float>(devicePixelRatioF());
    mdl->Draw(fbW, fbH, _settings.zoom, _settings.panX, _settings.panY);
}

// ---------------------------------------------------------------------------
// Canais de rastreio
// ---------------------------------------------------------------------------
void ModelView::pollGlobalMouse()
{
    QPoint gp;

    if (_evdev.running()) {
        // Caminho Wayland: posição global reconstruída pelo evdev. O Qt não
        // atualiza QCursor::pos() fora da janela, então usamos o acumulador.
        // Enquanto o cursor está sob a janela, o Qt é autoridade e mantém o
        // acumulador calibrado (evita deriva).
        const bool overWindow = underMouse() || rect().contains(mapFromGlobal(QCursor::pos()));
        if (overWindow) {
            const QPoint qt = QCursor::pos();
            _evdev.setPos(qt);
            gp = qt;
        } else {
            _evdev.poll();
            gp = _evdev.pos();
        }
    } else {
        // Sem evdev: cai para o Qt (só funciona com o cursor na janela).
        gp = QCursor::pos();
    }

    // Tela a rastrear: a escolhida nas configs (por nome); se vazia/ausente,
    // cai para a tela onde a janela está; por fim a primária.
    const QScreen* scr = nullptr;
    if (!_settings.mouseScreen.isEmpty()) {
        for (QScreen* s : QGuiApplication::screens()) {
            if (s->name() == _settings.mouseScreen) { scr = s; break; }
        }
    }
    if (!scr) scr = this->screen();
    if (!scr) scr = QGuiApplication::primaryScreen();
    if (!scr)
        return;

    const QRect geo = scr->geometry();

    // Normaliza em relação ao CENTRO da tela rastreada: cursor no centro =
    // neutro; borda da tela = ~±1.
    const float cx = geo.x() + geo.width() * 0.5f;
    const float cy = geo.y() + geo.height() * 0.5f;
    const float radius = qMax(1.0f, qMin(geo.width(), geo.height()) * 0.5f);

    const float nx = (gp.x() - cx) / radius;
    const float ny = (gp.y() - cy) / radius;
    _mouse.updatePointer(nx, ny);

    if (qEnvironmentVariableIsSet("GUMM_DEBUG")) {
        static int n = 0;
        if (n++ % 120 == 0)
            qInfo("[mouse] evdev=%d screen='%s' global=(%d,%d) n=(%.2f,%.2f)",
                  _evdev.running() ? 1 : 0, qPrintable(scr->name()),
                  gp.x(), gp.y(), double(nx), double(ny));
    }
}

Channels ModelView::composeChannels(float dt)
{
    // O modelo deve olhar para o cursor global, mesmo com o mouse fora da
    // janela do app (o usuário vai para outro monitor/janela). Lemos a
    // posição global a cada frame e normalizamos em relação ao centro da
    // janela, usando um raio de referência para dar alcance natural.
    if (_settings.mouseEnabled)
        pollGlobalMouse();

    _mouse.tick(dt);
    updateBlink(dt);

    Channels c; // valores neutros

    // Respiração procedural (usada pelo mapeamento "Breathing -> ParamBreath")
    c.breath = _settings.breathing
                   ? (std::sin(_time * 1.4f) * 0.5f + 0.5f)
                   : 0.0f;

    // ---- Webcam (tem prioridade quando encontra rosto) ----
    const bool camActive = _settings.webcamEnabled && _cam.running();
    const FaceReading& f = _cam.face();
    const float camTarget = (camActive && f.found) ? 1.0f : 0.0f;
    const float fadeSpeed = (camTarget > _camFade) ? 6.0f : 2.5f; // entra rápido, sai devagar
    _camFade += (camTarget - _camFade) * (1.0f - std::exp(-dt * fadeSpeed));

    Channels camCh;
    if (camActive) {
        const FaceNeutral n = _cam.neutral();
        const float eyeXOff  = n.valid ? n.eyeX  : _settings.calEyeX;
        const float eyeYOff  = n.valid ? n.eyeY  : _settings.calEyeY;
        const float mouthOff = n.valid ? n.mouth : _settings.calMouth;
        const float browOff  = n.valid ? n.brow  : _settings.calBrow;

        _faceInput.setFilterParams(_settings.oneEuroMinCutoff, _settings.oneEuroBeta);
        FaceAngles fa = _faceInput.compute(f, n, _settings, dt);
        camCh.headX = fa.x;
        camCh.headY = fa.y;
        camCh.headZ = fa.z;

        camCh.bodyX = camCh.headX * 0.4f;
        camCh.bodyY = 0.0f;
        camCh.bodyZ = camCh.headZ * 0.4f;
        // Histerese de piscada: quando aberto, fecha com blink ≥ 0.55;
        // quando fechado, só reabre com blink ≤ 0.30. As curvas de abertura
        // são deslocadas junto, então a transição é suave e sem tremor no limiar.
        auto eyeOpenHyst = [](float v, bool& closed) {
            if (!closed && v >= 0.55f) closed = true;
            else if (closed && v <= 0.30f) closed = false;
            const float lo = closed ? 0.30f : 0.15f;  // faixa de fechamento
            const float hi = closed ? 0.70f : 0.55f;
            return clampf(1.0f - (v - lo) / (hi - lo), 0.0f, 1.0f);
        };
        camCh.eyeOpenL = eyeOpenHyst(f.blinkL, _blinkClosedL);
        camCh.eyeOpenR = eyeOpenHyst(f.blinkR, _blinkClosedR);
        // Íris: deadzone circular (ignora micro-tremor quando olhando reto)
        float ex = (f.eyeX - eyeXOff) * 4.0f;
        float ey = -(f.eyeY - eyeYOff) * 4.0f;
        if (std::hypot(ex, ey) < 0.22f) { ex = 0.0f; ey = 0.0f; }
        camCh.eyeBallX = clampf(ex, -1.0f, 1.0f) * _settings.sensEye;
        camCh.eyeBallY = clampf(ey, -1.0f, 1.0f) * _settings.sensEye;
        camCh.brows    = clampf(0.5f + (f.brow - browOff) * 1.2f, 0.0f, 1.0f);
        camCh.mouthOpen= clampf((f.jaw - mouthOff) / 0.55f, 0.0f, 1.0f);
        camCh.mouthSmile = clampf(0.5f + f.smile, 0.0f, 1.0f);
        camCh.mouthPucker  = clampf(f.pucker, 0.0f, 1.0f);
        camCh.mouthFunnel  = clampf(f.funnel, 0.0f, 1.0f);
        camCh.cheekPuff    = clampf(f.cheek, 0.0f, 1.0f);
        camCh.browDown     = clampf(f.browDown, 0.0f, 1.0f);
        camCh.jawSide      = clampf(f.jawSide, -1.0f, 1.0f);
        camCh.frown        = clampf(f.frown, 0.0f, 1.0f);
        camCh.mouthStretch = clampf(f.stretch, 0.0f, 1.0f);
        camCh.breath   = c.breath;
    }

    // ---- Mouse ----
    Channels mouCh;
    if (_settings.mouseEnabled) {
        const float ix = _settings.invertHeadX ? -1.0f : 1.0f;
        const float iy = _settings.invertHeadY ? -1.0f : 1.0f;
        const float iz = _settings.invertHeadZ ? -1.0f : 1.0f;
        mouCh.headX = clampf(_mouse.x() * _settings.sensHead * ix, -1.0f, 1.0f);
        mouCh.headY = clampf(_mouse.y() * _settings.sensHead * iy, -1.0f, 1.0f);
        mouCh.headZ = 0.0f;
        mouCh.bodyX = mouCh.headX * 0.4f;
        mouCh.bodyY = 0.0f;
        mouCh.bodyZ = mouCh.headZ * iz * 0.4f;
        mouCh.eyeBallX = clampf(_mouse.x() * _settings.sensEye * ix, -1.0f, 1.0f);
        mouCh.eyeBallY = clampf(_mouse.y() * _settings.sensEye * iy, -1.0f, 1.0f);
        mouCh.eyeOpenL = 1.0f - _blinkValue;
        mouCh.eyeOpenR = 1.0f - _blinkValue;
        mouCh.mouthOpen = 0.0f;
    } else {
        mouCh.eyeOpenL = mouCh.eyeOpenR = 1.0f;
    }

    // Microfone sempre alimenta a boca quando ativo
    const float micMouth = (_settings.micEnabled && _mic.running()) ? _mic.mouthOpen() : 0.0f;

    // ---- Mistura webcam x mouse ----
    const float a = clampf(_camFade, 0.0f, 1.0f);
    c.headX     = mouCh.headX * (1 - a) + camCh.headX * a;
    c.headY     = mouCh.headY * (1 - a) + camCh.headY * a;
    c.headZ     = mouCh.headZ * (1 - a) + camCh.headZ * a;
    c.bodyX     = mouCh.bodyX * (1 - a) + camCh.bodyX * a;
    c.bodyY     = mouCh.bodyY * (1 - a) + camCh.bodyY * a;
    c.bodyZ     = mouCh.bodyZ * (1 - a) + camCh.bodyZ * a;
    c.eyeOpenL  = mouCh.eyeOpenL * (1 - a) + camCh.eyeOpenL * a;
    c.eyeOpenR  = mouCh.eyeOpenR * (1 - a) + camCh.eyeOpenR * a;
    c.eyeBallX  = mouCh.eyeBallX * (1 - a) + camCh.eyeBallX * a;
    c.eyeBallY  = mouCh.eyeBallY * (1 - a) + camCh.eyeBallY * a;
    c.brows     = 0.5f * (1 - a) + camCh.brows * a;
    c.mouthSmile= 0.5f * (1 - a) + camCh.mouthSmile * a;
    c.mouthPucker  = camCh.mouthPucker * a;
    c.mouthFunnel  = camCh.mouthFunnel * a;
    c.cheekPuff    = camCh.cheekPuff * a;
    c.browDown     = camCh.browDown * a;
    c.jawSide      = camCh.jawSide * a;
    c.frown        = camCh.frown * a;
    c.mouthStretch = camCh.mouthStretch * a;
    c.mouthOpen = qMax(micMouth, camCh.mouthOpen * a);
    return c;
}

void ModelView::updateBlink(float dt)
{
    _blinkCountdown -= dt;
    if (_blinkCountdown <= 0.0f) {
        // piscada: dura ~130 ms (ida e volta)
        _blinkValue = 1.0f;
        _blinkCountdown = 2.0f + QRandomGenerator::global()->bounded(3000) / 1000.0f;
    }
    if (_blinkValue > 0.0f) {
        _blinkValue -= dt * 8.0f;
        if (_blinkValue < 0.0f)
            _blinkValue = 0.0f;
        // curva triangular suave
        _blinkValue = _blinkValue;
    }
}

// ---------------------------------------------------------------------------
// Mouse (olhar + arrastar/zoom)
// ---------------------------------------------------------------------------
void ModelView::mouseMoveEvent(QMouseEvent* e)
{
    if (_dragging) {
        const QPoint d = e->pos() - _lastDragPos;
        _lastDragPos = e->pos();
        const float w = qMax(1, width());
        const float h = qMax(1, height());
        _settings.panX += 2.0f * d.x() / w;
        _settings.panY -= 2.0f * d.y() / h;
        emit viewAdjusted(_settings.zoom, _settings.panX, _settings.panY);
        return;
    }
    // O tracking do olhar é feito por pollGlobalMouse() a cada frame, que
    // também funciona com o cursor fora da janela. Enquanto o cursor está
    // sob a janela, o Qt entrega a posição global real — usamos para
    // recalibrar o acumulador do evdev (evita deriva ao longo do tempo).
    if (_evdev.running())
        _evdev.setPos(e->globalPosition().toPoint());
    QOpenGLWidget::mouseMoveEvent(e);
}

void ModelView::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        _dragging = true;
        _lastDragPos = e->pos();
        setCursor(Qt::ClosedHandCursor);
    }
    QOpenGLWidget::mousePressEvent(e);
}

void ModelView::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        _dragging = false;
        unsetCursor();
    }
    QOpenGLWidget::mouseReleaseEvent(e);
}

void ModelView::wheelEvent(QWheelEvent* e)
{
    const float steps = e->angleDelta().y() / 120.0f;
    _settings.zoom = clampf(_settings.zoom * std::pow(1.12f, steps), 0.15f, 10.0f);
    emit viewAdjusted(_settings.zoom, _settings.panX, _settings.panY);
    e->accept();
}

void ModelView::leaveEvent(QEvent* e)
{
    QOpenGLWidget::leaveEvent(e);
}
