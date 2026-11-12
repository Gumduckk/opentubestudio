#include "AppConfig.h"

#include <QSettings>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFileInfo>
#include <QDir>
#include <QtMath>

// ---------------------------------------------------------------------------
// Channels
// ---------------------------------------------------------------------------
float Channels::inputScale(const QString& input)
{
    if (input == QLatin1String("FaceAngleX")) return 30.0f;
    if (input == QLatin1String("FaceAngleY")) return 20.0f;
    if (input == QLatin1String("FaceAngleZ")) return 30.0f;
    if (input == QLatin1String("BodyAngleX")) return 30.0f;
    if (input == QLatin1String("BodyAngleY")) return 30.0f;
    if (input == QLatin1String("BodyAngleZ")) return 30.0f;
    return 1.0f;
}

float Channels::valueFor(const QString& input) const
{
    if (input == QLatin1String("FaceAngleX"))  return headX * inputScale(input);
    if (input == QLatin1String("FaceAngleY"))  return headY * inputScale(input);
    if (input == QLatin1String("FaceAngleZ"))  return headZ * inputScale(input);
    if (input == QLatin1String("BodyAngleX"))  return bodyX * inputScale(input);
    if (input == QLatin1String("BodyAngleY"))  return bodyY * inputScale(input);
    if (input == QLatin1String("BodyAngleZ"))  return bodyZ * inputScale(input);
    if (input == QLatin1String("EyeOpenLeft"))  return eyeOpenL;
    if (input == QLatin1String("EyeOpenRight")) return eyeOpenR;
    if (input == QLatin1String("EyeSmileL"))    return eyeSmileL;
    if (input == QLatin1String("EyeSmileR"))    return eyeSmileR;
    if (input == QLatin1String("TongueOut"))    return tongueOut;
    // Alias histórico do .vtube.json da Akari: MouthX = deslocamento da boca
    if (input == QLatin1String("MouthX"))       return mouthX;
    if (input == QLatin1String("MouthY"))       return mouthY;
    if (input == QLatin1String("BrowLeftY"))    return browL;
    if (input == QLatin1String("BrowRightY"))   return browR;
    if (input == QLatin1String("FacePositionX")) return facePosX;
    if (input == QLatin1String("FacePositionY")) return facePosY;
    if (input == QLatin1String("FacePositionZ")) return facePosZ;
    if (input == QLatin1String("CheekSquint"))   return cheekSquint;
    if (input == QLatin1String("NoseSneer"))     return noseSneer;
    if (input == QLatin1String("MouthDimple"))   return mouthDimple;
    if (input == QLatin1String("MouthPress"))    return mouthPress;
    if (input == QLatin1String("EyeRightX"))    return eyeBallX;
    if (input == QLatin1String("EyeRightY"))    return eyeBallY;
    if (input == QLatin1String("Brows"))        return brows;
    if (input == QLatin1String("MouthOpen"))    return mouthOpen;
    if (input == QLatin1String("MouthSmile"))   return mouthSmile;
    if (input == QLatin1String("MouthPucker"))  return mouthPucker;
    if (input == QLatin1String("MouthFunnel"))  return mouthFunnel;
    if (input == QLatin1String("CheekPuff"))    return cheekPuff;
    if (input == QLatin1String("BrowDown"))     return browDown;
    if (input == QLatin1String("JawSide"))      return jawSide;
    if (input == QLatin1String("Frown"))        return frown;
    if (input == QLatin1String("MouthStretch")) return mouthStretch;
    if (input == QLatin1String("Breathing"))    return breath;
    return 0.0f;
}

QStringList Channels::allInputs()
{
    return { QStringLiteral("FaceAngleX"), QStringLiteral("FaceAngleY"),
             QStringLiteral("FaceAngleZ"), QStringLiteral("BodyAngleX"),
             QStringLiteral("BodyAngleY"), QStringLiteral("BodyAngleZ"),
             QStringLiteral("EyeOpenLeft"), QStringLiteral("EyeOpenRight"),
             QStringLiteral("EyeSmileL"), QStringLiteral("EyeSmileR"),
             QStringLiteral("EyeRightX"), QStringLiteral("EyeRightY"),
             QStringLiteral("Brows"), QStringLiteral("MouthOpen"),
             QStringLiteral("MouthSmile"), QStringLiteral("MouthPucker"),
             QStringLiteral("MouthFunnel"), QStringLiteral("CheekPuff"),
             QStringLiteral("BrowDown"), QStringLiteral("JawSide"),
             QStringLiteral("Frown"), QStringLiteral("MouthStretch"),
             QStringLiteral("TongueOut"), QStringLiteral("MouthX"),
             QStringLiteral("FacePositionX"), QStringLiteral("FacePositionY"),
             QStringLiteral("FacePositionZ"),
             QStringLiteral("CheekSquint"), QStringLiteral("NoseSneer"),
             QStringLiteral("MouthDimple"), QStringLiteral("MouthPress"),
             QStringLiteral("MouthY"),
             QStringLiteral("BrowLeftY"), QStringLiteral("BrowRightY"),
             QStringLiteral("Breathing") };
}

// ---------------------------------------------------------------------------
// Serialização
// ---------------------------------------------------------------------------
static QJsonObject mappingToJson(const ParamMapping& m)
{
    QJsonObject o;
    o[QStringLiteral("input")]  = m.input;
    o[QStringLiteral("output")] = m.output;
    o[QStringLiteral("inMin")]  = m.inMin;
    o[QStringLiteral("inMax")]  = m.inMax;
    o[QStringLiteral("outMin")] = m.outMin;
    o[QStringLiteral("outMax")] = m.outMax;
    o[QStringLiteral("smoothing")] = m.smoothing;
    o[QStringLiteral("clampOutput")] = m.clampOutput;
    o[QStringLiteral("enabled")] = m.enabled;
    return o;
}

static ParamMapping mappingFromJson(const QJsonObject& o)
{
    ParamMapping m;
    m.input  = o[QStringLiteral("input")].toString();
    m.output = o[QStringLiteral("output")].toString();
    m.inMin  = static_cast<float>(o[QStringLiteral("inMin")].toDouble());
    m.inMax  = static_cast<float>(o[QStringLiteral("inMax")].toDouble());
    m.outMin = static_cast<float>(o[QStringLiteral("outMin")].toDouble());
    m.outMax = static_cast<float>(o[QStringLiteral("outMax")].toDouble());
    m.smoothing = static_cast<float>(o[QStringLiteral("smoothing")].toDouble());
    m.clampOutput = o[QStringLiteral("clampOutput")].toBool(true);
    m.enabled = o[QStringLiteral("enabled")].toBool(true);
    return m;
}

QJsonObject AppSettings::toJson() const
{
    QJsonObject o;
    o[QStringLiteral("modelPath")] = modelPath;
    o[QStringLiteral("transparentBackground")] = transparentBackground;
    o[QStringLiteral("backgroundColor")] = backgroundColor.name(QColor::HexRgb);
    o[QStringLiteral("frameless")] = frameless;
    o[QStringLiteral("alwaysOnTop")] = alwaysOnTop;

    QJsonObject tr;
    tr[QStringLiteral("mouse")] = mouseEnabled;
    tr[QStringLiteral("debugCam")] = debugCam;
    tr[QStringLiteral("mouseScreen")] = mouseScreen;
    tr[QStringLiteral("mic")] = micEnabled;
    tr[QStringLiteral("webcam")] = webcamEnabled;
    tr[QStringLiteral("webcamDevice")] = webcamDevice;
    tr[QStringLiteral("webcamIndex")] = webcamIndex;
    tr[QStringLiteral("webcamWidth")] = webcamWidth;
    tr[QStringLiteral("webcamHeight")] = webcamHeight;
    tr[QStringLiteral("micDevice")] = micDevice;
    tr[QStringLiteral("micThreshold")] = micThreshold;
    tr[QStringLiteral("micGain")] = micGain;
    tr[QStringLiteral("invertHeadX")] = invertHeadX;
    tr[QStringLiteral("invertHeadY")] = invertHeadY;
    tr[QStringLiteral("invertHeadZ")] = invertHeadZ;
    tr[QStringLiteral("sensHead")] = sensHead;
    tr[QStringLiteral("sensEye")] = sensEye;
    tr[QStringLiteral("oneEuroMinCutoff")] = oneEuroMinCutoff;
    tr[QStringLiteral("oneEuroBeta")] = oneEuroBeta;
    tr[QStringLiteral("sensPos")] = sensPos;
    tr[QStringLiteral("smoothingScale")] = smoothingScale;
    tr[QStringLiteral("breathing")] = breathing;
    o[QStringLiteral("tracking")] = tr;

    QJsonObject cal;
    cal[QStringLiteral("yaw")] = calYaw;
    cal[QStringLiteral("pitch")] = calPitch;
    cal[QStringLiteral("roll")] = calRoll;
    cal[QStringLiteral("eyeX")] = calEyeX;
    cal[QStringLiteral("eyeY")] = calEyeY;
    cal[QStringLiteral("mouth")] = calMouth;
    cal[QStringLiteral("brow")] = calBrow;
    cal[QStringLiteral("done")] = calibrated;
    o[QStringLiteral("calibration")] = cal;

    QJsonObject v;
    v[QStringLiteral("zoom")] = zoom;
    v[QStringLiteral("panX")] = panX;
    v[QStringLiteral("panY")] = panY;
    o[QStringLiteral("view")] = v;

    QJsonArray map;
    for (const ParamMapping& m : mappings)
        map.append(mappingToJson(m));
    o[QStringLiteral("mappings")] = map;

    QJsonArray exp;
    for (const ExpressionInfo& e : expressions) {
        QJsonObject eo;
        eo[QStringLiteral("name")] = e.name;
        eo[QStringLiteral("file")] = e.file;
        exp.append(eo);
    }
    o[QStringLiteral("expressions")] = exp;
    return o;
}

AppSettings AppSettings::fromJson(const QJsonObject& o)
{
    AppSettings s;
    s.modelPath = o[QStringLiteral("modelPath")].toString();
    s.transparentBackground = o[QStringLiteral("transparentBackground")].toBool(false);
    s.backgroundColor = QColor(o[QStringLiteral("backgroundColor")].toString(QStringLiteral("#00c83c")));
    s.frameless = o[QStringLiteral("frameless")].toBool(false);
    s.alwaysOnTop = o[QStringLiteral("alwaysOnTop")].toBool(false);

    const QJsonObject tr = o[QStringLiteral("tracking")].toObject();
    s.mouseEnabled = tr[QStringLiteral("mouse")].toBool(true);
    s.debugCam     = tr[QStringLiteral("debugCam")].toBool(false);
    s.mouseScreen  = tr[QStringLiteral("mouseScreen")].toString();
    s.micEnabled   = tr[QStringLiteral("mic")].toBool(true);
    s.webcamEnabled= tr[QStringLiteral("webcam")].toBool(false);
    s.webcamDevice = tr[QStringLiteral("webcamDevice")].toString();
    s.webcamIndex  = tr[QStringLiteral("webcamIndex")].toInt(0);
    s.webcamWidth  = tr[QStringLiteral("webcamWidth")].toString(QStringLiteral("1280"));
    s.webcamHeight = tr[QStringLiteral("webcamHeight")].toString(QStringLiteral("720"));
    s.micDevice    = tr[QStringLiteral("micDevice")].toString();
    s.micThreshold = static_cast<float>(tr[QStringLiteral("micThreshold")].toDouble(0.025));
    s.micGain      = static_cast<float>(tr[QStringLiteral("micGain")].toDouble(6.0));
    s.invertHeadX  = tr[QStringLiteral("invertHeadX")].toBool(false);
    s.invertHeadY  = tr[QStringLiteral("invertHeadY")].toBool(true);
    s.invertHeadZ  = tr[QStringLiteral("invertHeadZ")].toBool(false);
    s.sensHead     = static_cast<float>(tr[QStringLiteral("sensHead")].toDouble(1.0));
    s.sensEye      = static_cast<float>(tr[QStringLiteral("sensEye")].toDouble(1.0));
    s.oneEuroMinCutoff = static_cast<float>(tr[QStringLiteral("oneEuroMinCutoff")].toDouble(1.0));
    s.oneEuroBeta      = static_cast<float>(tr[QStringLiteral("oneEuroBeta")].toDouble(0.5));
    s.sensPos          = static_cast<float>(tr[QStringLiteral("sensPos")].toDouble(1.0));
    s.smoothingScale   = static_cast<float>(tr[QStringLiteral("smoothingScale")].toDouble(0.8));
    s.breathing    = tr[QStringLiteral("breathing")].toBool(true);

    const QJsonObject cal = o[QStringLiteral("calibration")].toObject();
    s.calYaw   = static_cast<float>(cal[QStringLiteral("yaw")].toDouble());
    s.calPitch = static_cast<float>(cal[QStringLiteral("pitch")].toDouble());
    s.calRoll  = static_cast<float>(cal[QStringLiteral("roll")].toDouble());
    s.calEyeX  = static_cast<float>(cal[QStringLiteral("eyeX")].toDouble());
    s.calEyeY  = static_cast<float>(cal[QStringLiteral("eyeY")].toDouble());
    s.calMouth = static_cast<float>(cal[QStringLiteral("mouth")].toDouble());
    s.calBrow  = static_cast<float>(cal[QStringLiteral("brow")].toDouble());
    s.calibrated = cal[QStringLiteral("done")].toBool(false);

    const QJsonObject v = o[QStringLiteral("view")].toObject();
    s.zoom = static_cast<float>(v[QStringLiteral("zoom")].toDouble(1.0));
    s.panX = static_cast<float>(v[QStringLiteral("panX")].toDouble(0.0));
    s.panY = static_cast<float>(v[QStringLiteral("panY")].toDouble(0.0));

    for (const QJsonValue& mv : o[QStringLiteral("mappings")].toArray())
        s.mappings.append(mappingFromJson(mv.toObject()));

    for (const QJsonValue& ev : o[QStringLiteral("expressions")].toArray()) {
        const QJsonObject eo = ev.toObject();
        ExpressionInfo e;
        e.name = eo[QStringLiteral("name")].toString();
        e.file = eo[QStringLiteral("file")].toString();
        s.expressions.append(e);
    }
    return s;
}

// ---------------------------------------------------------------------------
// Persistência
// ---------------------------------------------------------------------------
static QSettings makeSettings()
{
    return QSettings(QStringLiteral("GummVTuber"), QStringLiteral("GummVTuber"));
}

AppSettings AppConfig::load()
{
    QSettings st = makeSettings();
    const QByteArray blob = st.value(QStringLiteral("config")).toByteArray();
    if (blob.isEmpty())
        return AppSettings();
    const QJsonObject o = QJsonDocument::fromJson(blob).object();
    return AppSettings::fromJson(o);
}

void AppConfig::save(const AppSettings& s)
{
    QSettings st = makeSettings();
    st.setValue(QStringLiteral("config"),
                QJsonDocument(s.toJson()).toJson(QJsonDocument::Compact));
}

void AppConfig::saveView(float zoom, float panX, float panY)
{
    AppSettings s = load();
    s.zoom = zoom;
    s.panX = panX;
    s.panY = panY;
    save(s);
}

// ---------------------------------------------------------------------------
// Auto-configuração
// ---------------------------------------------------------------------------
static ParamMapping makeMapping(const QString& input, const QString& output,
                                float inMin, float inMax,
                                float outMin, float outMax, float smoothing)
{
    ParamMapping m;
    m.input = input;
    m.output = output;
    m.inMin = inMin; m.inMax = inMax;
    m.outMin = outMin; m.outMax = outMax;
    m.smoothing = smoothing;
    m.clampOutput = true;
    return m;
}

// Mapeamento padrão (estilo VTube Studio) — só usa parâmetros que existem
// no modelo.
static QVector<ParamMapping> defaultMappings(const QSet<QString>& have)
{
    QVector<ParamMapping> v;
    auto add = [&](const ParamMapping& m) {
        if (have.contains(m.output)) v.append(m);
    };
    add(makeMapping(QStringLiteral("FaceAngleX"), QStringLiteral("ParamAngleX"), -30, 30, -30, 30, 15));
    add(makeMapping(QStringLiteral("FaceAngleY"), QStringLiteral("ParamAngleY"), -20, 20, -30, 30, 15));
    add(makeMapping(QStringLiteral("FaceAngleZ"), QStringLiteral("ParamAngleZ"), -30, 30, -30, 30, 30));
    add(makeMapping(QStringLiteral("FaceAngleX"), QStringLiteral("ParamBodyAngleX"), -30, 30, 10, -10, 20));
    add(makeMapping(QStringLiteral("FaceAngleY"), QStringLiteral("ParamBodyAngleY"), -20, 20, -10, 10, 20));
    add(makeMapping(QStringLiteral("FaceAngleZ"), QStringLiteral("ParamBodyAngleZ"), -30, 30, -10, 10, 20));
    add(makeMapping(QStringLiteral("EyeOpenLeft"),  QStringLiteral("ParamEyeLOpen"), 0, 1, 0, 1, 10));
    add(makeMapping(QStringLiteral("EyeOpenRight"), QStringLiteral("ParamEyeROpen"), 0, 1, 0, 1, 10));
    add(makeMapping(QStringLiteral("EyeSmileL"),    QStringLiteral("ParamEyeLSmile"), 0, 1, 0, 1, 10));
    add(makeMapping(QStringLiteral("EyeSmileR"),    QStringLiteral("ParamEyeRSmile"), 0, 1, 0, 1, 10));
    // Sobrancelha franzida -> ângulos internos (inclinado pra baixo no meio = raiva)
    add(makeMapping(QStringLiteral("BrowDown"),     QStringLiteral("ParamBrowLAngle"), 0, 1, 0, -1, 10));
    add(makeMapping(QStringLiteral("BrowDown"),     QStringLiteral("ParamBrowRAngle"), 0, 1, 0, 1, 10));
    // Mandíbula lateral -> posição X da boca (só existe em modelos como a Hiyori)
    add(makeMapping(QStringLiteral("JawSide"),      QStringLiteral("ParamMouthX"), -1, 1, -1, 1, 10));
    add(makeMapping(QStringLiteral("EyeRightX"), QStringLiteral("ParamEyeBallX"), -1, 1, -1, 1, 8));
    add(makeMapping(QStringLiteral("EyeRightY"), QStringLiteral("ParamEyeBallY"), -1, 1, 1, -1, 8));
    add(makeMapping(QStringLiteral("Brows"), QStringLiteral("ParamBrowLY"), 0, 1, 1, -1, 10));
    add(makeMapping(QStringLiteral("Brows"), QStringLiteral("ParamBrowRY"), 0, 1, 1, -1, 10));
    add(makeMapping(QStringLiteral("Brows"), QStringLiteral("ParamBrowLForm"), 0, 1, -1, 1, 15));
    add(makeMapping(QStringLiteral("Brows"), QStringLiteral("ParamBrowRForm"), 0, 1, -1, 1, 15));
    add(makeMapping(QStringLiteral("MouthOpen"),  QStringLiteral("ParamMouthOpenY"), 0, 1, 0, 1.3f, 5));
    add(makeMapping(QStringLiteral("MouthSmile"), QStringLiteral("ParamMouthForm"), 0, 1, -1, 1, 10));
    // Expressões extras: só quando o modelo realmente tiver o parâmetro
    add(makeMapping(QStringLiteral("MouthPucker"), QStringLiteral("ParamMouthPucker"), 0, 1, 0, 1, 10));
    add(makeMapping(QStringLiteral("MouthFunnel"), QStringLiteral("ParamMouthFunnel"), 0, 1, 0, 1, 10));
    add(makeMapping(QStringLiteral("CheekPuff"),   QStringLiteral("ParamCheekPuff"),   0, 1, 0, 1, 10));
    add(makeMapping(QStringLiteral("CheekPuff"),   QStringLiteral("ParamCheek"),       0, 1, 0, 1, 10));
    add(makeMapping(QStringLiteral("Breathing"),  QStringLiteral("ParamBreath"), 0, 1, 0, 1, 0));
    return v;
}

int AppConfig::ensureTrackingMappings(AppSettings& settings,
                                      const QStringList& modelParamIds)
{
    QSet<QString> have;
    for (const QString& id : modelParamIds)
        have.insert(id);
    QSet<QString> driven;
    for (const ParamMapping& m : settings.mappings)
        driven.insert(m.output);

    int added = 0;
    auto add = [&](const QString& input, const QString& output,
                   float inMin, float inMax,
                   float outMin, float outMax, float smoothing) {
        if (!have.contains(output))
            return;
        if (driven.contains(output))
            return; // já comandado por outro input; respeita o existente
        settings.mappings.append(makeMapping(input, output, inMin, inMax,
                                             outMin, outMax, smoothing));
        driven.insert(output);
        ++added;
    };

    add(QStringLiteral("EyeSmileL"), QStringLiteral("ParamEyeLSmile"), 0, 1, 0, 1, 10);
    add(QStringLiteral("EyeSmileR"), QStringLiteral("ParamEyeRSmile"), 0, 1, 0, 1, 10);
    add(QStringLiteral("BrowDown"),  QStringLiteral("ParamBrowLAngle"), 0, 1, 0, -1, 10);
    add(QStringLiteral("BrowDown"),  QStringLiteral("ParamBrowRAngle"), 0, 1, 0, 1, 10);
    add(QStringLiteral("JawSide"),   QStringLiteral("ParamMouthX"),    -1, 1, -1, 1, 10);
    add(QStringLiteral("CheekPuff"), QStringLiteral("ParamCheek"),      0, 1, 0, 1, 10);
    // Bochecha cora no sorriso (igual ao .vtube.json oficial da Gum) quando
    // NADA dirige ParamCheek ainda.
    add(QStringLiteral("MouthSmile"), QStringLiteral("ParamCheek"),     0, 1, 0, 1, 15);
    // Posição 3D do rosto (lean in/out) -> ParamPositionX/Y/Z. Range
    // conservador (±10, como os BodyAngle); ajuste na UI se o modelo
    // expuser outra convenção.
    add(QStringLiteral("FacePositionX"), QStringLiteral("ParamPositionX"), -1, 1, -10, 10, 20);
    add(QStringLiteral("FacePositionY"), QStringLiteral("ParamPositionY"), -1, 1, -10, 10, 20);
    add(QStringLiteral("FacePositionZ"), QStringLiteral("ParamPositionZ"), -1, 1, -10, 10, 20);
    // Extensões Akari (só ativam neste modelo — só ele tem esses parâmetros):
    add(QStringLiteral("NoseSneer"),   QStringLiteral("ParamAngrySign"),   0.15f, 0.75f, 0, 1, 20);
    add(QStringLiteral("CheekPuff"),   QStringLiteral("ParamShock"),       0.2f,  0.8f,  0, 1, 15);
    add(QStringLiteral("CheekSquint"), QStringLiteral("ParamEyeXDVisible"), 0.45f, 0.9f,  0, 1, 15);
    return added;
}

// Importa configuração do VTube Studio (.vtube.json), se existir.
static bool importVTubeJson(const QString& vtubePath, const QSet<QString>& have,
                            QVector<ParamMapping>& mappings,
                            QVector<ExpressionInfo>& expressions,
                            QString& summary)
{
    QFile f(vtubePath);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();

    // ---- Mapeamento de parâmetros (igual o VTS usa) ----
    const QJsonArray params = root[QStringLiteral("ParameterSettings")].toArray();
    int imported = 0;
    for (const QJsonValue& pv : params) {
        const QJsonObject p = pv.toObject();
        const QString outId = p[QStringLiteral("OutputLive2D")].toString();
        if (outId.isEmpty() || !have.contains(outId))
            continue; // parâmetro não existe mais neste modelo

        QString input = p[QStringLiteral("Input")].toString();
        if (input.isEmpty() && p[QStringLiteral("UseBreathing")].toBool())
            input = QStringLiteral("Breathing");
        if (input.isEmpty())
            continue;

        ParamMapping m;
        m.input = input;
        m.output = outId;
        m.inMin = static_cast<float>(p[QStringLiteral("InputRangeLower")].toDouble(0));
        m.inMax = static_cast<float>(p[QStringLiteral("InputRangeUpper")].toDouble(1));
        m.outMin = static_cast<float>(p[QStringLiteral("OutputRangeLower")].toDouble(0));
        m.outMax = static_cast<float>(p[QStringLiteral("OutputRangeUpper")].toDouble(1));
        m.smoothing = static_cast<float>(p[QStringLiteral("Smoothing")].toDouble(0));
        m.clampOutput = p[QStringLiteral("ClampOutput")].toBool(true);
        mappings.append(m);
        ++imported;
    }

    // Desativa vínculos cujos inputs não existem em nossas entradas
    // (ficariam mortos para sempre, mexendo com ninguém; melhor visível e
    // desmarcados na UI do que aparentar configurar de verdade).
    int disabled = 0;
    const QStringList known = Channels::allInputs();
    for (ParamMapping& mapping : mappings) {
        if (!known.contains(mapping.input)) {
            mapping.enabled = false;
            ++disabled;
        }
    }
    if (disabled > 0)
        summary += QStringLiteral("• %1 vínculos desativados (entradas não suportadas)\n")
                       .arg(disabled);

    // ---- Expressões (hotkeys do VTS, na ordem) ----
    const QJsonArray hotkeys = root[QStringLiteral("Hotkeys")].toArray();
    QSet<QString> seen;
    for (const QJsonValue& hv : hotkeys) {
        const QJsonObject h = hv.toObject();
        if (h[QStringLiteral("Action")].toString() != QLatin1String("ToggleExpression"))
            continue;
        const QString file = h[QStringLiteral("File")].toString();
        if (file.isEmpty() || seen.contains(file))
            continue;
        seen.insert(file);
        ExpressionInfo e;
        e.file = file;
        e.name = QFileInfo(file).completeBaseName();
        e.name.remove(QStringLiteral(".exp3"));
        expressions.append(e);
    }

    summary += QStringLiteral("• Importado do arquivo de configuração do VTube Studio\n");
    summary += QStringLiteral("  (%1 vínculos de parâmetros, %2 expressões com hotkeys)\n")
                   .arg(imported).arg(expressions.size());
    return true;
}

QString AppConfig::buildAutoConfig(const QString& model3Path, AppSettings& settings,
                                   const QStringList& modelParamIds)
{
    Q_UNUSED(model3Path);
    QString summary;
    QSet<QString> have;
    for (const QString& id : modelParamIds)
        have.insert(id);

    settings.mappings.clear();
    settings.expressions.clear();

    // Procura .vtube.json no diretório do modelo
    const QDir dir = QFileInfo(model3Path).absoluteDir();
    const QString base = QFileInfo(model3Path).completeBaseName(); // sem ".model3"
    QStringList vtubeCandidates;
    // nome "<base>.vtube.json", ou qualquer *.vtube.json do diretório
    vtubeCandidates << base + QStringLiteral(".vtube.json");
    const QStringList all = dir.entryList({ QStringLiteral("*.vtube.json") }, QDir::Files);
    for (const QString& n : all)
        vtubeCandidates << n;

    bool imported = false;
    for (const QString& cand : vtubeCandidates) {
        if (importVTubeJson(dir.filePath(cand), have, settings.mappings,
                            settings.expressions, summary)) {
            imported = true;
            break;
        }
    }

    if (!imported) {
        settings.mappings = defaultMappings(have);
        summary += QStringLiteral("• Mapeamento padrão criado (estilo VTube Studio)\n");
        summary += QStringLiteral("  (%1 vínculos de parâmetros)\n").arg(settings.mappings.size());
    }

    // Expressões: garante que todo *.exp3.json do diretório esteja listado
    // (mesmo sem hotkey no VTS).
    QSet<QString> haveExpFiles;
    for (const ExpressionInfo& e : settings.expressions)
        haveExpFiles.insert(e.file);
    const QStringList exps = dir.entryList({ QStringLiteral("*.exp3.json") }, QDir::Files, QDir::Name);
    int added = 0;
    for (const QString& n : exps) {
        if (!haveExpFiles.contains(n)) {
            ExpressionInfo e;
            e.file = n;
            e.name = QFileInfo(n).completeBaseName();
            e.name.remove(QStringLiteral(".exp3"));
            settings.expressions.append(e);
            ++added;
        }
    }
    if (added > 0)
        summary += QStringLiteral("• %1 expressões adicionadas encontradas na pasta\n").arg(added);

    summary += QStringLiteral("• Fundo, zoom e posição podem ser ajustados depois na aba \"Fundo/OBS\"");
    return summary;
}
