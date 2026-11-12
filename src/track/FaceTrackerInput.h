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

private:
    static float clampf(float v, float lo, float hi)
    {
        if (v < lo) return lo;
        if (v > hi) return hi;
        return v;
    }

    OneEuroFilter _fx, _fy, _fz;
};

// Compatibilidade com o nome antigo (uso interno).
using FaceTrackerInput = FaceInput;
