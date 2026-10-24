#include "VTuberModel.h"

#include <GL/glew.h>

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QImage>
#include <QSet>
#include <QMap>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QOpenGLContext>
#include <QtMath>
#include <cmath>
#include <cstdio>
#include <vector>

#include <Rendering/OpenGL/CubismRenderer_OpenGLES2.hpp>
#include <Rendering/OpenGL/CubismOffscreenManager_OpenGLES2.hpp>
#include <Math/CubismMatrix44.hpp>
#include <Motion/CubismMotion.hpp>
#include <Id/CubismIdManager.hpp>
#include <Id/CubismId.hpp>

using namespace Csm;

// ---------------------------------------------------------------------------
// Auxiliares
// ---------------------------------------------------------------------------
static std::vector<unsigned char> readFileBytes(const QString& path)
{
    QFile f(path);
    std::vector<unsigned char> out;
    if (!f.open(QIODevice::ReadOnly))
        return out;
    const QByteArray b = f.readAll();
    out.assign(b.begin(), b.end());
    return out;
}

VTuberModel::VTuberModel() = default;

VTuberModel::~VTuberModel()
{
    for (Csm::ACubismMotion* m : _idleMotions)
        Csm::ACubismMotion::Delete(m);
    _idleMotions.clear();
    delete _setting;
    _setting = nullptr;
}

// ---------------------------------------------------------------------------
// Carregamento
// ---------------------------------------------------------------------------
bool VTuberModel::Load(const QString& model3Path, QString* errorOut)
{
    const QFileInfo fi(model3Path);
    if (!fi.exists()) {
        if (errorOut) *errorOut = QStringLiteral("Arquivo não encontrado: %1").arg(model3Path);
        return false;
    }
    _dir = fi.absolutePath();
    _name = fi.completeBaseName();
    _name.remove(QStringLiteral(".model3"));

    // model3.json
    const std::vector<unsigned char> settingBytes = readFileBytes(model3Path);
    if (settingBytes.empty()) {
        if (errorOut) *errorOut = QStringLiteral("Não foi possível ler %1").arg(model3Path);
        return false;
    }
    _setting = new Csm::CubismModelSettingJson(settingBytes.data(), settingBytes.size());

    // moc3
    if (QString::fromUtf8(_setting->GetModelFileName()).isEmpty()) {
        if (errorOut) *errorOut = QStringLiteral("model3.json sem arquivo .moc3");
        return false;
    }
    const QString mocPath = _dir + QLatin1Char('/') + QString::fromUtf8(_setting->GetModelFileName());
    const std::vector<unsigned char> mocBytes = readFileBytes(mocPath);
    if (mocBytes.empty()) {
        if (errorOut) *errorOut = QStringLiteral("Não foi possível ler %1").arg(mocPath);
        return false;
    }
    LoadModel(mocBytes.data(), mocBytes.size(), true);
    if (!_model) {
        if (errorOut) *errorOut = QStringLiteral("moc3 inválido ou corrompido: %1").arg(mocPath);
        return false;
    }
    // Modelo ocupa 2 unidades de largura no espaço de desenho.
    GetModelMatrix()->SetWidth(2.0f);

    // Física
    if (strcmp(_setting->GetPhysicsFileName(), "") != 0) {
        const QString physPath = _dir + QLatin1Char('/')
                                 + QString::fromUtf8(_setting->GetPhysicsFileName());
        const std::vector<unsigned char> bytes = readFileBytes(physPath);
        if (!bytes.empty())
            LoadPhysics(bytes.data(), bytes.size());
    }

    // Pose (raramente usada neste modelo, mas suportada)
    if (strcmp(_setting->GetPoseFileName(), "") != 0) {
        const QString posePath = _dir + QLatin1Char('/')
                                 + QString::fromUtf8(_setting->GetPoseFileName());
        const std::vector<unsigned char> bytes = readFileBytes(posePath);
        if (!bytes.empty())
            LoadPose(bytes.data(), bytes.size());
    }

    // Expressões: as referenciadas no model3.json + todos os *.exp3.json da
    // pasta (assim expressões "soltas" — comuns em modelos do VTS — funcionam).
    loadExpressionsFromDir();

    // Movimentos ociosos (se o modelo tiver; este não tem, aí fica com
    // respiração/ piscada procedurais)
    loadIdleMotions();
    return true;
}

void VTuberModel::loadExpressionsFromDir()
{
    _expressions.clear();
    _expressionNames.clear();

    struct Cand { QString name; QString file; };
    QVector<Cand> cands;

    // Referências no model3.json
    const csmInt32 count = _setting->GetExpressionCount();
    for (csmInt32 i = 0; i < count; ++i) {
        Cand c;
        c.name = QString::fromUtf8(_setting->GetExpressionName(i));
        c.file = QString::fromUtf8(_setting->GetExpressionFileName(i));
        cands.append(c);
    }

    // Varredura da pasta (dedup por arquivo)
    QSet<QString> have;
    for (const Cand& c : cands)
        have.insert(c.file);
    const QDir dir(_dir);
    const QStringList files = dir.entryList({ QStringLiteral("*.exp3.json") },
                                            QDir::Files, QDir::Name);
    for (const QString& f : files) {
        if (have.contains(f))
            continue;
        Cand c;
        c.file = f;
        c.name = QFileInfo(f).completeBaseName();
        c.name.remove(QStringLiteral(".exp3"));
        cands.append(c);
    }

    for (const Cand& c : cands) {
        const std::vector<unsigned char> bytes = readFileBytes(dir.filePath(c.file));
        if (bytes.empty())
            continue;
        const QJsonObject root = QJsonDocument::fromJson(
                                     QByteArray(reinterpret_cast<const char*>(bytes.data()),
                                                static_cast<int>(bytes.size()))).object();
        ExpData exp;
        exp.name = c.name;
        exp.file = c.file;
        for (const QJsonValue& pv : root[QStringLiteral("Parameters")].toArray()) {
            const QJsonObject p = pv.toObject();
            ExpParam ep;
            ep.paramIndex = paramIndex(p[QStringLiteral("Id")].toString());
            ep.value = static_cast<float>(p[QStringLiteral("Value")].toDouble());
            const QString blend = p[QStringLiteral("Blend")].toString();
            ep.blend = (blend == QLatin1String("Multiply")) ? 1
                       : (blend == QLatin1String("Overwrite")) ? 2 : 0;
            if (ep.paramIndex >= 0)
                exp.params.append(ep);
        }
        if (exp.params.isEmpty())
            continue;
        _expressions.append(exp);
        _expressionNames.append(exp.name);
    }
}

void VTuberModel::loadIdleMotions()
{
    _idleMotions.clear();
    _hasIdle = false;
    const QStringList groups = motionGroups();
    static const char* kIdleNames[] = { "Idle", "idle", "Motion", "motion" };
    QString idleGroup;
    for (const char* g : kIdleNames) {
        if (groups.contains(QLatin1String(g))) {
            idleGroup = QLatin1String(g);
            break;
        }
    }
    if (idleGroup.isEmpty())
        return;

    const QByteArray groupUtf8 = idleGroup.toUtf8();
    const csmInt32 n = _setting->GetMotionCount(groupUtf8.constData());
    for (csmInt32 i = 0; i < n; ++i) {
        const QString file = QString::fromUtf8(
            _setting->GetMotionFileName(groupUtf8.constData(), i));
        const std::vector<unsigned char> bytes = readFileBytes(_dir + QLatin1Char('/') + file);
        if (bytes.empty())
            continue;
        const QByteArray nameUtf8 = file.toUtf8();
        Csm::ACubismMotion* motion = LoadMotion(bytes.data(), bytes.size(),
                                                nameUtf8.constData());
        if (motion) {
            if (Csm::CubismMotion* m = dynamic_cast<Csm::CubismMotion*>(motion)) {
                Csm::csmVector<Csm::CubismIdHandle> eyes;
                eyes.PushBack(Csm::CubismFramework::GetIdManager()->GetId("ParamEyeLOpen"));
                eyes.PushBack(Csm::CubismFramework::GetIdManager()->GetId("ParamEyeROpen"));
                Csm::csmVector<Csm::CubismIdHandle> lips;
                lips.PushBack(Csm::CubismFramework::GetIdManager()->GetId("ParamMouthOpenY"));
                m->SetEffectIds(eyes, lips);
            }
            _idleMotions.append(motion);
        }
    }
    _hasIdle = !_idleMotions.isEmpty();
}

// ---------------------------------------------------------------------------
// GL
// ---------------------------------------------------------------------------
bool VTuberModel::SetupGl(int windowWidth, int windowHeight, QString* errorOut)
{
    if (!_model) {
        if (errorOut) *errorOut = QStringLiteral("Nenhum modelo carregado");
        return false;
    }
    CreateRenderer(static_cast<csmUint32>(windowWidth),
                   static_cast<csmUint32>(windowHeight));
    if (!GetRenderer<Csm::Rendering::CubismRenderer_OpenGLES2>()) {
        if (errorOut) *errorOut = QStringLiteral("Falha ao criar o renderer OpenGL");
        return false;
    }
    setupTextures();
    _glReady = true;

    if (qEnvironmentVariableIsSet("GUMM_DEBUG")) {
        auto* r = GetRenderer<Csm::Rendering::CubismRenderer_OpenGLES2>();
        fprintf(stderr, "[model] setupGl ok: textures=%d drawables=%d canvas=%gx%g "
                        "blendMode=%d masking=%d renderer=%p\n",
              _setting->GetTextureCount(), _model->GetDrawableCount(),
              double(_model->GetCanvasWidth()), double(_model->GetCanvasHeight()),
              _model->IsBlendModeEnabled() ? 1 : 0,
              _model->IsUsingMasking() ? 1 : 0,
              static_cast<void*>(r));

        // Cruzamento moc3 <-> model3.json: quais índices de textura os
        // drawables usam e quantos drawables apontam para cada textura.
        QMap<int, int> useCount;
        int maxIdx = -1;
        for (csmInt32 d = 0; d < _model->GetDrawableCount(); ++d) {
            const int ti = _model->GetDrawableTextureIndex(d);
            useCount[ti]++;
            maxIdx = qMax(maxIdx, ti);
        }
        fprintf(stderr, "[model] drawable->texture: max=%d (textures carregadas=%d)%,"
                        " distribuição:", maxIdx, _setting->GetTextureCount());
        for (auto it = useCount.constBegin(); it != useCount.constEnd(); ++it)
            fprintf(stderr, " [%d]=%d", it.key(), it.value());
        fprintf(stderr, "\n");
    }
    return true;
}

void VTuberModel::setupTextures()
{
    auto* renderer = GetRenderer<Csm::Rendering::CubismRenderer_OpenGLES2>();
    if (!renderer)
        return;

    for (csmInt32 i = 0; i < _setting->GetTextureCount(); ++i) {
        const QString rel = QString::fromUtf8(_setting->GetTextureFileName(i));
        if (rel.isEmpty())
            continue;

        const QString path = _dir + QLatin1Char('/') + rel;
        QImage img(path);
        if (img.isNull()) {
            qWarning("[GummVTuber] textura não encontrada: %s", qPrintable(path));
            continue;
        }

        img = img.convertToFormat(QImage::Format_RGBA8888);

        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        // O QImage alinha cada linha (bytesPerLine), que pode ter padding;
        // sem informar o row length o GL lê com stride errado.
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(img.bytesPerLine() / 4));
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.width(), img.height(), 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, img.constBits());
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

        // O shader do Cubism amostra com GL_LINEAR_MIPMAP_LINEAR
        // (CubismShader_OpenGLES2::SetupTexture). Sem a cadeia de mipmaps a
        // textura fica incompleta e o GL devolve (0,0,0,1) => silhueta preta.
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        // A assinatura é BindTexture(modelTextureIndex, glTextureIndex).
        renderer->BindTexture(i, tex);
    }

    // Os assets do VTube Studio são PNG com alpha "straight" (não pré-multiplicado).
    renderer->IsPremultipliedAlpha(false);
}

// ---------------------------------------------------------------------------
// Atualização por frame
// ---------------------------------------------------------------------------
void VTuberModel::Update(const Channels& channels,
                         const QVector<ParamMapping>& mappings, float dt)
{
    _time += dt;

    // Base: estado salvo (movimento ocioso/expressões do framework)
    _model->LoadParameters();
    updateIdleMotion(dt);
    _model->SaveParameters();

    // Tracking (mapeamento entrada -> parâmetro, com suavização por saída)
    applyMappings(channels, mappings, dt);

    // Expressões por cima (estilo VTS: somam no valor atual)
    applyExpressions(dt);

    // Sincroniza parâmetros -> drawables
    _model->Update();

    // Física (cabelo, acessórios...) reage ao movimento atual
    if (_physics)
        _physics->Evaluate(_model, dt);
}

void VTuberModel::updateIdleMotion(float dt)
{
    if (!_hasIdle)
        return;
    if (_motionManager->IsFinished()) {
        const int idx = QRandomGenerator::global()->bounded(_idleMotions.size());
        _motionManager->StartMotionPriority(_idleMotions[idx], false, 1 /* idle */);
    }
    _motionManager->UpdateMotion(_model, dt);
}

void VTuberModel::applyMappings(const Channels& channels,
                                const QVector<ParamMapping>& mappings, float dt)
{
    for (const ParamMapping& m : mappings) {
        if (!m.enabled)
            continue;
        const int idx = paramIndex(m.output);
        if (idx < 0)
            continue;

        const float raw = channels.valueFor(m.input);
        const float span = (m.inMax - m.inMin);
        float t = (span != 0.0f) ? (raw - m.inMin) / span : 0.0f;
        float target = m.outMin + t * (m.outMax - m.outMin);
        if (m.clampOutput) {
            const float lo = qMin(m.outMin, m.outMax);
            const float hi = qMax(m.outMin, m.outMax);
            target = qBound(lo, target, hi);
        }

        // Suavização exponencial (quanto maior o "smoothing", mais lento)
        float cur = _smoothedValues.value(m.output, target);
        if (m.smoothing > 0.0f) {
            const float k = 15.0f / (1.0f + m.smoothing * 0.35f);
            const float alpha = 1.0f - std::exp(-dt * k);
            cur += (target - cur) * alpha;
        } else {
            cur = target;
        }
        _smoothedValues.insert(m.output, cur);
        _model->SetParameterValue(idx, cur);
    }
}

void VTuberModel::applyExpressions(float dt)
{
    for (ExpData& e : _expressions) {
        const float target = e.active ? 1.0f : 0.0f;
        e.weight += (target - e.weight) * (1.0f - std::exp(-dt * 8.0f));
        if (e.weight < 0.002f && !e.active)
            continue;
        for (const ExpParam& p : e.params) {
            const float cur = _model->GetParameterValue(p.paramIndex);
            float v = cur;
            switch (p.blend) {
            case 1:  // Multiply
                v = cur * (1.0f + (p.value - 1.0f) * e.weight);
                break;
            case 2:  // Overwrite
                v = cur + (p.value - cur) * e.weight;
                break;
            default: // Add
                v = cur + p.value * e.weight;
                break;
            }
            _model->SetParameterValue(p.paramIndex, v);
        }
    }
}

void VTuberModel::toggleExpression(const QString& name)
{
    for (ExpData& e : _expressions) {
        if (e.name == name) {
            e.active = !e.active;
            return;
        }
    }
}

bool VTuberModel::isExpressionActive(const QString& name) const
{
    for (const ExpData& e : _expressions)
        if (e.name == name)
            return e.active;
    return false;
}

void VTuberModel::clearExpressions()
{
    for (ExpData& e : _expressions)
        e.active = false;
}

// ---------------------------------------------------------------------------
// Desenho
// ---------------------------------------------------------------------------
void VTuberModel::Draw(float viewW, float viewH, float zoom, float panX, float panY)
{
    if (!_glReady || !_model)
        return;

    // O modelo ocupa 2 unidades de largura, centrado na origem. Mantém o
    // centro sempre dentro do quadro para o modelo nunca "sumir" por causa
    // de um pan salvo (isso já aconteceu: panY = -0.5 deixava tudo fora).
    panX = qBound(-1.5f, panX, 1.5f);
    panY = qBound(-1.5f, panY, 1.5f);

    // O render target offscreen (usado por modelos com blend mode) precisa
    // acompanhar o tamanho atual da tela; senão o blit final cobre a tela
    // inteira com o conteúdo (possivelmente preto) do buffer antigo.
    SetRenderTargetSize(static_cast<csmUint32>(qMax(1.0f, viewW)),
                        static_cast<csmUint32>(qMax(1.0f, viewH)));

    Csm::Rendering::CubismOffscreenManager_OpenGLES2::GetInstance()->BeginFrameProcess();

    Csm::CubismMatrix44 mvp;
    mvp.LoadIdentity();

    // Escala UNIFORME: o modelo tem 2 unidades de largura (ver SetWidth em
    // Load). Escalamos igual nos dois eixos para preservar a proporção do
    // personagem — escalas diferentes por eixo "engordavam" o modelo ao
    // redimensionar a janela.
    const float minSide = (viewW < viewH) ? viewW : viewH;
    const float s = minSide / 2.0f;
    mvp.Scale(s, s);
    mvp.Scale(zoom, zoom);
    mvp.Translate(panX, panY);
    mvp.MultiplyByMatrix(GetModelMatrix());

    GetRenderer<Csm::Rendering::CubismRenderer_OpenGLES2>()->SetMvpMatrix(&mvp);
    GetRenderer<Csm::Rendering::CubismRenderer_OpenGLES2>()->DrawModel();

    Csm::Rendering::CubismOffscreenManager_OpenGLES2::GetInstance()->EndFrameProcess();
    Csm::Rendering::CubismOffscreenManager_OpenGLES2::GetInstance()->ReleaseStaleRenderTextures();
}

// ---------------------------------------------------------------------------
// Consultas
// ---------------------------------------------------------------------------
int VTuberModel::paramIndex(const QString& id)
{
    const auto it = _paramIndexCache.constFind(id);
    if (it != _paramIndexCache.constEnd())
        return it.value();
    const QByteArray idUtf8 = id.toUtf8();
    const int idx = _model->GetParameterIndex(
        Csm::CubismFramework::GetIdManager()->GetId(idUtf8.constData()));
    _paramIndexCache.insert(id, idx);
    return idx;
}

QStringList VTuberModel::parameterIds() const
{
    QStringList out;
    if (!_model)
        return out;
    for (csmInt32 i = 0; i < _model->GetParameterCount(); ++i)
        out.append(QString::fromUtf8(_model->GetParameterId(i)->GetString().GetRawString()));
    return out;
}

QStringList VTuberModel::motionGroups() const
{
    // Não existe API para enumerar grupos; testa os nomes comuns.
    QStringList out;
    if (!_setting)
        return out;
    static const char* kGroups[] = { "Idle", "idle", "Motion", "motion",
                                     "TapBody", "Talk", "Flick" };
    for (const char* g : kGroups) {
        if (_setting->GetMotionCount(g) > 0)
            out.append(QLatin1String(g));
    }
    return out;
}
