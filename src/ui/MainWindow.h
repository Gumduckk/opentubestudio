#pragma once
/*
 * MainWindow — janela principal do OpentubeStudio.
 * Visor do modelo + painel de configuração (rastreio, parâmetros,
 * fundo/OBS, modelo) + assistente de configuração automática.
 */
#include <QMainWindow>
#include <QTimer>
#include "config/AppConfig.h"

class ModelView;
class QTabWidget;
class QComboBox;
class QCheckBox;
class QSlider;
class QPushButton;
class QLabel;
class QTableWidget;
class QRadioButton;
class SetupWizard;
class QToolButton;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // Abre um modelo pelo caminho do .model3.json (usado por --model e atalhos).
    // Se o modelo ainda não tiver mapeamento salvo, roda a configuração automática.
    void openModelPath(const QString& model3Path);

protected:
    void closeEvent(QCloseEvent* e) override;

private slots:
    void openModelDialog();
    void runWizard();
    void autoConfigure();
    void calibrateWebcam();
    void onExpressionClicked();
    void persistView(float zoom, float panX, float panY);

private:
    void buildUi();
    void buildTrackingTab();
    void buildMappingTab();
    void buildBackgroundTab();
    void buildModelTab();
    void rebuildMappingTable();
    void rebuildExpressionButtons();
    void loadModel(const QString& path);
    void applySettings();       // aplica _settings na view/janela
    void persist();             // salva no disco
    void updateWindowFlags();
    void startWebcamIfEnabled();
    void refreshWebcamDevices();
    void refreshMicDevices();
    void refreshScreenDevices();
    void fillModelParamCombo(QComboBox* combo);

    QString pythonExecutable() const;
    QString trackerScriptPath() const;
    QString faceModelPath() const;

    AppSettings _settings;
    ModelView*  _view = nullptr;
    QDockWidget* _dock = nullptr;
    QTabWidget* _tabs = nullptr;
    bool _rebuildingUi = false;
    bool _wizardSuppressed = false; // --model passado: não abrir o assistente
    // Estado do fundo antes de ligar o Modo OBS (para restaurar ao desligar).
    bool   _bgSavedOnObs = false;
    bool   _obsSavedTransparent = false;
    QColor _obsSavedColor = QColor(0, 200, 60);

    // Aba Rastreio
    QCheckBox* _chkMouse = nullptr;
    QCheckBox* _chkMic = nullptr;
    QCheckBox* _chkWebcam = nullptr;
    QCheckBox* _chkDebugCam = nullptr;
    QWidget*   _dbgBox = nullptr;        // painel com a imagem do rastreio
    QLabel*    _dbgImg = nullptr;        // QLabel que mostra o frame anotado
    QComboBox* _comboCam = nullptr;
    QComboBox* _comboMic = nullptr;
    QComboBox* _comboScreen = nullptr;
    QSlider*   _sldSensHead = nullptr;
    QSlider*   _sldSensEye = nullptr;
    QSlider*   _sldSensPos = nullptr;
    QSlider*   _sldSmoothing = nullptr;
    QSlider*   _sldMicGain = nullptr;
    QSlider*   _sldMicThreshold = nullptr;
    QCheckBox* _chkInvertX = nullptr;
    QCheckBox* _chkInvertY = nullptr;
    QCheckBox* _chkBreath = nullptr;
    QLabel*    _lblCalibStatus = nullptr;

    // Aba Parâmetros
    QTableWidget* _mapTable = nullptr;

    // Aba Fundo/OBS
    QRadioButton* _bgSolid = nullptr;
    QRadioButton* _bgTransparent = nullptr;
    QPushButton*  _bgColorBtn = nullptr;
    QCheckBox*    _chkFrameless = nullptr;
    QCheckBox*    _chkAlwaysOnTop = nullptr;
    QComboBox*    _comboResolution = nullptr;
    QToolButton*  _bgQuickButton = nullptr;

    // Aba Modelo
    QLabel*        _lblModelInfo = nullptr;
    QWidget*       _exprContainer = nullptr;

    // Barra de status
    QLabel* _lblModel = nullptr;
    QLabel* _lblFps = nullptr;
    QLabel* _lblTracking = nullptr;

    QTimer _persistDebounce;
};
