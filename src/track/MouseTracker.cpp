#include "MouseTracker.h"

#include <cmath>

void MouseTracker::updatePointer(float nx, float ny)
{
    auto clampf = [](float v, float lo, float hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    };
    _targetX = clampf(nx, -1.0f, 1.0f);
    _targetY = clampf(ny, -1.0f, 1.0f);
}

void MouseTracker::tick(float dt)
{
    const float k = 14.0f;
    const float alpha = 1.0f - std::exp(-dt * k);
    _x += (_targetX - _x) * alpha;
    _y += (_targetY - _y) * alpha;
}
