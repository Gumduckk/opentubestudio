#pragma once
/*
 * OneEuroFilter — filtro passa-baixa adaptativo (Casiez et al., 2012),
 * o padrão-ouro para suavizar sinais de tracking: suaviza muito quando o
 * sinal está parado (sem tremido) e quase nada quando se move rápido
 * (sem lag). Usa o dt real entre amostras, então funciona em qualquer FPS.
 *
 * Referência: "1€ Filter: A Simple Speed-based Low-pass Filter for
 * Noisy Input in Interactive Systems" (CHI 2012).
 */
#include <cmath>

class OneEuroFilter
{
public:
    // minCutoff: corte mínimo (Hz). Menor = mais suave parado (mais lag em movimentos lentos).
    // beta:      quanto a velocidade aumenta o corte. Maior = menos lag em movimentos rápidos.
    // dCutoff:   corte do filtro derivativo (Hz). 1 Hz é suficiente na prática.
    explicit OneEuroFilter(float minCutoff = 1.0f, float beta = 0.5f, float dCutoff = 1.0f)
        : _minCutoff(minCutoff), _beta(beta), _dCutoff(dCutoff)
    {
    }

    void setParams(float minCutoff, float beta)
    {
        _minCutoff = minCutoff;
        _beta = beta;
    }

    void reset()
    {
        _hasPrev = false;
        _prevX = 0.0f;
        _prevDx = 0.0f;
    }

    float filter(float x, float dt)
    {
        if (!(dt > 0.0f) || !std::isfinite(dt))
            return _hasPrev ? _prevX : x;

        if (!_hasPrev) {
            _hasPrev = true;
            _prevX = x;
            _prevDx = 0.0f;
            return x;
        }

        // Derivada suavizada
        const float dx = (x - _prevX) / dt;
        const float edx = exponentialSmoothing(dx, _prevDx, alpha(_dCutoff, dt));

        // Corte adaptativo: quanto mais rápido o movimento, mais alto o corte
        const float cutoff = _minCutoff + _beta * std::fabs(edx);
        const float result = exponentialSmoothing(x, _prevX, alpha(cutoff, dt));

        _prevX = result;
        _prevDx = edx;
        return result;
    }

private:
    static float alpha(float cutoff, float dt)
    {
        const float tau = 1.0f / (2.0f * 3.14159265358979f * cutoff);
        return 1.0f / (1.0f + tau / dt);
    }

    static float exponentialSmoothing(float x, float prev, float a)
    {
        return a * x + (1.0f - a) * prev;
    }

    float _minCutoff;
    float _beta;
    float _dCutoff;
    bool  _hasPrev = false;
    float _prevX = 0.0f;
    float _prevDx = 0.0f;
};
