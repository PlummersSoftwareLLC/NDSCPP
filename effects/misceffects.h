#pragma once

using namespace std;
using namespace std::chrono;

#include "../interfaces.h"
#include "../ledeffectbase.h"
#include "../pixeltypes.h"
#include <vector>
#include <random>
#include <cmath>

class SolidColorFill : public LEDEffectBase
{
private:
    CRGB _color;

public:
    SolidColorFill(const string& name, const CRGB& color)
        : LEDEffectBase(name), _color(color)
    {
    }

    void Start(ICanvas& canvas) override
    {
    }

    void Update(ICanvas& canvas, milliseconds deltaTime) override
    {
        canvas.Graphics().Clear(_color);
    }
};