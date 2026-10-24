#pragma once
/*
 * AppConfig — configurações do GummVTuber.
 *
 * Guarda tudo em ~/.config/GummVTuber/GummVTuber.conf (via QSettings):
 * modelo, fundo (verde/transparente/cor), opções de OBS, rastreio e o
 * mapeamento de parâmetros (no formato do VTube Studio: entrada -> parâmetro
 * Live2D com faixas de entrada/saída e suavização).
 */
#include <QString>
#include <QStringList>
#include <QVector>
#include <QColor>
#include <QJsonObject>

// ---------------------------------------------------------------------------
// Canais: valores normalizados produzidos pelos rastreadores (mouse, micro,
// webcam) e consumidos pelo mapeamento de parâmetros.
// ---------------------------------------------------------------------------
struct Channels
{
    float headX     = 0.0f;   // -1..1  (para frente/tras... na pratica esq/dir)
    float headY     = 0.0f;   // -1..1  (cima/baixo)
    float headZ     = 0.0f;   // -1..1  (inclinação)
    float bodyX     = 0.0f;   // -1..1
    float bodyY     = 0.0f;
    float bodyZ     = 0.0f;
    float eyeOpenL  = 1.0f;   // 0..1
    float eyeOpenR  = 1.0f;   // 0..1
    float eyeBallX  = 0.0f;   // -1..1
    float eyeBallY  = 0.0f;   // -1..1
    float brows     = 0.5f;   // 0..1 (0.5 = neutro)
    float mouthOpen = 0.0f;   // 0..1
    float mouthSmile= 0.5f;   // 0..1 (0.5 = neutro)
    float breath    = 0.0f;   // 0..1 (procedural)

    // Escala nominal de cada entrada (ex.: FaceAngleX vai de -30..30).
    static float inputScale(const QString& input);
    // Valor do canal na unidade de entrada do mapeamento.
    float valueFor(const QString& input) const;
    // Nome de todas as entradas disponíveis (para a UI).
    static QStringList allInputs();
};

// ---------------------------------------------------------------------------
// Um vínculo entrada -> parâmetro (igual ao "Parameter Settings" do VTS).
// ---------------------------------------------------------------------------
struct ParamMapping
{
    QString input;        // ex.: "FaceAngleX"
    QString output;       // ex.: "ParamAngleX"
    float   inMin    = 0.0f;
    float   inMax    = 1.0f;
    float   outMin   = 0.0f;
    float   outMax   = 1.0f;
    float   smoothing = 0.0f;  // 0 = sem suavização; quanto maior, mais lento
    bool    clampOutput = true;
    bool    enabled   = true;
};

// ---------------------------------------------------------------------------
// Expressão (arquivo .exp3.json)
// ---------------------------------------------------------------------------
struct ExpressionInfo
{
    QString name;    // nome exibido
    QString file;    // caminho relativo ao diretório do modelo
};

// ---------------------------------------------------------------------------
// Configurações completas
// ---------------------------------------------------------------------------
struct AppSettings
{
    QString modelPath;

    // Fundo
    bool    transparentBackground = false;
    QColor  backgroundColor       = QColor(0, 200, 60); // verde "OBS friendly"

    // Janela / OBS
    bool    frameless   = false;
    bool    alwaysOnTop = false;

    // Rastreio
    bool    mouseEnabled  = true;
    QString mouseScreen;                  // nome da tela p/ tracking ("" = automático)
    bool    micEnabled    = true;
    bool    webcamEnabled = false;
    QString webcamDevice;                 // uniqueId da câmera (Qt)
    int     webcamIndex   = 0;            // índice v4l2 (/dev/videoN) p/ OpenSeeFace
    QString webcamWidth   = QStringLiteral("1280");
    QString webcamHeight  = QStringLiteral("720");
    QString micDevice;                    // deviceId do microfone
    float   micThreshold = 0.025f;
    float   micGain      = 6.0f;
    bool    invertHeadX  = false;
    bool    invertHeadY  = true;          // mouse: Y invertido por padrão (sobe olha pra cima)
    bool    invertHeadZ  = false;
    float   sensHead     = 1.0f;
    float   sensEye      = 1.0f;
    bool    breathing    = true;

    // Calibração da webcam (offsets neutros)
    float calYaw = 0, calPitch = 0, calRoll = 0;
    float calEyeX = 0, calEyeY = 0, calMouth = 0, calBrow = 0;
    bool  calibrated = false;

    // Visão (zoom/pan)
    float zoom = 1.0f;
    float panX = 0.0f;
    float panY = 0.0f;

    // Mapeamentos e expressões
    QVector<ParamMapping>  mappings;
    QVector<ExpressionInfo> expressions; // ordem = ordem dos atalhos Ctrl+1..9

    QJsonObject toJson() const;
    static AppSettings fromJson(const QJsonObject& o);
};

// ---------------------------------------------------------------------------
// Persistência + utilitários
// ---------------------------------------------------------------------------
class AppConfig
{
public:
    static AppSettings load();
    static void save(const AppSettings& s);

    // Lê/escrive só a parte de "view" separadamente (zoom/pan mudam a cada
    // arrasto e não queremos regravar o arquivo todo o tempo).
    static void saveView(float zoom, float panX, float panY);

    // Gera mapeamento padrão para um modelo. Se existir um arquivo .vtube.json
    // (configuração do VTube Studio) no diretório do modelo, importa dele —
    // aí as expressões e inversões ficam iguaizinhas ao VTS.
    // Retorna um resumo legível do que foi feito.
    static QString buildAutoConfig(const QString& model3Path,
                                   AppSettings& settings,
                                   const QStringList& modelParamIds);
};
