#ifndef EASING_H
#define EASING_H

#include <cmath>
#include <functional>

// Easing functions map a linear time (t from 0 to 1) to a non-linear time.
typedef std::function<float(float)> EasingFunc;

// --- Standard Easing Functions --- //

inline float linear(float t) { return t; }

// --- Quadratic --- //
inline float ease_in_quad(float t) { return t * t; }
inline float ease_out_quad(float t) { return t * (2 - t); }
inline float ease_in_out_quad(float t) {
    return t < 0.5 ? 2 * t * t : -1 + (4 - 2 * t) * t;
}

// --- Sine --- //
inline float ease_in_sine(float t) { return 1 - cosf((t * M_PI) / 2); }
inline float ease_out_sine(float t) { return sinf((t * M_PI) / 2); }
inline float ease_in_out_sine(float t) { return -(cosf(M_PI * t) - 1) / 2; }

// --- Exponential --- //
inline float ease_in_expo(float t) { return t == 0 ? 0 : powf(2, 10 * t - 10); }
inline float ease_out_expo(float t) { return t == 1 ? 1 : 1 - powf(2, -10 * t); }
inline float ease_in_out_expo(float t) {
    if (t == 0) return 0;
    if (t == 1) return 1;
    if (t < 0.5) return powf(2, 20 * t - 10) / 2;
    return (2 - powf(2, -20 * t + 10)) / 2;
}

// --- Back --- //
inline float ease_in_back(float t) {
    const float c1 = 1.70158f;
    const float c3 = c1 + 1;
    return c3 * t * t * t - c1 * t * t;
}

inline float ease_out_back(float t) {
    const float c1 = 1.70158f;
    const float c3 = c1 + 1;
    return 1 + c3 * powf(t - 1, 3) + c1 * powf(t - 1, 2);
}

inline float ease_in_out_back(float t) {
    const float c1 = 1.70158f;
    const float c2 = c1 * 1.525f;
    if (t < 0.5)
        return (powf(2 * t, 2) * ((c2 + 1) * 2 * t - c2)) / 2;
    return (powf(2 * t - 2, 2) * ((c2 + 1) * (t * 2 - 2) + c2) + 2) / 2;
}


#endif // EASING_H