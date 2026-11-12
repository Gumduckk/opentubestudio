#pragma once
/*
 * FaceTrackerInput — converte a leitura crua da webcam em ângulos normalizados
 * (-1..1) para os canais FaceAngleX/Y/Z.
 *
 * Pipeline (inspirado no Kalidokit e no OpenSeeFace):
 *   1. subtrai o neutro calibrado e normaliza pela amplitude nominal;
 *   2. deadzone circular-ish por eixo (ignora tremor fino em repouso);
 *   3. atenuação PROGRESSIVA dos eixos não-dominantes (em vez do corte seco
 *      antigo, que "destravava" de repente o eixo secundário);
 *   4. filtragem One Euro (suaviza parado, quase sem lag em movimento).
 *
 * A classe é stateful (guarda os filtros): uma instância por rastreador.
 */
#include <cmath>
#include "../config/AppConfig.h"
#include "../track/OneEuroFilter.h"
#include "../track/WebcamTracker.h"

struct FaceAngles {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

class FaceInput {
public:
    // minCutoff/beta vêm das settings (sensHead não entra aqui; a escala
    // final de sensibilidade é aplicada depois, em ModelView).
    explicit FaceInput(float minCutoff = 1.0f, float beta = 0.5f)
        : _fx(minCutoff, beta), _fy(minCutoff, beta), _fz(minCutoff, beta)
    {
    }

    void setFilterParams(float minCutoff, float beta)
    {
        _fx.setParams(minCutoff, beta);
        _fy.setParams(minCutoff, beta);
        _fz.setParams(minCutoff, beta);
    }

    void reset()
    {
        _fx.reset();
        _fy.reset();
        _fz.reset();
        _px.reset();
        _py.reset();
        _pz.reset();
    }

    FaceAngles compute(const FaceReading& f, const FaceNeutral& n,
                       const AppSettings& s, float dt)
    {
        const float yawOff   = n.valid ? n.yaw   : s.calYaw;
        const float pitchOff = n.valid ? n.pitch : s.calPitch;
        const float rollOff  = n.valid ? n.roll  : s.calRoll;

        float rawX = -(f.yaw   - yawOff)   / 30.0f;
        float rawY = (f.pitch - pitchOff) / 20.0f;  // sinal ajustado (nod invertido no teste)
        float rawZ = -(f.roll  - rollOff)  / 30.0f;

        // deadzone por eixo
        if (std::fabs(rawX) < 0.10f) rawX = 0.0f;
        if (std::fabs(rawY) < 0.10f) rawY = 0.0f;
        if (std::fabs(rawZ) < 0.10f) rawZ = 0.0f;

        // Atenuação progressiva dos eixos não-dominantes: quanto mais um
        // eixo domina, mais os outros são atenuados — suavemente, sem o
        // "salto" do corte seco anterior.
        const float ax = std::fabs(rawX), ay = std::fabs(rawY), az = std::fabs(rawZ);
        const float dom = std::fmax(ax, std::fmax(ay, az));
        if (dom > 0.10f) {
            // 1.0 no limiar -> 0.25 em amplitude total (nunca zera de vez,
            // só evita o vazamento artificial estilo "cuello robótico").
            const float base = 1.0f - 0.75f * std::fmin(1.0f, (dom - 0.10f) / 0.90f);
            if (ax >= ay && ax >= az) { rawY *= base; rawZ *= base; }
            else if (ay >= ax && ay >= az) { rawX *= base; rawZ *= base; }
            else                        { rawX *= base; rawY *= base; }
        }

        const float sx = _fx.filter(rawX, dt);
        const float sy = _fy.filter(rawY, dt);
        const float sz = _fz.filter(rawZ, dt);

        FaceAngles a;
        a.x = clampf(sx * s.sensHead, -1.0f, 1.0f);
        a.y = clampf(sy * s.sensHead, -1.0f, 1.0f);
        a.z = clampf(sz * s.sensHead, -1.0f, 1.0f);
        return a;
    }

    // Posição 3D do rosto (lean in/out, andar pro lado). Normaliza pelas
    // MEDIDAS DO NEUTRO: altura do rosto = 15.7 cm × escala da matriz de
    // calibração (a matriz é R·S uniforme), então comparações são de
    // "fração de rosto". sensPos multiplica o alcance final.
    // Retorna x = lateral, y = vertical, z = aproximar (positivo = mais perto).
    FaceAngles computePosition(const FaceReading& f, const FaceNeutral& n,
                               float sensPos, float dt)
    {
        const float fw = (n.valid && n.faceScale > 1e-3f)
                             ? 15.7f * n.faceScale
                             : (f.faceScale > 1e-3f ? 15.7f * f.faceScale : 15.7f);
        const float nx = n.valid ? n.faceX : 0.0f;
        const float ny = n.valid ? n.faceY : 0.0f;
        const float nz = n.valid ? n.faceZ : 0.0f;

        float dx = (f.faceX - nx) / fw;   // fração de rosto
        float dy = (f.faceY - ny) / fw;
        float dz = -(f.faceZ - nz) / fw;

        // deadzone pequena
        if (std::fabs(dx) < 0.04f) dx = 0.0f;
        if (std::fabs(dy) < 0.04f) dy = 0.0f;
        if (std::fabs(dz) < 0.04f) dz = 0.0f;

        const float px = _px.filter(dx, dt);
        const float py = _py.filter(dy, dt);
        const float pz = _pz.filter(dz, dt);

        FaceAngles a;
        a.x = clampf(px * sensPos, -1.0f, 1.0f);
        a.y = clampf(py * sensPos, -1.0f, 1.0f);
        a.z = clampf(pz * sensPos, -1.0f, 1.0f);
        return a;
    }

    void resetPosition()
    {
        _px.reset();
        _py.reset();
        _pz.reset();
    }

private:
    static float clampf(float v, float lo, float hi)
    {
        if (v < lo) return lo;
        if (v > hi) return hi;
        return v;
    }

    OneEuroFilter _fx, _fy, _fz;
    OneEuroFilter _px, _py, _pz;
};

// Compatibilidade com o nome antigo (uso interno).
using FaceTrackerInput = FaceInput;
