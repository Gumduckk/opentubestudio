#include "MainWindow.h"
#include "SetupWizard.h"

#include "../gl/ModelView.h"

#include <QApplication>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QDockWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPixmap>
#include <QImage>
#include <QPushButton>
#include <QToolButton>
#include <QCheckBox>
#include <QSlider>
#include <QComboBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QGuiApplication>
#include <cstdio>
#include <QScreen>
#include <QRadioButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QColorDialog>
#include <QCloseEvent>
#include <QAction>
#include <QMenu>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QMediaDevices>
#include <QCameraDevice>
#include <QAudioDevice>
#include <QSettings>
#include <QScroller>

// ---------------------------------------------------------------------------
// Construção
// ---------------------------------------------------------------------------
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("OpentubeStudio — Live2D no Linux"));
    _settings = AppConfig::load();

    _persistDebounce.setSingleShot(true);
    _persistDebounce.setInterval(400);
    connect(&_persistDebounce, &QTimer::timeout, this, [this]() { persist(); });

    buildUi();
    refreshWebcamDevices();
    refreshMicDevices();
    refreshScreenDevices();

    connect(_view, &ModelView::viewAdjusted, this, &MainWindow::persistView);
    connect(_view, &ModelView::fpsUpdated, this, [this](float fps) {
        _lblFps->setText(QStringLiteral("%1 fps").arg(fps, 0, 'f', 0));
    });
    connect(_view->webcam(), &WebcamTracker::neutralCaptured, this,
            [this](const FaceNeutral& n) {
                if (!n.valid) {
                    _lblCalibStatus->setText(QStringLiteral("Calibração falhou (nenhum rosto)"));
                    return;
                }
                _settings.calYaw = n.yaw;   _settings.calPitch = n.pitch;
                _settings.calRoll = n.roll; _settings.calEyeX = n.eyeX;
                _settings.calEyeY = n.eyeY; _settings.calMouth = n.mouth;
                _settings.calBrow = n.brow; _settings.calibrated = true;
                _lblCalibStatus->setText(QStringLiteral("✓ Calibrado! Posição neutra salva."));
                _persistDebounce.start();
            });
    connect(_view->webcam(), &WebcamTracker::failed, this, [this](const QString& r) {
        _lblCalibStatus->setText(r);
        QMessageBox::warning(this, QStringLiteral("Webcam"),
                             r + QStringLiteral("\n\nDica: rode ./setup.sh para instalar o "
                               "MediaPipe (mediapipe + opencv) em tools/venv."));
    });

    // Carrega modelo salvo, ou inicia o assistente na primeira vez
    applySettings();
    // Se a webcam foi deixada ligada na sessão anterior, já começa o rastreio
    // ao abrir o app (sem precisar marcar a caixa novamente).
    if (_settings.webcamEnabled)
        startWebcamIfEnabled();
    if (!_settings.modelPath.isEmpty() && QFileInfo::exists(_settings.modelPath)) {
        loadModel(_settings.modelPath);
        if (_settings.mappings.isEmpty())
            autoConfigure();
    } else {
        QTimer::singleShot(350, this, [this]() {
            if (!_wizardSuppressed)
                runWizard();
        });
    }

    // Status de rastreio (atualiza periodicamente)
    auto* statusTimer = new QTimer(this);
    connect(statusTimer, &QTimer::timeout, this, [this]() {
        QString t;
        if (_view->webcam()->running()) {
            const auto& f = _view->webcam()->face();
t = f.found
    ? QStringLiteral("Webcam ✓  yaw=%1 pitch=%2 roll=%3  jaw=%4 smile=%5  eyeX=%6 eyeY=%7")
          .arg(f.yaw,0,'f',1).arg(f.pitch,0,'f',1).arg(f.roll,0,'f',1)
          .arg(f.jaw,0,'f',2).arg(f.smile,0,'f',2)
          .arg(f.eyeX,0,'f',2).arg(f.eyeY,0,'f',2)
    : QStringLiteral("Webcam: procurando rosto...");
        } else if (_view->webcam()->starting()) {
            t = QStringLiteral("Webcam: iniciando...");
        } else {
            t = (_settings.mouseEnabled ? QStringLiteral("Mouse ") : QString())
                + (_settings.micEnabled ? QStringLiteral("+ Microfone") : QString());
            if (t.isEmpty())
                t = QStringLiteral("Sem rastreio");
        }
        _lblTracking->setText(t);
    });
    statusTimer->start(800);
}

MainWindow::~MainWindow() = default;

void MainWindow::openModelPath(const QString& model3Path)
{
    if (qEnvironmentVariableIsSet("GUMM_DEBUG"))
        fprintf(stderr, "[trace] openModelPath('%s')\n", qPrintable(model3Path));
    if (model3Path.isEmpty() || !QFileInfo::exists(model3Path))
        return;
    _wizardSuppressed = true;
    loadModel(model3Path);
    if (_settings.mappings.isEmpty())
        autoConfigure();
}

// ---------------------------------------------------------------------------
// UI
// ---------------------------------------------------------------------------
void MainWindow::buildUi()
{
    _view = new ModelView(this);
    setCentralWidget(_view);

    // ---- Barra de ferramentas ----
    auto* tb = addToolBar(QStringLiteral("Principal"));
    tb->setMovable(false);

    auto* actOpen = tb->addAction(QStringLiteral("📂 Abrir modelo"), this,
                                  &MainWindow::openModelDialog);
    actOpen->setShortcut(QKeySequence(QStringLiteral("Ctrl+O")));

    auto* actAuto = tb->addAction(QStringLiteral("⚙ Configuração automática"), this,
                                  &MainWindow::autoConfigure);
    actAuto->setShortcut(QKeySequence(QStringLiteral("Ctrl+W")));

    auto* actCalib = tb->addAction(QStringLiteral("🎯 Calibrar"), this,
                                   &MainWindow::calibrateWebcam);
    actCalib->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+C")));

    tb->addSeparator();

    // Fundo rápido
    _bgQuickButton = new QToolButton(this);
    _bgQuickButton->setText(QStringLiteral("Fundo"));
    _bgQuickButton->setPopupMode(QToolButton::InstantPopup);
    auto* bgMenu = new QMenu(_bgQuickButton);
    auto addPreset = [&](const QString& name, const QColor& c) {
        auto* a = bgMenu->addAction(name);
        a->setData(c.name());
        connect(a, &QAction::triggered, this, [this, c]() {
            _settings.transparentBackground = false;
            _settings.backgroundColor = c;
            _rebuildingUi = true;
            _bgSolid->setChecked(true);
            _rebuildingUi = false;
            applySettings();
        });
    };
    addPreset(QStringLiteral("Verde OBS (recomendado)"), QColor(0, 200, 60));
    addPreset(QStringLiteral("Verde puro"), QColor(0, 255, 0));
    addPreset(QStringLiteral("Azul"), QColor(0, 0, 255));
    addPreset(QStringLiteral("Magenta"), QColor(255, 0, 255));
    addPreset(QStringLiteral("Branco"), QColor(255, 255, 255));
    bgMenu->addSeparator();
    auto* aCustom = bgMenu->addAction(QStringLiteral("Cor personalizada..."));
    connect(aCustom, &QAction::triggered, this, [this]() {
        const QColor c = QColorDialog::getColor(_settings.backgroundColor, this);
        if (c.isValid()) {
            _settings.backgroundColor = c;
            _settings.transparentBackground = false;
            applySettings();
        }
    });
    auto* aTransp = bgMenu->addAction(QStringLiteral("Transparente (alpha)"));
    connect(aTransp, &QAction::triggered, this, [this]() {
        _settings.transparentBackground = true;
        _rebuildingUi = true;
        _bgTransparent->setChecked(true);
        _rebuildingUi = false;
        applySettings();
    });
    _bgQuickButton->setMenu(bgMenu);
    tb->addWidget(_bgQuickButton);

    auto* actObs = tb->addAction(QStringLiteral("🎬 Modo OBS"));
    actObs->setCheckable(true);
    actObs->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+O")));
    connect(actObs, &QAction::toggled, this, [this, actObs](bool on) {
        const QRect g = geometry();
        _settings.frameless = on;
        if (on) {
            // Guarda o fundo atual para restaurar quando o Modo OBS for desligado.
            if (!_bgSavedOnObs) {
                _obsSavedTransparent = _settings.transparentBackground;
                _obsSavedColor = _settings.backgroundColor;
                _bgSavedOnObs = true;
            }
            _settings.transparentBackground = true;
            _dock->hide();
            resize(1280, 720);
            move(QGuiApplication::primaryScreen()->availableGeometry().center()
                 - QPoint(640, 360));
        } else {
            // Restaura o fundo que estava configurado antes do Modo OBS.
            if (_bgSavedOnObs) {
                _settings.transparentBackground = _obsSavedTransparent;
                _settings.backgroundColor = _obsSavedColor;
                _bgSavedOnObs = false;
            }
            _dock->show();
            setGeometry(g);
        }
        applySettings();
        actObs->blockSignals(false);
    });

    auto* actTop = tb->addAction(QStringLiteral("📌 Sempre visível"));
    actTop->setCheckable(true);
    connect(actTop, &QAction::toggled, this, [this](bool on) {
        _settings.alwaysOnTop = on;
        applySettings();
    });

    auto* actReset = tb->addAction(QStringLiteral("🔍 Centralizar"), this, [this]() {
        _view->resetView();
    });
    actReset->setShortcut(QKeySequence(QStringLiteral("Ctrl+R")));

    // ---- Painel lateral ----
    _dock = new QDockWidget(QStringLiteral("Configurações"), this);
    _dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    _tabs = new QTabWidget(_dock);
    buildTrackingTab();
    buildMappingTab();
    buildBackgroundTab();
    buildModelTab();
    _dock->setWidget(_tabs);
    addDockWidget(Qt::RightDockWidgetArea, _dock);

    // ---- Barra de status ----
    _lblModel = new QLabel(QStringLiteral("Nenhum modelo carregado"), this);
    _lblFps = new QLabel(QStringLiteral("— fps"), this);
    _lblTracking = new QLabel(QStringLiteral("—"), this);
    statusBar()->addWidget(_lblModel, 1);
    statusBar()->addPermanentWidget(_lblTracking);
    statusBar()->addPermanentWidget(_lblFps);

    resize(1200, 760);
}

void MainWindow::buildTrackingTab()
{
    auto* page = new QWidget(_tabs);
    auto* v = new QVBoxLayout(page);

    _chkMouse = new QCheckBox(QStringLiteral("🖱 Mouse (cabeça/olhos seguem o cursor)"), page);
    _chkMic = new QCheckBox(QStringLiteral("🎤 Microfone (lip sync pela voz)"), page);
    _chkWebcam = new QCheckBox(QStringLiteral("📷 Webcam (rastreio facial)"), page);
    _chkDebugCam = new QCheckBox(QStringLiteral("🔬 Mostrar debug do rastreio (janela com a câmera e os pontos)"), page);
    v->addWidget(_chkMouse);
    v->addWidget(_chkMic);
    v->addWidget(_chkWebcam);
    v->addWidget(_chkDebugCam);

    // Painel do debug: mostra o frame anotado direto no app
    _dbgBox = new QWidget(page);
    auto* dbgLayout = new QVBoxLayout(_dbgBox);
    dbgLayout->setContentsMargins(0, 4, 0, 0);
    auto* dbgTitle = new QLabel(QStringLiteral("🔬 Câmera de rastreio (malha + valores)"), _dbgBox);
    dbgTitle->setStyleSheet(QStringLiteral("color: gray;"));
    dbgLayout->addWidget(dbgTitle);
    _dbgImg = new QLabel(_dbgBox);
    _dbgImg->setFixedSize(320, 180);
    _dbgImg->setScaledContents(true);
    _dbgImg->setStyleSheet(QStringLiteral(
        "border: 1px solid #555; border-radius: 6px; background: #111;"));
    dbgLayout->addWidget(_dbgImg);
    _dbgBox->setVisible(false);
    v->addWidget(_dbgBox);

    _comboCam = new QComboBox(page);
    _comboMic = new QComboBox(page);
    v->addWidget(new QLabel(QStringLiteral("Câmera:"), page));
    v->addWidget(_comboCam);
    v->addWidget(new QLabel(QStringLiteral("Microfone:"), page));
    v->addWidget(_comboMic);

    auto addSlider = [&](const QString& title, int min, int max) {
        v->addWidget(new QLabel(title, page));
        auto* s = new QSlider(Qt::Horizontal, page);
        s->setRange(min, max);
        v->addWidget(s);
        return s;
    };
    _sldSensHead = addSlider(QStringLiteral("Sensibilidade da cabeça"), 20, 300);
    _sldSensEye = addSlider(QStringLiteral("Sensibilidade dos olhos"), 20, 300);
    _sldSensPos = addSlider(QStringLiteral("Sensibilidade do corpo/lean (posição 3D)"), 20, 300);
    _sldSmoothing = addSlider(QStringLiteral("Suavização dos parâmetros (menor = mais rápido)"), 20, 200);
    _sldMicGain = addSlider(QStringLiteral("Ganho do microfone (boca)"), 10, 200);
    _sldMicThreshold = addSlider(QStringLiteral("Limiar de ruído (mais alto = menos falso-trigger)"), 0, 200);

    _chkInvertX = new QCheckBox(QStringLiteral("Inverter eixo X (esquerda/direita)"), page);
    _chkInvertY = new QCheckBox(QStringLiteral("Inverter eixo Y (cima/baixo)"), page);
    _chkBreath = new QCheckBox(QStringLiteral("Respiração automática"), page);
    v->addWidget(_chkInvertX);
    v->addWidget(_chkInvertY);
    v->addWidget(_chkBreath);

    auto* calib = new QPushButton(QStringLiteral("🎯 Calibrar posição neutra (webcam)"), page);
    v->addWidget(calib);
    auto* refreshCam = new QPushButton(QStringLiteral("↻ Procurar câmeras (/dev/video*)"), page);
    v->addWidget(refreshCam);
    _lblCalibStatus = new QLabel(page);
    _lblCalibStatus->setWordWrap(true);
    v->addWidget(_lblCalibStatus);
    v->addStretch();

    QScroller::grabGesture(page, QScroller::LeftMouseButtonGesture);

    // ---- conexões ----
    connect(_chkMouse, &QCheckBox::toggled, this, [this](bool on) {
        _settings.mouseEnabled = on; applySettings();
    });
    connect(_chkMic, &QCheckBox::toggled, this, [this](bool on) {
        _settings.micEnabled = on; applySettings();
    });
    connect(_chkWebcam, &QCheckBox::toggled, this, [this](bool on) {
        _settings.webcamEnabled = on;
        applySettings();
        if (on) {
            _view->webcam()->setDebugCam(_settings.debugCam);
            startWebcamIfEnabled();
        } else {
            // Libera a câmera imediatamente (o LED apaga e o daemon encerra).
            _view->webcam()->stop();
            _lblCalibStatus->setText(QStringLiteral("Rastreio facial desligado."));
        }
    });
    connect(_view->webcam(), &WebcamTracker::debugFrame, this, [this](const QImage& img) {
        if (!_dbgBox || !_dbgBox->isVisible() || !_dbgImg)
            return;
        _dbgImg->setPixmap(QPixmap::fromImage(img));
    });
    connect(_chkDebugCam, &QCheckBox::toggled, this, [this](bool on) {
        _settings.debugCam = on;
        _persistDebounce.start();
        if (_dbgBox)
            _dbgBox->setVisible(on);
        _view->webcam()->setDebugCam(on);
    });
    connect(_comboCam, &QComboBox::currentIndexChanged, this, [this](int i) {
        if (_rebuildingUi || i < 0)
            return;
        _settings.webcamIndex = _comboCam->itemData(i).toInt();
        _persistDebounce.start();
        if (_settings.webcamEnabled) startWebcamIfEnabled();
    });
    connect(_comboMic, &QComboBox::currentIndexChanged, this, [this](int) {
        const auto mics = QMediaDevices::audioInputs();
        const int i = _comboMic->currentIndex();
        if (i >= 0 && i < mics.size())
            _settings.micDevice = QString::fromUtf8(mics[i].id());
        _persistDebounce.start();
    });
    connect(_sldSensHead, &QSlider::valueChanged, this, [this](int v2) {
        _settings.sensHead = v2 / 100.0f; _persistDebounce.start();
    });
    connect(_sldSensEye, &QSlider::valueChanged, this, [this](int v2) {
        _settings.sensEye = v2 / 100.0f; _persistDebounce.start();
    });
    connect(_sldSensPos, &QSlider::valueChanged, this, [this](int v2) {
        _settings.sensPos = v2 / 100.0f; _persistDebounce.start();
    });
    connect(_sldSmoothing, &QSlider::valueChanged, this, [this](int v2) {
        _settings.smoothingScale = v2 / 100.0f;
        applySettings();
        _persistDebounce.start();
    });
    connect(_sldMicGain, &QSlider::valueChanged, this, [this](int v2) {
        _settings.micGain = v2 / 10.0f; applySettings();
    });
    connect(_sldMicThreshold, &QSlider::valueChanged, this, [this](int v2) {
        _settings.micThreshold = v2 / 1000.0f; applySettings();
    });
    connect(_chkInvertX, &QCheckBox::toggled, this, [this](bool on) {
        _settings.invertHeadX = on; _persistDebounce.start();
    });
    connect(_chkInvertY, &QCheckBox::toggled, this, [this](bool on) {
        _settings.invertHeadY = on; _persistDebounce.start();
    });
    connect(_chkBreath, &QCheckBox::toggled, this, [this](bool on) {
        _settings.breathing = on; _persistDebounce.start();
    });
    connect(calib, &QPushButton::clicked, this, &MainWindow::calibrateWebcam);
    connect(refreshCam, &QPushButton::clicked, this, [this]() {
        refreshWebcamDevices();
        _lblCalibStatus->setText(QStringLiteral("Câmeras atualizadas."));
    });

    _tabs->addTab(page, QStringLiteral("Rastreio"));
}

void MainWindow::buildMappingTab()
{
    auto* page = new QWidget(_tabs);
    auto* v = new QVBoxLayout(page);
    auto* hint = new QLabel(
        QStringLiteral("Como o rastreio vira parâmetros do modelo "
                       "(mesmo esquema do VTube Studio)."), page);
    hint->setWordWrap(true);
    v->addWidget(hint);

    _mapTable = new QTableWidget(0, 7, page);
    _mapTable->setHorizontalHeaderLabels(
        { QStringLiteral("Entrada"), QStringLiteral("Parâmetro"),
          QStringLiteral("In min"), QStringLiteral("In max"),
          QStringLiteral("Out min"), QStringLiteral("Out max"),
          QStringLiteral("Suav.") });
    _mapTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    _mapTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    _mapTable->verticalHeader()->setVisible(false);
    v->addWidget(_mapTable, 1);

    auto* h = new QHBoxLayout;
    auto* btnAdd = new QPushButton(QStringLiteral("+ Adicionar"), page);
    auto* btnDel = new QPushButton(QStringLiteral("− Remover"), page);
    auto* btnDef = new QPushButton(QStringLiteral("Restaurar padrão"), page);
    h->addWidget(btnAdd);
    h->addWidget(btnDel);
    h->addWidget(btnDef);
    h->addStretch();
    v->addLayout(h);

    connect(btnAdd, &QPushButton::clicked, this, [this]() {
        ParamMapping m;
        m.input = QStringLiteral("FaceAngleX");
        m.output = QStringLiteral("ParamAngleX");
        m.inMin = -30; m.inMax = 30; m.outMin = -30; m.outMax = 30; m.smoothing = 15;
        _settings.mappings.append(m);
        rebuildMappingTable();
        _persistDebounce.start();
    });
    connect(btnDel, &QPushButton::clicked, this, [this]() {
        const int r = _mapTable->currentRow();
        if (r >= 0 && r < _settings.mappings.size()) {
            _settings.mappings.removeAt(r);
            rebuildMappingTable();
            _view->updateSettings(_settings);
            _persistDebounce.start();
        }
    });
    connect(btnDef, &QPushButton::clicked, this, &MainWindow::autoConfigure);

    connect(_mapTable, &QTableWidget::cellChanged, this, [this](int r, int c) {
        if (_rebuildingUi)
            return;
        if (r < 0 || r >= _settings.mappings.size())
            return;
        ParamMapping& m = _settings.mappings[r];
        auto text = [this, r, c]() {
            return _mapTable->item(r, c) ? _mapTable->item(r, c)->text() : QString();
        };
        auto number = [this, r, c](float def) {
            const QString t = _mapTable->item(r, c) ? _mapTable->item(r, c)->text() : QString();
            bool ok = false;
            const float v = t.toFloat(&ok);
            return ok ? v : def;
        };
        switch (c) {
        case 0: m.input = text(); break;
        case 1: m.output = text(); break;
        case 2: m.inMin = number(m.inMin); break;
        case 3: m.inMax = number(m.inMax); break;
        case 4: m.outMin = number(m.outMin); break;
        case 5: m.outMax = number(m.outMax); break;
        case 6: m.smoothing = number(m.smoothing); break;
        }
        _persistDebounce.start();
    });

    _tabs->addTab(page, QStringLiteral("Parâmetros"));

    // Aba de debug
    auto* dbgPage = new QWidget(_tabs);
    auto* dbgV = new QVBoxLayout(dbgPage);
    auto* dbgLabel = new QLabel(QStringLiteral("Debug..."), dbgPage);
    dbgLabel->setFont(QFont(QStringLiteral("Monospace"), 10));
    dbgV->addWidget(dbgLabel);
    dbgV->addStretch();
    _tabs->addTab(dbgPage, QStringLiteral("Debug"));

    auto* dbgTimer = new QTimer(this);
    connect(dbgTimer, &QTimer::timeout, this, [this, dbgLabel]() {
        if (_view->webcam()->running()) {
            const auto& f = _view->webcam()->face();
            dbgLabel->setText(QStringLiteral("esq/dir (yaw)=%1\ncima/baixo (pitch)=%2\ninclinação (roll)=%3\nrosto=%4")
                                 .arg(f.yaw,0,'f',2).arg(f.pitch,0,'f',2).arg(f.roll,0,'f',2)
                                 .arg(f.found ? QStringLiteral("sim") : QStringLiteral("não")));
        } else {
            dbgLabel->setText(QStringLiteral("Webcam inativa"));
        }
    });
    dbgTimer->start(200);
}

void MainWindow::buildBackgroundTab()
{
    auto* page = new QWidget(_tabs);
    auto* v = new QVBoxLayout(page);

    v->addWidget(new QLabel(QStringLiteral("Tipo de fundo:"), page));
    _bgSolid = new QRadioButton(QStringLiteral("Cor sólida (tela verde p/ OBS)"), page);
    _bgTransparent = new QRadioButton(QStringLiteral("Transparente (alpha real)"), page);
    v->addWidget(_bgSolid);
    v->addWidget(_bgTransparent);

    auto* h = new QHBoxLayout;
    _bgColorBtn = new QPushButton(QStringLiteral("Escolher cor..."), page);
    auto* presets = new QToolButton(page);
    presets->setText(QStringLiteral("Predefinições"));
    presets->setPopupMode(QToolButton::InstantPopup);
    auto* menu = new QMenu(presets);
    for (const auto& p : { std::pair<const char*, QColor>("Verde OBS", QColor(0, 200, 60)),
                           std::pair<const char*, QColor>("Verde puro", QColor(0, 255, 0)),
                           std::pair<const char*, QColor>("Azul", QColor(0, 0, 255)),
                           std::pair<const char*, QColor>("Magenta", QColor(255, 0, 255)),
                           std::pair<const char*, QColor>("Branco", QColor(255, 255, 255)) }) {
        auto* a = menu->addAction(QString::fromLatin1(p.first));
        const QColor c = p.second;
        connect(a, &QAction::triggered, this, [this, c]() {
            _settings.backgroundColor = c;
            _settings.transparentBackground = false;
            applySettings();
        });
    }
    presets->setMenu(menu);
    h->addWidget(_bgColorBtn);
    h->addWidget(presets);
    h->addStretch();
    v->addLayout(h);

    connect(_bgSolid, &QRadioButton::toggled, this, [this](bool on) {
        if (_rebuildingUi || !on)
            return;
        _settings.transparentBackground = false;
        applySettings();
    });
    connect(_bgTransparent, &QRadioButton::toggled, this, [this](bool on) {
        if (_rebuildingUi || !on)
            return;
        _settings.transparentBackground = true;
        applySettings();
    });
    connect(_bgColorBtn, &QPushButton::clicked, this, [this]() {
        const QColor c = QColorDialog::getColor(_settings.backgroundColor, this,
                                                QStringLiteral("Cor de fundo"));
        if (c.isValid()) {
            _settings.backgroundColor = c;
            _settings.transparentBackground = false;
            applySettings();
        }
    });

    _chkFrameless = new QCheckBox(QStringLiteral("Sem moldura (janela invisível p/ captura)"), page);
    _chkAlwaysOnTop = new QCheckBox(QStringLiteral("Sempre visível (por cima)"), page);
    connect(_chkFrameless, &QCheckBox::toggled, this, [this](bool on) {
        _settings.frameless = on; applySettings();
    });
    connect(_chkAlwaysOnTop, &QCheckBox::toggled, this, [this](bool on) {
        _settings.alwaysOnTop = on; applySettings();
    });
    v->addWidget(_chkFrameless);
    v->addWidget(_chkAlwaysOnTop);

    // Seletor de monitor usado pelo rastreio de mouse (multi-monitor).
    v->addWidget(new QLabel(QStringLiteral("Monitor usado pelo rastreio (mouse):"), page));
    _comboScreen = new QComboBox(page);
    v->addWidget(_comboScreen);

    auto* h2 = new QHBoxLayout;
    _comboResolution = new QComboBox(page);
    _comboResolution->addItems({ QStringLiteral("Tamanho atual"),
                                 QStringLiteral("1920×1080"),
                                 QStringLiteral("1280×720"),
                                 QStringLiteral("1080×1920 (vertical)") });
    // A conexão é feita aqui, depois que _comboScreen foi criado (antes ele
    // ainda era nullptr e o Qt avisava "invalid nullptr parameter").
    connect(_comboScreen, &QComboBox::currentIndexChanged, this, [this](int i) {
        if (_rebuildingUi || i < 0)
            return;
        if (i <= 0) {
            _settings.mouseScreen.clear();
        } else {
            const auto screens = QGuiApplication::screens();
            if (i - 1 < screens.size())
                _settings.mouseScreen = screens[i - 1]->name();
        }
        _persistDebounce.start();
    });
    auto* btnResize = new QPushButton(QStringLiteral("Redimensionar"), page);
    connect(btnResize, &QPushButton::clicked, this, [this]() {
        switch (_comboResolution->currentIndex()) {
        case 1: resize(1920, 1080); break;
        case 2: resize(1280, 720); break;
        case 3: resize(1080, 1920); break;
        default: break;
        }
    });
    h2->addWidget(new QLabel(QStringLiteral("Resolução p/ OBS:"), page));
    h2->addWidget(_comboResolution, 1);
    h2->addWidget(btnResize);
    v->addLayout(h2);

    auto* obsHelp = new QLabel(page);
    obsHelp->setWordWrap(true);
    obsHelp->setTextFormat(Qt::RichText);
    obsHelp->setText(QStringLiteral(
        "<b>Como usar no OBS Studio</b><br>"
        "1. Deixe o fundo <b>verde</b> aqui no OpentubeStudio.<br>"
        "2. No OBS: <i>Fonte → + → Captura de Tela</i> (PipeWire) e escolha a "
        "janela do OpentubeStudio — ou <i>Captura de Janela</i> se estiver no X11.<br>"
        "3. No filtro da fonte: <i>+ → Chroma Key</i>, cor de corte "
        "<b>Verde</b> (Similaridade ~200, Suavidade ~80).<br><br>"
        "<b>Fundo transparente:</b> funciona melhor no X11 (ou rodando com "
        "<code>QT_QPA_PLATFORM=xcb</code>). Nesse modo use <i>Captura de Janela</i> "
        "e adicione <i>Recortar</i> se quiser. No Wayland puro, prefira tela verde."
        ));
    v->addWidget(obsHelp);
    v->addStretch();

    _tabs->addTab(page, QStringLiteral("Fundo/OBS"));
}

void MainWindow::buildModelTab()
{
    auto* page = new QWidget(_tabs);
    auto* v = new QVBoxLayout(page);

    auto* btnOpen = new QPushButton(QStringLiteral("📂 Trocar modelo..."), page);
    btnOpen->connect(btnOpen, &QPushButton::clicked, this, &MainWindow::openModelDialog);
    v->addWidget(btnOpen);

    _lblModelInfo = new QLabel(page);
    _lblModelInfo->setWordWrap(true);
    v->addWidget(_lblModelInfo);

    v->addWidget(new QLabel(QStringLiteral("<b>Expressões</b> (atalhos Ctrl+1…9):"), page));
    _exprContainer = new QWidget(page);
    v->addWidget(_exprContainer, 1);

    auto* obsNote = new QLabel(
        QStringLiteral("Dica: clique e arraste no visor pra mover o modelo, "
                       "scroll pra zoom, Ctrl+R centraliza."), page);
    obsNote->setWordWrap(true);
    v->addWidget(obsNote);

    _tabs->addTab(page, QStringLiteral("Modelo"));
}

// ---------------------------------------------------------------------------
// Tabela de mapeamentos
// ---------------------------------------------------------------------------
void MainWindow::rebuildMappingTable()
{
    _rebuildingUi = true;
    _mapTable->setRowCount(_settings.mappings.size());
    for (int r = 0; r < _settings.mappings.size(); ++r) {
        const ParamMapping& m = _settings.mappings[r];

        auto* comboIn = new QComboBox(_mapTable);
        comboIn->addItems(Channels::allInputs());
        comboIn->setCurrentText(m.input);
        connect(comboIn, &QComboBox::currentTextChanged, this, [this, r](const QString& t) {
            if (_rebuildingUi || r >= _settings.mappings.size())
                return;
            _settings.mappings[r].input = t;
            _persistDebounce.start();
        });
        _mapTable->setCellWidget(r, 0, comboIn);

        auto* comboOut = new QComboBox(_mapTable);
        fillModelParamCombo(comboOut);
        comboOut->setCurrentText(m.output);
        connect(comboOut, &QComboBox::currentTextChanged, this, [this, r](const QString& t) {
            if (_rebuildingUi || r >= _settings.mappings.size())
                return;
            _settings.mappings[r].output = t;
            _persistDebounce.start();
        });
        _mapTable->setCellWidget(r, 1, comboOut);

        auto setNum = [this, r](int col, float val) {
            auto* item = new QTableWidgetItem(QString::number(val, 'g', 4));
            _mapTable->setItem(r, col, item);
        };
        setNum(2, m.inMin);
        setNum(3, m.inMax);
        setNum(4, m.outMin);
        setNum(5, m.outMax);
        setNum(6, m.smoothing);
    }
    _rebuildingUi = false;
}

void MainWindow::fillModelParamCombo(QComboBox* combo)
{
    combo->clear();
    if (_view->model())
        combo->addItems(_view->model()->parameterIds());
}

// ---------------------------------------------------------------------------
// Expressões
// ---------------------------------------------------------------------------
void MainWindow::rebuildExpressionButtons()
{
    // remove botões antigos
    if (auto* lay = _exprContainer->layout()) {
        QLayoutItem* item;
        while ((item = lay->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
        delete lay;
    }
    auto* grid = new QGridLayout(_exprContainer);
    grid->setContentsMargins(0, 0, 0, 0);
    if (!_view->model())
        return;
    const QStringList names = _view->model()->expressionNames();
    for (int i = 0; i < names.size(); ++i) {
        auto* btn = new QPushButton(QStringLiteral("%1  [%2]").arg(names[i]).arg(i + 1),
                                    _exprContainer);
        btn->setCheckable(true);
        connect(btn, &QPushButton::clicked, this, &MainWindow::onExpressionClicked);
        btn->setProperty("exprName", names[i]);
        grid->addWidget(btn, i / 3, i % 3);
        // atalho Ctrl+N
        if (i < 9) {
            auto* act = new QAction(btn);
            act->setShortcut(QKeySequence(QStringLiteral("Ctrl+%1").arg(i + 1)));
            act->setProperty("exprName", names[i]);
            connect(act, &QAction::triggered, this, [this, names, i]() {
                _view->toggleExpressionByName(names[i]);
                _tabs->setCurrentIndex(3);
                // atualiza o botão correspondente
                const auto buttons = _exprContainer->findChildren<QPushButton*>();
                for (QPushButton* b : buttons) {
                    if (b->property("exprName").toString() == names[i]) {
                        b->setChecked(_view->model()
                                          && _view->model()->isExpressionActive(names[i]));
                    }
                }
            });
            addAction(act);
        }
    }
}

void MainWindow::onExpressionClicked()
{
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn || !_view->model())
        return;
    const QString name = btn->property("exprName").toString();
    _view->toggleExpressionByName(name);
    btn->setChecked(_view->model()->isExpressionActive(name));
}

// ---------------------------------------------------------------------------
// Modelo
// ---------------------------------------------------------------------------
void MainWindow::openModelDialog()
{
    const QString start = _settings.modelPath.isEmpty()
                              ? QDir::homePath()
                              : QFileInfo(_settings.modelPath).absolutePath();
    const QString f = QFileDialog::getOpenFileName(
        this, QStringLiteral("Escolher modelo Live2D"), start,
        QStringLiteral("Modelos Live2D (*.model3.json model.json);;Todos (*)"));
    if (!f.isEmpty())
        loadModel(f);
}

void MainWindow::loadModel(const QString& path)
{
    _wizardSuppressed = true;
    const QString err = _view->loadModel(path);
    if (!err.isEmpty()) {
        QMessageBox::critical(this, QStringLiteral("Erro ao carregar modelo"),
                             QStringLiteral("%1\n\nModelo: %2").arg(err, path));
        return;
    }
    _settings.modelPath = path;
    _lblModel->setText(_view->model()->displayName());

    // Info do modelo
    const QStringList groups = _view->model()->motionGroups();
    _lblModelInfo->setText(QStringLiteral(
        "<b>%1</b><br>Parâmetros: %2<br>Movimentos: %3<br>Física: %4")
        .arg(_view->model()->displayName())
        .arg(_view->model()->parameterIds().size())
        .arg(groups.isEmpty() ? QStringLiteral("nenhum (usa respiração/piscada automáticas)")
                              : groups.join(QStringLiteral(", ")))
        .arg(_view->model()->hasPhysics() ? QStringLiteral("sim ✓")
                                          : QStringLiteral("não")));

    // Se o modelo não tem mapeamento salvo, configura tudo automaticamente;
    // senão, só adiciona os vínculos dos canais novos (sem mexer no resto).
    if (_settings.mappings.isEmpty()) {
        autoConfigure();
    } else {
        const int added = AppConfig::ensureTrackingMappings(
            _settings, _view->model()->parameterIds());
        if (added > 0)
            qInfo("[OpentubeStudio] %d vínculos de tracking novos adicionados ao modelo", added);
        rebuildMappingTable();
    }
    rebuildExpressionButtons();
    persist();
}

void MainWindow::autoConfigure()
{
    if (!_view->model()) {
        QMessageBox::information(this, QStringLiteral("Configuração automática"),
                                 QStringLiteral("Carregue um modelo primeiro."));
        return;
    }
    const bool confirm = !_wizardSuppressed && sender() != nullptr;
    if (confirm &&
        QMessageBox::question(this, QStringLiteral("Configuração automática"),
                              QStringLiteral("Recriar a configuração automática do modelo "
                                             "\"%1\"?\n\nOs vínculos de parâmetros e expressões "
                                             "serão refeitos (fundo e rastreio são mantidos).")
                                  .arg(_view->model()->displayName()))
            != QMessageBox::Yes)
        return;

    const QString summary = AppConfig::buildAutoConfig(_settings.modelPath, _settings,
                                                       _view->model()->parameterIds());
    rebuildMappingTable();
    rebuildExpressionButtons();
    persist();
    if (confirm)
        QMessageBox::information(this, QStringLiteral("Configuração automática"), summary);
}

void MainWindow::calibrateWebcam()
{
    if (!_settings.webcamEnabled) {
        QMessageBox::information(this, QStringLiteral("Calibrar"),
                                 QStringLiteral("Ative a webcam na aba Rastreio primeiro."));
        return;
    }
    if (!_view->webcam()->running()) {
        startWebcamIfEnabled();
        // espera a câmera abrir antes de calibrar
        QTimer::singleShot(1800, this, [this]() {
            if (_view->webcam()->running()) {
                _lblCalibStatus->setText(QStringLiteral("Calibrando... olhe reto pra câmera (~1.5s)"));
                _view->webcam()->calibrate();
            } else {
                _lblCalibStatus->setText(QStringLiteral("Webcam não iniciou."));
            }
        });
        return;
    }
    _lblCalibStatus->setText(QStringLiteral("Calibrando... olhe reto pra câmera (~1.5s)"));
    _view->webcam()->calibrate();
}

// ---------------------------------------------------------------------------
// Assistente
// ---------------------------------------------------------------------------
void MainWindow::runWizard()
{
    // lista de câmeras/microfones
    QStringList camNames, micNames;
    const auto cams = QMediaDevices::videoInputs();
    for (const QCameraDevice& c : cams)
        camNames << c.description();
    const auto mics = QMediaDevices::audioInputs();
    for (const QAudioDevice& m : mics)
        micNames << m.description();

    SetupWizard w(camNames, micNames, this);

    AppSettings temp = _settings; // configuração temporária do assistente
    connect(&w, &SetupWizard::analyzeRequested, this, [&w, &temp, this](const QString& path) {
        const QString err = _view->loadModel(path);
        if (!err.isEmpty()) {
            QMessageBox::critical(&w, QStringLiteral("Erro"), err);
            return;
        }
        temp.modelPath = path;
        const QString summary = AppConfig::buildAutoConfig(
            path, temp, _view->model()->parameterIds());
        w.setSummaryText(summary);
    });
    connect(&w, &SetupWizard::calibrateRequested, this, [&w, &temp, &cams, this]() {
        const int camIdx = w.webcamIndex();
        if (camIdx < 0 || camIdx >= cams.size()) {
            QMessageBox::information(&w, QStringLiteral("Calibrar"),
                                     QStringLiteral("Nenhuma câmera disponível."));
            return;
        }
        temp.webcamDevice = QString::fromUtf8(cams[camIdx].id());
        temp.webcamEnabled = true;
        _view->webcam()->setDebugCam(_settings.debugCam);
        _view->webcam()->start(pythonExecutable(), trackerScriptPath(),
                               faceModelPath(), camIdx);
        QTimer::singleShot(1800, this, [&w, this]() {
            if (_view->webcam()->running())
                _view->webcam()->calibrate();
            else
                w.reject();
        });
    });
    connect(_view->webcam(), &WebcamTracker::neutralCaptured, &w,
            [](const FaceNeutral& n) {
                Q_UNUSED(n);
            });

    if (w.exec() != QDialog::Accepted)
        return;

    temp.transparentBackground = w.transparentBackground();
    if (!temp.transparentBackground)
        temp.backgroundColor = w.backgroundColor();
    temp.mouseEnabled = w.mouseEnabled();
    temp.micEnabled = w.micEnabled();
    temp.webcamEnabled = w.webcamEnabled();
    const int camIdx = w.webcamIndex();
    if (camIdx >= 0 && camIdx < cams.size())
        temp.webcamDevice = QString::fromUtf8(cams[camIdx].id());
    const int micIdx = w.micIndex();
    if (micIdx >= 0 && micIdx < mics.size())
        temp.micDevice = QString::fromUtf8(mics[micIdx].id());

    // Se o modelo ainda não foi analisado (usuário pulou), analisa agora
    if (!temp.modelPath.isEmpty() && temp.mappings.isEmpty()
        && _view->model() && _view->model()->modelDirectory() != QFileInfo(temp.modelPath).absolutePath()) {
        const QString err = _view->loadModel(temp.modelPath);
        if (!err.isEmpty()) {
            QMessageBox::critical(this, QStringLiteral("Erro"), err);
            return;
        }
        AppConfig::buildAutoConfig(temp.modelPath, temp,
                                   _view->model()->parameterIds());
    }

    _settings = temp;
    applySettings();
    if (_settings.webcamEnabled)
        startWebcamIfEnabled();
    if (_view->model()) {
        _lblModel->setText(_view->model()->displayName());
        rebuildMappingTable();
        rebuildExpressionButtons();
    }
    persist();
}

// ---------------------------------------------------------------------------
// Aplicar configurações
// ---------------------------------------------------------------------------
void MainWindow::applySettings()
{
    _view->updateSettings(_settings);
    updateWindowFlags();

    _rebuildingUi = true;
    _chkMouse->setChecked(_settings.mouseEnabled);
    _chkMic->setChecked(_settings.micEnabled);
    _chkWebcam->setChecked(_settings.webcamEnabled);
    if (_chkDebugCam) {
        _chkDebugCam->setChecked(_settings.debugCam);
        if (_dbgBox)
            _dbgBox->setVisible(_settings.debugCam);
        _view->webcam()->setDebugCam(_settings.debugCam);
    }
    _chkInvertX->setChecked(_settings.invertHeadX);
    _chkInvertY->setChecked(_settings.invertHeadY);
    _chkBreath->setChecked(_settings.breathing);
    _sldSensHead->setValue(qRound(_settings.sensHead * 100));
    _sldSensEye->setValue(qRound(_settings.sensEye * 100));
    _sldSensPos->setValue(qRound(_settings.sensPos * 100));
    _sldSmoothing->setValue(qRound(_settings.smoothingScale * 100));
    _sldMicGain->setValue(qRound(_settings.micGain * 10));
    _sldMicThreshold->setValue(qRound(_settings.micThreshold * 1000));
    _bgSolid->setChecked(!_settings.transparentBackground);
    _bgTransparent->setChecked(_settings.transparentBackground);
    _chkFrameless->setChecked(_settings.frameless);
    _chkAlwaysOnTop->setChecked(_settings.alwaysOnTop);
    _rebuildingUi = false;

    // Desligar a webcam em qualquer caminho (inclusive pelo assistente)
    // encerra o daemon e libera o dispositivo na hora.
    if (!_settings.webcamEnabled && _view->webcam()->active())
        _view->webcam()->stop();

    _persistDebounce.start();
}

void MainWindow::updateWindowFlags()
{
    Qt::WindowFlags flags = Qt::Window;
    if (_settings.frameless)
        flags |= Qt::FramelessWindowHint;
    if (_settings.alwaysOnTop)
        flags |= Qt::WindowStaysOnTopHint;

    const QRect g = geometry();
    const bool changed = (windowFlags() != flags)
                         || (testAttribute(Qt::WA_TranslucentBackground)
                             != _settings.transparentBackground);
    if (!changed)
        return;
    hide();
    setWindowFlags(flags);
    setAttribute(Qt::WA_TranslucentBackground, _settings.transparentBackground);
    setGeometry(g);
    show();
}

void MainWindow::startWebcamIfEnabled()
{
    if (!_settings.webcamEnabled) {
        _view->webcam()->stop();
        return;
    }

    // Já está ativo nesta mesma câmera: não reinicia (evita piscar o LED e
    // perder a calibração a cada mudança de outro ajuste).
    if (_view->webcam()->active()
        && _view->webcam()->cameraIndex() == _settings.webcamIndex)
        return;

    // O OpenSeeFace lê a câmera direto pelo v4l2 (/dev/videoN), então usamos
    // o índice escolhido na UI em vez do índice do Qt (que no Wayland/PipeWire
    // pode não bater com o dispositivo real).
    if (!QFileInfo::exists(QStringLiteral("/dev/video%1").arg(_settings.webcamIndex))) {
        _lblCalibStatus->setText(QStringLiteral(
            "Câmera /dev/video%1 não encontrada. Selecione outra na aba Rastreio.")
            .arg(_settings.webcamIndex));
        return;
    }
    const QString model = faceModelPath();
    // O MediaPipe precisa do modelo; o OpenSeeFace não. Se o script for o do
    // OpenSeeFace, o modelo ausente não impede.
    if (model.isEmpty()) {
        _lblCalibStatus->setText(QStringLiteral(
            "Modelo de rastreio facial não encontrado.\nRode ./setup.sh para baixá-lo."));
        return;
    }
    _lblCalibStatus->setText(QStringLiteral("Iniciando rastreio facial..."));
    _view->webcam()->setDebugCam(_settings.debugCam);
    _view->webcam()->start(pythonExecutable(), trackerScriptPath(), model,
                           _settings.webcamIndex,
                           _settings.webcamWidth.toInt(),
                           _settings.webcamHeight.toInt());
}

void MainWindow::persist()
{
    _view->updateSettings(_settings);
    AppConfig::save(_settings);
}

void MainWindow::persistView(float zoom, float panX, float panY)
{
    _settings.zoom = zoom;
    _settings.panX = panX;
    _settings.panY = panY;
    _persistDebounce.start();
}

void MainWindow::closeEvent(QCloseEvent* e)
{
    _view->webcam()->stop();
    _view->mic()->stop();
    persist();
    QMainWindow::closeEvent(e);
}

// ---------------------------------------------------------------------------
// Dispositivos / caminhos
// ---------------------------------------------------------------------------
void MainWindow::refreshWebcamDevices()
{
    _rebuildingUi = true;
    _comboCam->clear();
    // Lista dispositivos v4l2 (/dev/video0, video1, ...) — é o que o
    // rastreador usa diretamente. Mostra também o nome (sysfs) para o usuário
    // distinguir webcam real de câmera virtual (OBS/Iriun).
    int selected = -1;
    for (int i = 0; i < 16; ++i) {
        const QString dev = QStringLiteral("/dev/video%1").arg(i);
        if (!QFileInfo::exists(dev))
            continue;

        QString label = dev;
        QFile nameFile(QStringLiteral("/sys/class/video4linux/video%1/name").arg(i));
        if (nameFile.open(QIODevice::ReadOnly)) {
            const QString name = QString::fromUtf8(nameFile.readAll()).trimmed();
            if (!name.isEmpty())
                label = QStringLiteral("%1 — %2").arg(dev, name);
        }

        _comboCam->addItem(label, i);
        if (i == _settings.webcamIndex)
            selected = _comboCam->count() - 1;
    }
    if (_comboCam->count() == 0) {
        _comboCam->addItem(QStringLiteral("(nenhuma /dev/video*)"), 0);
    } else if (selected < 0) {
        // O índice salvo não existe mais (ex.: câmera virtual removida):
        // seleciona o primeiro dispositivo disponível.
        selected = 0;
        _settings.webcamIndex = _comboCam->itemData(0).toInt();
    }
    if (selected >= 0)
        _comboCam->setCurrentIndex(selected);
    _rebuildingUi = false;
}

void MainWindow::refreshMicDevices()
{
    _rebuildingUi = true;
    _comboMic->clear();
    for (const QAudioDevice& m : QMediaDevices::audioInputs())
        _comboMic->addItem(m.description());
    const auto mics = QMediaDevices::audioInputs();
    for (int i = 0; i < mics.size(); ++i) {
        if (QString::fromUtf8(mics[i].id()) == _settings.micDevice.toUtf8()) {
            _comboMic->setCurrentIndex(i);
            break;
        }
    }
    _rebuildingUi = false;
}

void MainWindow::refreshScreenDevices()
{
    if (!_comboScreen)
        return;
    _rebuildingUi = true;
    _comboScreen->clear();
    _comboScreen->addItem(QStringLiteral("Automático (tela da janela)"));
    const auto screens = QGuiApplication::screens();
    for (const QScreen* s : screens)
        _comboScreen->addItem(QStringLiteral("%1 — %2x%3")
                                  .arg(s->name())
                                  .arg(s->geometry().width())
                                  .arg(s->geometry().height()));
    int sel = 0;
    for (int i = 0; i < screens.size(); ++i) {
        if (screens[i]->name() == _settings.mouseScreen) {
            sel = i + 1;
            break;
        }
    }
    _comboScreen->setCurrentIndex(sel);
    _rebuildingUi = false;
}

QString MainWindow::pythonExecutable() const
{
    const QByteArray env = qgetenv("GUMM_PYTHON");
    if (!env.isEmpty())
        return QString::fromUtf8(env);

    // Venv do projeto (criado pelo setup.sh) tem prioridade — evita o
    // bloqueio PEP 668 das distros (Arch/Fedora/Debian).
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList venvs = {
        appDir + QStringLiteral("/../tools/venv/bin/python"),
        appDir + QStringLiteral("/tools/venv/bin/python"),
        QDir::current().filePath(QStringLiteral("tools/venv/bin/python")),
        // OpenSeeFace instalado pelo usuário (ex.: ~/OpenSeeFace/env).
        QDir::home().filePath(QStringLiteral("OpenSeeFace/env/bin/python")),
        QDir::home().filePath(QStringLiteral("OpenSeeFace/venv/bin/python")),
    };
    for (const QString& v : venvs) {
        if (QFileInfo::exists(v))
            return QFileInfo(v).absoluteFilePath();
    }
    return QStringLiteral("python3");
}

QString MainWindow::trackerScriptPath() const
{
    const QString appDir = QCoreApplication::applicationDirPath();

    // O MediaPipe é o padrão: entrega yaw/pitch/roll corretos e blendshapes
    // (piscada, boca, sobrancelha) de forma estável mesmo com OpenCV novo.
    // Quem preferir o OpenSeeFace (mesmo rastreador do VTube Studio) pode
    // rodar com GUMM_TRACKER=openseeface.
    QStringList names = { QStringLiteral("tracker_daemon.py") };

    QStringList candidates;
    for (const QString& name : names) {
        candidates << appDir + QStringLiteral("/tools/") + name
                   << QDir::current().filePath(QStringLiteral("tools/") + name);
    }
    for (const QString& c : candidates) {
        if (QFileInfo::exists(c))
            return QFileInfo(c).absoluteFilePath();
    }
    return QDir::current().filePath(QStringLiteral("tools/tracker_daemon.py"));
}

QString MainWindow::faceModelPath() const
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir + QStringLiteral("/tools/models/face_landmarker.task"),
        QStringLiteral("~/.local/share/opentubestudio/face_landmarker.task"),
        QDir::home().filePath(QStringLiteral(".local/share/opentubestudio/face_landmarker.task")),
    };
    for (const QString& c : candidates) {
        const QString expanded = c.startsWith(QLatin1Char('~'))
                                     ? QDir::home().filePath(c.mid(2))
                                     : c;
        if (QFileInfo::exists(expanded))
            return expanded;
    }
    return QString();
}
