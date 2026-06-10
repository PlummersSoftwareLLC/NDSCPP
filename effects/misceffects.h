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

    void Update(ICanvas& canvas, microseconds deltaTime) override
    {
        canvas.Graphics().Clear(_color);
    }

    friend inline void to_json(nlohmann::json& j, const SolidColorFill & effect);
    friend inline void from_json(const nlohmann::json& j, shared_ptr<SolidColorFill>& effect);
};

inline void to_json(nlohmann::json& j, const SolidColorFill & effect) 
{
    j = {
        {"name", effect.Name()},
        {"color", effect._color} // Assumes `to_json` for CRGB is already defined
    };
}

inline void from_json(const nlohmann::json& j, shared_ptr<SolidColorFill>& effect) 
{
    effect = make_shared<SolidColorFill>(
            j.at("name").get<string>(),
            j.at("color").get<CRGB>() // Assumes `from_json` for CRGB is already defined
        );
}