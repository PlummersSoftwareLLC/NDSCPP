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

class DaveDebugEffect : public LEDEffectBase
{
public:
    DaveDebugEffect(const string& name)
        : LEDEffectBase(name)
    {
    }

    void Update(ICanvas& canvas, milliseconds deltaTime) override
    {
        (void) deltaTime;

        auto& graphics = canvas.Graphics();
        const auto width = graphics.Width();
        const auto height = graphics.Height();

        if (width == 0 || height == 0)
            return;

        graphics.Clear(CRGB::Blue);
        // Fill the pixels with a rainbow pattern
        for (uint32_t y = 0; y < height; ++y)
        {
            for (uint32_t x = 0; x < width; ++x)
            {
                const auto hue = static_cast<uint8_t>((x + y) % 256);
                graphics.SetPixel(x, y, CHSV(hue, 255, 255));
            }
        }
    }

    friend inline void to_json(nlohmann::json& j, const DaveDebugEffect& effect);
    friend inline void from_json(const nlohmann::json& j, shared_ptr<DaveDebugEffect>& effect);
};

inline void to_json(nlohmann::json& j, const DaveDebugEffect& effect)
{
    j = {
        {"name", effect.Name()}
    };
}

inline void from_json(const nlohmann::json& j, shared_ptr<DaveDebugEffect>& effect)
{
    effect = make_shared<DaveDebugEffect>(j.at("name").get<string>());
}
