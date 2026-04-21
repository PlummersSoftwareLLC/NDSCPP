#pragma once
using namespace std;

#include "../interfaces.h"
#include "../ledeffectbase.h"
#include "../pixeltypes.h"
#include <cmath>

class AuroraEffect : public LEDEffectBase
{
private:
    double _time;
    double _speed;
    double _brightness;
    
public:
    AuroraEffect(const string& name, double speed = 0.2, double brightness = 1.0)
        : LEDEffectBase(name, "AuroraEffect"), _time(0.0), _speed(speed), _brightness(brightness)
    {
    }

    void Start(ICanvas& canvas) override
    {
        _time = 0.0;
    }

    void Update(ICanvas& canvas, microseconds deltaTime) override
    {
        _time += _speed * deltaTime.count() / 1000000.0;
        
        auto& graphics = canvas.Graphics();
        int width = graphics.Width();
        int height = graphics.Height();

        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                // Create some "dancing" waves using multiple sine waves
                double xf = x / static_cast<double>(width);
                double yf = y / static_cast<double>(height);

                double v1 = sin(xf * 2.0 + _time * 0.7);
                double v2 = sin(yf * 3.0 + _time * 0.5);
                double v3 = sin((xf + yf) * 1.5 + _time * 0.3);
                double v4 = sin(sqrt(xf*xf + yf*yf) * 4.0 + _time * 0.2);
                
                double combined = (v1 + v2 + v3 + v4) / 4.0; // range [-1, 1]
                combined = (combined + 1.0) / 2.0; // range [0, 1]
                
                // Aurora colors: green, blue, purple
                // We want a nice transition between these colors
                double hue;
                if (combined < 0.3) {
                    // Deep Blue to Cyan (240 to 180)
                    hue = 240.0 - (combined / 0.3) * 60.0;
                } else if (combined < 0.6) {
                    // Cyan to Green (180 to 120)
                    hue = 180.0 - ((combined - 0.3) / 0.3) * 60.0;
                } else {
                    // Green to Purple (120 to 280)
                    double t = (combined - 0.6) / 0.4;
                    if (t < 0.5) {
                        hue = 120.0 - (t * 2.0) * 40.0; // Green to Lime (120 to 80)
                    } else {
                        hue = 260.0 + (t - 0.5) * 2.0 * 60.0; // Purple to Violet (260 to 320)
                    }
                }
                
                // Value (brightness) should also pulse
                // Lift the floor significantly and use a smoother curve
                double brightness = (0.4 + 0.6 * combined) * _brightness; 
                
                // Add some vertical bands to make it look more like aurora curtains
                double curtain = sin(xf * 15.0 + sin(_time * 0.2) * 5.0) * 0.15 + 0.85;
                brightness *= curtain;

                // Max saturation for visibility
                CRGB color = CRGB::HSV2RGB(hue, 1.0, brightness);
                graphics.SetPixel(x, y, color);
            }
        }
    }

    friend inline void to_json(nlohmann::json& j, const AuroraEffect & effect);
    friend inline void from_json(const nlohmann::json& j, shared_ptr<AuroraEffect>& effect);
};

inline void to_json(nlohmann::json& j, const AuroraEffect & effect) 
{
    j = {
        {"name", effect.Name()},
        {"speed", effect._speed},
        {"brightness", effect._brightness}
    };
}

inline void from_json(const nlohmann::json& j, shared_ptr<AuroraEffect>& effect) 
{
    effect =  make_shared<AuroraEffect>(
        j.at("name").get<string>(),
        j.value("speed", 0.2),
        j.value("brightness", 1.0)
    );
}
