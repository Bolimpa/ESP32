#include "PerlinNoise.h"

PerlinNoise::PerlinNoise(unsigned int seed)
{

    randomSeed(seed);
    precomputeFadeTable();

    for (int i = 0; i < 256; ++i)
    {
        p[i] = i;
        float angle = random(0, 1 << 14) / float(1 << 14) * 2.0 * PI;
        gradients[i][0] = cos(angle);
        gradients[i][1] = sin(angle);
    }

    for (int i = 0; i < 256; ++i)
    {
        int j = random(0, 256);
        std::swap(p[i], p[j]);
    }

    memcpy(p + 256, p, 256 * sizeof(int));
}

float PerlinNoise::noise(float x, float y) const
{
    uint8_t xi = static_cast<uint8_t>(floor(x)) & 255;
    uint8_t yi = static_cast<uint8_t>(floor(y)) & 255;

    float xf = x - floor(x);
    float yf = y - floor(y);
    float u = fade(xf);
    float v = fade(yf);

    int a = p[xi] + yi;
    int b = p[xi + 1] + yi;

    float x1 = lerp(dot(gradients[p[a]], xf, yf), dot(gradients[p[b]], xf - 1, yf), u);
    float x2 = lerp(dot(gradients[p[a + 1]], xf, yf - 1), dot(gradients[p[b + 1]], xf - 1, yf - 1), u);

    return lerp(x1, x2, v);
}

float fadeTable[256];

void PerlinNoise::precomputeFadeTable()
{
    for (int i = 0; i < 256; ++i)
    {
        float t = i / 255.0f;
        fadeTable[i] = t * t * t * (t * (t * 6 - 15) + 10);
    }
}

float PerlinNoise::fade(float t) const
{
    int index = static_cast<int>(t * 255);
    return fadeTable[index];
}

float PerlinNoise::lerp(float a, float b, float t) const
{
    return a + t * (b - a);
}

float PerlinNoise::dot(const float gradient[2], float x, float y) const
{
    return gradient[0] * x + gradient[1] * y;
}