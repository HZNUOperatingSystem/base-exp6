#ifndef XV6_USER_MATH_H
#define XV6_USER_MATH_H

#define PI 3.1415926535f
#define LN2 0.6931471806f

float sqrt_approx(float x);
float exp_approx(float x);
float log_approx(float x);
float pow_approx(float base, float exp);
float sin_approx(float x);
float cos_approx(float x);
void softmax(float* x, int n);

#endif
