#pragma once

#include <vector>
#include <cmath>
#include <cstdint>
#include <array>
#include <algorithm>
#include "pixeltypes.h"
#include "palette.h"

class Palette
{
protected:
    std::vector<CRGB> colorEntries;

public:
    bool blend = true;

    static const std::vector<CRGB> Rainbow;
    static const std::vector<CRGB> RainbowStripes;
    static const std::vector<CRGB> ChristmasLights;

    explicit Palette(const std::vector<CRGB> &colors) : colorEntries(colors) {}

    size_t originalSize() const
    {
        return colorEntries.size();
    }

    virtual CRGB getColor(double d) const
    {
        while (d < 0)
            d += 1.0;
        d -= std::floor(d);

        double fracPerColor = 1.0 / colorEntries.size();
        double indexD = d / fracPerColor;
        int index = static_cast<int>(indexD) % colorEntries.size();
        double fraction = indexD - index;

        CRGB color1 = colorEntries[index];
        if (!blend)
            return color1;

        CRGB color2 = colorEntries[(index + 1) % colorEntries.size()];
        return color1.blendWith(color2, 1 - fraction);
    }
};

// GaussianPalette
//
// This palette uses a Gaussian distribution to blend colors smoothly.

class GaussianPalette : public Palette
{
protected:
    double smoothing = 0.0;
    std::array<double, 5> factors = {0.06136, 0.24477, 0.38774, 0.24477, 0.06136};

public:
    explicit GaussianPalette(const std::vector<CRGB> &colors) : Palette(colors)
    {
        smoothing = 1.0 / colors.size();
    }

    CRGB getColor(double d) const override
    {
        double s = smoothing / originalSize();

        auto blendColor = [&](double offset) -> CRGB
        {
            return Palette::getColor(d + offset);
        };

        double red = blendColor(-2 * s).r   * factors[0] +
                     blendColor(-s).r       * factors[1] +
                     blendColor(0).r        * factors[2] +
                     blendColor(s).r        * factors[3] +
                     blendColor(2 * s).r    * factors[4];

        double green = blendColor(-2 * s).g * factors[0] +
                       blendColor(-s).g     * factors[1] +
                       blendColor(0).g      * factors[2] +
                       blendColor(s).g      * factors[3] +
                       blendColor(2 * s).g  * factors[4];

        double blue = blendColor(-2 * s).b  * factors[0] +
                      blendColor(-s).b      * factors[1] +
                      blendColor(0).b       * factors[2] +
                      blendColor(s).b       * factors[3] +
                      blendColor(2 * s).b   * factors[4];

        return CRGB(static_cast<uint8_t>(std::clamp(red, 0.0, 255.0)),
                    static_cast<uint8_t>(std::clamp(green, 0.0, 255.0)),
                    static_cast<uint8_t>(std::clamp(blue, 0.0, 255.0)));
    }
};
