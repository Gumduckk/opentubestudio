#pragma once
/*
 * VTuberModel — carrega e anima um modelo Live2D (Cubism 3/4/5, .model3.json)
 * usando o Cubism SDK oficial (funciona nativamente no Linux).
 *
 * Diferente do sample oficial, as expressões são aplicadas por nós (e não pelo
 * CubismExpressionManager). Isso permite "toggle" instantâneo igual ao VTube
 * Studio: a expressão é somada/escrita por cima dos parâmetros de tracking a
 * cada frame, e ao desligar ela simplesmente para de ser aplicada.
 */
#include <QString>
#include <QStringList>
#include <QVector>
#include <QHash>
#include <QElapsedTimer>

#include <Model/CubismUserModel.hpp>
#include <CubismModelSettingJson.hpp>

#include "config/AppConfig.h"

class VTuberModel : public Csm::CubismUserModel
{
public:
    VTuberModel();
    ~VTuberModel() override;

    // Carrega moc3, física, expressões e movimentos. Não toca em OpenGL.
    bool Load(const QString& model3Path, QString* errorOut);

    // Cria renderer e texturas (chamar com contexto GL corrente).
    bool SetupGl(int windowWidth, int windowHeight, QString* errorOut);

    // Chamado por frame: movimento ocioso -> tracking -> expressões -> física.
    void Update(const Channels& channels, const QVector<ParamMapping>& mappings,
                float dtSeconds);

    // Desenha com zoom/pan/aspecto (chamar com contexto GL corrente).
    void Draw(float viewW, float viewH, float zoom, float panX, float panY);

    // ---- Expressões ----
    QStringList expressionNames() const { return _expressionNames; }
    void toggleExpression(const QString& name);
    bool isExpressionActive(const QString& name) const;
    void clearExpressions();

    // ---- Informações do modelo ----
    QStringList parameterIds() const;     // todos os parâmetros do moc
    QStringList motionGroups() const;
    bool hasPhysics() const { return _physics != nullptr; }
    QString modelDirectory() const { return _dir; }
    QString displayName() const { return _name; }

private:
    struct ExpParam
    {
        int   paramIndex = -1;
        float value = 0.0f;
        int   blend = 0; // 0=Add 1=Multiply 2=Overwrite
    };
    struct ExpData
    {
        QString name;
        QString file;
        QVector<ExpParam> params;
        bool  active = false;
        float weight = 0.0f;
    };

    void loadExpressionsFromDir();
    void loadIdleMotions();
    void setupTextures();
    void applyMappings(const Channels& channels, const QVector<ParamMapping>& mappings,
                       float dt);
    void applyExpressions(float dt);
    void updateIdleMotion(float dt);
    int  paramIndex(const QString& id);

    QString _dir;
    QString _name;
    Csm::CubismModelSettingJson* _setting = nullptr; // config do model3.json
    QVector<ExpData> _expressions;
    QStringList _expressionNames;
    QHash<QString, int> _paramIndexCache;
    QHash<QString, float> _smoothedValues;

    QVector<Csm::ACubismMotion*> _idleMotions;
    bool  _hasIdle = false;
    float _time = 0.0f;
    bool  _glReady = false;
};
