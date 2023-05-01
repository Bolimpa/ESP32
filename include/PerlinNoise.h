// PerlinNoise.h

#ifndef PERLIN_NOISE_H
#define PERLIN_NOISE_H

#include <Arduino.h>

class PerlinNoise {
public:
    PerlinNoise(unsigned int seed = random(0, 65535));
    float noise(float x, float y) const;

private:
    void precomputeFadeTable();
    float fade(float t) const;
    float lerp(float a, float b, float t) const;
    float dot(const float gradient[2], float x, float y) const;

    int p[512];
    float gradients[256][2];
};

#endif // PERLIN_NOISE_H