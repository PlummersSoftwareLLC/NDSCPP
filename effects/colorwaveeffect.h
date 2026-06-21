#pragma once
using namespace std;

#include "../interfaces.h"
#include "../ledeffectbase.h"
#include "../pixeltypes.h"

class ColorWaveEffect : public LEDEffectBase
{
private:
    double _hue; // Current hue for the wave
    double _speed; // Speed of hue change
    double _waveFrequency; // Frequency of the wave pattern

public:
    ColorWaveEffect(const string& name, double speed = 0.5, double waveFrequency = 10.0)
        : LEDEffectBase(name), _hue(0.0), _speed(speed), _waveFrequency(waveFrequency)
    {
    }

    void Start(ICanvas& canvas) override
    {
        // Reset the hue at the start
        _hue = 0.0;
    }

    void Update(ICanvas& canvas, microseconds deltaTime) override
    {
        // Increment the hue based on speed and elapsed time
        _hue += _speed * deltaTime.count() / 1000000.0;
        if (_hue >= 1.0) _hue -= 1.0; // Wrap around hue to stay in [0, 1)

        auto& graphics = canvas.Graphics();
        int width = graphics.Width();
        int height = graphics.Height();

        // Draw the wave
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                // Calculate the hue based on position and wave frequency
                double localHue = _hue + (x / static_cast<double>(width) * _waveFrequency);
                if (localHue > 1.0) localHue -= 1.0; // Wrap around hue

                // Convert the hue to RGB and draw the pixel
                graphics.SetPixel(x, y, CRGB::HSV2RGB(localHue * 360.0)); 
            }
        }
    }

    friend inline void to_json(nlohmann::json& j, const ColorWaveEffect & effect);
    friend inline void from_json(const nlohmann::json& j, shared_ptr<ColorWaveEffect>& effect);
};

inline void to_json(nlohmann::json& j, const ColorWaveEffect & effect) 
{
    j = {
        {"name", effect.Name()},
        {"speed", effect._speed},
        {"waveFrequency", effect._waveFrequency}
    };
}

inline void from_json(const nlohmann::json& j, shared_ptr<ColorWaveEffect>& effect) 
{
    effect =  make_shared<ColorWaveEffect>(
        j.at("name").get<string>(),
        j.value("speed", 0.5),
        j.value("waveFrequency", 10.0)
    );
}

