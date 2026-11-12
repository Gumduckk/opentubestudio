#pragma once
#include <cmath>
#include "../config/AppConfig.h"
#include "../track/WebcamTracker.h"

struct FaceAngles {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

class FaceTrackerInput {
public:
    static FaceAngles compute(const FaceReading& f, const FaceNeutral& n,
                             const AppSettings& s, float& sx, float& sy, float& sz) {
        FaceAngles a;
        const float yawOff   = n.valid ? n.yaw   : s.calYaw;
        const float pitchOff = n.valid ? n.pitch : s.calPitch;
        const float rollOff  = n.valid ? n.roll  : s.calRoll;

        float rawX = (f.yaw - yawOff) / 30.0f;
        float rawY = -(f.pitch - pitchOff) / 20.0f;
        float rawZ = (f.roll - rollOff) / 30.0f;

        // deadzone
        if (std::fabs(rawX) < 0.10f) rawX = 0.0f;
        if (std::fabs(rawY) < 0.10f) rawY = 0.0f;
        if (std::fabs(rawZ) < 0.10f) rawZ = 0.0f;

        // desacoplamento por eixo (mais restrito)
        if (std::fabs(rawX) > 0.10f) rawY = rawZ = 0.0f;
        if (std::fabs(rawY) > 0.10f) rawX = rawZ = 0.0f;
        if (std::fabs(rawZ) > 0.10f) rawX = rawY = 0.0f;

        // suavização independente
        const float alpha = 0.35f;
        sx += (rawX - sx) * alpha;
        sy += (rawY - sy) * alpha;
        sz += (rawZ - sz) * alpha;

        a.x = clampf(sx * s.sensHead, -1.0f, 1.0f);
        a.y = clampf(sy * s.sensHead, -1.0f, 1.0f);
        a.z = clampf(sz * s.sensHead, -1.0f, 1.0f);
        return a;
    }

private:
    static float clampf(float v, float lo, float hi) {
        if (v < lo) return lo;
        if (v > hi) return hi;
        return v;
    }
};