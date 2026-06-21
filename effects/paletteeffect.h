#pragma once

// PaletteEffect
// 
// A versatile effect that scrolls a palette of colors across the canvas.  The effect can be configured
// with a variety of parameters to control the speed, density, and appearance of the scrolling colors.
//
// The palette effect advances one pixel at a time and one color at a time through the supplied palette.
// The speed of the color change and the speed of the pixel movement can be controlled independently.
// If left at a density of 1, you get one color per pixel.  At 0.5, you get a new color every two pixels, etc.

using namespace std;
using namespace std::chrono;

#include "../ledeffectbase.h"
#include "../pixeltypes.h"
#include "../palette.h"

class PaletteEffect : public LEDEffectBase 
{
private:
    double _iPixel = 0;
    double _iColor;

public:
    Palette  _Palette;
    double   _LEDColorPerSecond = 3.0;
    double   _LEDScrollSpeed = 0.0;
    double   _Density = 1.0;
    double   _EveryNthDot = 1.0;
    uint32_t _DotSize = 1;
    bool     _RampedColor = false;
    double   _Brightness = 1.0;
    bool     _Mirrored = false;

    // New constructor taking std::vector directly
    PaletteEffect(const    string & name, 
                  const    vector<CRGB> & colors,
                  double   ledColorPerSecond = 3.0,
                  double   ledScrollSpeed = 0.0,
                  double   density = 1.0,
                  double   everyNthDot = 1.0,
                  uint32_t dotSize = 1,
                  bool     rampedColor = false,
                  double   brightness = 1.0,
                  bool     mirrored = false,
                  bool     bBlend   = true) 
        : LEDEffectBase(name),
          _Palette(colors, bBlend), 
          _iColor(0),
          _LEDColorPerSecond(ledColorPerSecond),
          _LEDScrollSpeed(ledScrollSpeed),
          _Density(density),
          _EveryNthDot(everyNthDot),
          _DotSize(dotSize),
          _RampedColor(rampedColor),
          _Brightness(brightness),
          _Mirrored(mirrored)
    {
    }

    void Update(ICanvas& canvas, microseconds deltaTime) override 
    {
        auto& graphics = canvas.Graphics();
        const auto width = graphics.Width();
        const auto height = graphics.Height();
        const auto dotcount = width * height;
        
        graphics.Clear(CRGB::Black);

        // Pre-calculate constants
        const double secondsElapsed = deltaTime.count() / 1000000.0;
        const double cPixelsToScroll = secondsElapsed * _LEDScrollSpeed;
        const double cColorsToScroll = secondsElapsed * _LEDColorPerSecond;
        const uint32_t cLength = (_Mirrored ? dotcount / 2 : dotcount);
        const double cCenter = dotcount / 2.0;
        const double colorIncrement = _Density / _Palette.originalSize();
        const double fadeFactor = 1.0 - _Brightness;
        
        // Update state variables
        _iPixel = fmod(_iPixel + cPixelsToScroll, dotcount);
        _iColor = fmod(_iColor + (cColorsToScroll * _Density), 1.0);
        
        // Draw the scrolling color "dots"

        double iColor = _iColor;
        for (double i = 0; i < cLength; i += _EveryNthDot) 
        {
            double iPixel = fmod(i + _iPixel, cLength);
            CRGB c = _Palette.getColor(iColor).fadeToBlackBy(fadeFactor);
            
            graphics.SetPixelsF(iPixel + (_Mirrored ? cCenter : 0), _DotSize, c);
            if (_Mirrored) 
                graphics.SetPixelsF(cCenter - iPixel, _DotSize, c);
           
            iColor = fmod(iColor + colorIncrement, 1.0);
        }
        
        // Handle pixel 0 flicker prevention
        if (dotcount > 1) {
            graphics.SetPixel(0, 0, graphics.GetPixel(1, 0));
        }
    }

    friend inline void to_json(nlohmann::json& j, const PaletteEffect & effect);
    friend inline void from_json(const nlohmann::json& j, shared_ptr<PaletteEffect>& effect);
};

inline void to_json(nlohmann::json& j, const PaletteEffect & effect) 
{
    j = {
            {"name",              effect.Name()},
            {"palette",           effect._Palette},
            {"ledColorPerSecond", effect._LEDColorPerSecond},
            {"ledScrollSpeed",    effect._LEDScrollSpeed},
            {"density",           effect._Density},
            {"everyNthDot",       effect._EveryNthDot},
            {"dotSize",           effect._DotSize},
            {"rampedColor",       effect._RampedColor},
            {"brightness",        effect._Brightness},
            {"mirrored",          effect._Mirrored}
        };
}

inline void from_json(const nlohmann::json& j, shared_ptr<PaletteEffect>& effect) 
{
    effect = make_shared<PaletteEffect>(
        j.at("name").get<string>(),
        j.at("palette").at("colors").get<vector<CRGB>>(),
        j.at("ledColorPerSecond").get<double>(),
        j.at("ledScrollSpeed").get<double>(),
        j.at("density").get<double>(),
        j.at("everyNthDot").get<double>(),
        j.at("dotSize").get<uint32_t>(),
        j.at("rampedColor").get<bool>(),
        j.at("brightness").get<double>(),
        j.at("mirrored").get<bool>(),
        j.at("palette").at("blend").get<bool>()
    );
}
