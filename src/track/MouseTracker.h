#pragma once
/*
 * MouseTracker — converte a posição do cursor (normalizada -1..1, relativa ao
 * centro da janela) em canais de cabeça/olhos, com suavização.
 */
#include <QObject>

class MouseTracker
{
public:
    // nx/ny: -1..1 (0 = centro da janela)
    void updatePointer(float nx, float ny);

    float x() const { return _x; }
    float y() const { return _y; }

    // Chama a cada frame para suavizar em direção ao último ponteiro.
    void tick(float dt);

private:
    float _targetX = 0.0f;
    float _targetY = 0.0f;
    float _x = 0.0f;
    float _y = 0.0f;
};
