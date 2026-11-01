#pragma once
// Curvas de easing para keyframes. tipo 0 = Linear, 1 = Degrau, depois familias x (In, Out, InOut).
#include <cmath>
#include <algorithm>

static const int kEaseCount = 32;
static const char* kEaseName[kEaseCount] = {
    "Linear", "Degrau",
    "Quad In", "Quad Out", "Quad InOut",
    "Cubic In", "Cubic Out", "Cubic InOut",
    "Quart In", "Quart Out", "Quart InOut",
    "Quint In", "Quint Out", "Quint InOut",
    "Sine In", "Sine Out", "Sine InOut",
    "Expo In", "Expo Out", "Expo InOut",
    "Circ In", "Circ Out", "Circ InOut",
    "Back In", "Back Out", "Back InOut",
    "Elastic In", "Elastic Out", "Elastic InOut",
    "Bounce In", "Bounce Out", "Bounce InOut"};

inline float easeBounceOut(float t) {
    const float n1 = 7.5625f, d1 = 2.75f;
    if (t < 1.0f / d1) return n1 * t * t;
    if (t < 2.0f / d1) { t -= 1.5f / d1; return n1 * t * t + 0.75f; }
    if (t < 2.5f / d1) { t -= 2.25f / d1; return n1 * t * t + 0.9375f; }
    t -= 2.625f / d1;
    return n1 * t * t + 0.984375f;
}

inline float easeIn(int family, float t) {
    const float PI = 3.14159265f;
    switch (family) {
        case 0: return t * t;
        case 1: return t * t * t;
        case 2: return t * t * t * t;
        case 3: return t * t * t * t * t;
        case 4: return 1.0f - std::cos(t * PI * 0.5f);
        case 5: return t <= 0 ? 0.0f : std::pow(2.0f, 10.0f * t - 10.0f);
        case 6: return 1.0f - std::sqrt(std::max(0.0f, 1.0f - t * t));
        case 7: { const float c1 = 1.70158f, c3 = c1 + 1.0f; return c3 * t * t * t - c1 * t * t; }
        case 8: {
            if (t <= 0) return 0.0f;
            if (t >= 1) return 1.0f;
            const float c4 = 2.0f * PI / 3.0f;
            return -std::pow(2.0f, 10.0f * t - 10.0f) * std::sin((t * 10.0f - 10.75f) * c4);
        }
        default: return 1.0f - easeBounceOut(1.0f - t);
    }
}

inline float easeApply(int type, float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    if (type <= 0) return t;
    if (type == 1) return t < 1.0f ? 0.0f : 1.0f;
    int idx = std::min(type, kEaseCount - 1) - 2;
    int family = idx / 3, mode = idx % 3;
    if (mode == 0) return easeIn(family, t);
    if (mode == 1) return 1.0f - easeIn(family, 1.0f - t);
    return t < 0.5f ? easeIn(family, 2.0f * t) * 0.5f : 1.0f - easeIn(family, 2.0f - 2.0f * t) * 0.5f;
}
