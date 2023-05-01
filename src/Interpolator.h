#ifndef INTERPOLATOR_H
#define INTERPOLATOR_H

#include <Arduino.h>

float lerp(float firstValue, float secondValue, float t)
{
    return (1 - t) * firstValue + t * secondValue;
}

namespace Interpolator
{
    namespace Ease
    {
        float in(float t)
        {
            return t * t;
        }

        float out(float t)
        {
            return 1.0f - (1.0f - t) * (1.0f - t);
        }

        float inOut(float t)
        {
            return lerp(in(t), out(t), t);
        }
    };

    namespace Bounce
    {
        float out(float t)
        {
            if (t == 0.0f || t == 1.0f)
                return t;

            const float n1 = 7.5625f;
            const float d1 = 2.75f;

            if (t < 1.0f / d1)
            {
                return n1 * t * t;
            }
            else if (t < 2.0f / d1)
            {
                t -= 1.5 / d1;
                return n1 * t * t + 0.75f;
            }
            else if (t < 2.5f / d1)
            {
                t -= 2.25 / d1;
                return n1 * t * t + 0.9375f;
            }
            else
            {
                t -= 2.625 / d1;
                return n1 * t * t + 0.984375f;
            }
        }

        float in(float t)
        {
            if (t == 0.0f || t == 1.0f)
                return t;

            return 1.0f - out(1.0f - t);
        }

        float inOut(float t)
        {
            if (t == 0)
            {
                return 0;
            }

            if (t == 1)
            {
                return 1;
            }

            return lerp(in(t), out(t), t);
        }
    };

    namespace Elastic
    {
        float in(float t)
        {
            if (t == 0.0f || t == 1.0f)
                return t;
            return -(powf(2.0f, 10.0f * (t -= 1.0f)) * sinf((t - 0.1f) * (2.0f * M_PI) / 0.4f));
        }

        float out(float t)
        {
            if (t == 0.0f || t == 1.0f)
                return t;
            return powf(2.0f, -10.0f * t) * sinf((t - 0.1f) * (2.0f * M_PI) / 0.4f) + 1.0f;
        }

        float inOut(float t)
        {
            if ((t *= 2) < 1)
                return -0.5f * pow(2, 10 * (t -= 1)) * sinf((t - 0.1f) * (2 * M_PI) / 0.4f);
            return pow(2, -10 * (t -= 1)) * sinf((t - 0.1f) * (2 * M_PI) / 0.4f) * 0.5f + 1;
        }
    };
};

#endif