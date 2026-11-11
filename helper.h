#ifndef HELPER
#define HELPER

float clamp(float x, float lowerlimit, float upperlimit) {
    if (x < lowerlimit) return lowerlimit;
    if (x > upperlimit) return upperlimit;
    return x;
}

float smoothstep(float edge0, float edge1, float x) {
    // Scale, and clamp x to the 0..1 range
    x = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    
    // Evaluate cubic Hermite polynomial
    return x * x * (3.0f - 2.0f * x);
}

#endif
