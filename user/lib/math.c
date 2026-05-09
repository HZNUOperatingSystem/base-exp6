#include "math.h"

float absf(float x) { return x < 0.0f ? -x : x; }

float sqrt_approx(float x) {
    float r;

    if (x <= 0.0f)
        return 0.0f;
    r = x > 1.0f ? x : 1.0f;
    for (int i = 0; i < 12; i++)
        r = 0.5f * (r + x / r);
    return r;
}

float exp_unit(float x);
float exp_approx(float x) {
    int neg = 0;
    int scale = 0;
    float y;

    if (x < 0.0f) {
        neg = 1;
        x = -x;
    }
    if (x > 20.0f)
        x = 20.0f;
    while (x > 1.0f) {
        x *= 0.5f;
        scale++;
    }
    y = exp_unit(x);
    while (scale-- > 0)
        y *= y;
    return neg ? 1.0f / y : y;
}

float sin_approx(float x) {
    float x2;

    while (x > PI)
        x -= 2.0f * PI;
    while (x < -PI)
        x += 2.0f * PI;
    x2 = x * x;

    float t1 = 1.0f / 5040.0f;          // 1/7!
    float t2 = 1.0f / 120.0f - x2 * t1; // 1/5! - x²/7!
    float t3 = 1.0f / 6.0f - x2 * t2;   // 1/3! - x²*(...)

    return x * (1.0f - x2 * t3);
}

float cos_approx(float x) {
    float x2;

    while (x > PI)
        x -= 2.0f * PI;
    while (x < -PI)
        x += 2.0f * PI;
    x2 = x * x;

    float t1 = 1.0f / 720.0f;          // 1/6!
    float t2 = 1.0f / 24.0f - x2 * t1; // 1/4! - x
    float t3 = 1.0f / 2.0f - x2 * t2;  // 1/2! - x²*(...)

    return 1.0f - x2 * t3;
}

void softmax(float* x, int n) {
    float max = x[0];
    float sum = 0.0f;

    for (int i = 1; i < n; i++) {
        if (x[i] > max)
            max = x[i];
    }
    for (int i = 0; i < n; i++) {
        x[i] = exp_approx(x[i] - max);
        sum += x[i];
    }
    for (int i = 0; i < n; i++)
        x[i] /= sum;
}

// MARK: - helpers

float exp_unit(float x) {
    float term = 1.0f;
    float sum = 1.0f;

    for (int i = 1; i <= 10; i++) {
        term *= x / i;
        sum += term;
    }
    return sum;
}
