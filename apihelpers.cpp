
#include "apihelpers.h"
#include "json.hpp"
#include "pixeltypes.h"
#include "effects/misceffects.h"
#include "effects/paletteeffect.h"
#include "effects/colorwaveeffect.h"
#include "effects/starfield.h"
#include "effects/stockbannereffect.h"
#include "effects/bouncingballeffect.h"
#include "effects/fireworkseffect.h"
#include "effects/videoeffect.h"

namespace ndscpp::api
{

namespace
{

nlohmann::json CreateEffectCatalogJson()
{
    using nlohmann::json;

    const auto paletteDefaults = json{
        {"colors", StandardPalettes::Rainbow},
        {"blend", true}
    };

    return json{
        {"effects", json::array({
            {
                {"type", typeid(SolidColorFill).name()},
                {"label", "Solid Color Fill"},
                {"defaults", json{{"color", CRGB(255, 96, 64)}}},
                {"fields", json::array({
                    {{"path", "color"}, {"label", "Color"}, {"input", "color"}}
                })}
            },
            {
                {"type", typeid(DaveDebugEffect).name()},
                {"label", "Dave Debug"},
                {"defaults", json::object()},
                {"fields", json::array()}
            },
            {
                {"type", typeid(PaletteEffect).name()},
                {"label", "Palette Scroll"},
                {"defaults", json{
                    {"palette", paletteDefaults},
                    {"ledColorPerSecond", 3.0},
                    {"ledScrollSpeed", 0.0},
                    {"density", 1.0},
                    {"everyNthDot", 1.0},
                    {"dotSize", 1},
                    {"rampedColor", false},
                    {"brightness", 1.0},
                    {"mirrored", false}
                }},
                {"fields", json::array({
                    {{"path", "palette.colors"}, {"label", "Palette Colors"}, {"input", "json"}},
                    {{"path", "palette.blend"}, {"label", "Blend Colors"}, {"input", "checkbox"}},
                    {{"path", "ledColorPerSecond"}, {"label", "Color Rate"}, {"input", "number"}, {"step", 0.1}},
                    {{"path", "ledScrollSpeed"}, {"label", "Scroll Speed"}, {"input", "number"}, {"step", 0.1}},
                    {{"path", "density"}, {"label", "Density"}, {"input", "number"}, {"step", 0.01}},
                    {{"path", "everyNthDot"}, {"label", "Every Nth Dot"}, {"input", "number"}, {"step", 0.1}},
                    {{"path", "dotSize"}, {"label", "Dot Size"}, {"input", "number"}, {"step", 1}, {"min", 1}},
                    {{"path", "rampedColor"}, {"label", "Ramped Color"}, {"input", "checkbox"}},
                    {{"path", "brightness"}, {"label", "Brightness"}, {"input", "number"}, {"step", 0.05}, {"min", 0.0}, {"max", 1.0}},
                    {{"path", "mirrored"}, {"label", "Mirrored"}, {"input", "checkbox"}}
                })}
            },
            {
                {"type", typeid(ColorWaveEffect).name()},
                {"label", "Color Wave"},
                {"defaults", json{{"speed", 0.5}, {"waveFrequency", 10.0}}},
                {"fields", json::array({
                    {{"path", "speed"}, {"label", "Speed"}, {"input", "number"}, {"step", 0.1}},
                    {{"path", "waveFrequency"}, {"label", "Wave Frequency"}, {"input", "number"}, {"step", 0.1}}
                })}
            },
            {
                {"type", typeid(StarfieldEffect).name()},
                {"label", "Starfield"},
                {"defaults", json{{"starCount", 100}}},
                {"fields", json::array({
                    {{"path", "starCount"}, {"label", "Star Count"}, {"input", "number"}, {"step", 1}, {"min", 1}}
                })}
            },
            {
                {"type", typeid(StockBanner).name()},
                {"label", "Stock Banner"},
                {"defaults", json{
                    {"stockServerHost", "localhost"},
                    {"stockServerPort", 8888},
                    {"minQuoteWidth", 64},
                    {"compactQuoteWidth", 96},
                    {"refreshSeconds", 60}
                }},
                {"fields", json::array({
                    {{"path", "symbols"}, {"label", "Symbols"}, {"input", "json"}},
                    {{"path", "stockServerHost"}, {"label", "Stock Server Host"}, {"input", "text"}},
                    {{"path", "stockServerPort"}, {"label", "Stock Server Port"}, {"input", "number"}, {"step", 1}, {"min", 1}},
                    {{"path", "minQuoteWidth"}, {"label", "Min Quote Width"}, {"input", "number"}, {"step", 1}, {"min", 32}},
                    {{"path", "compactQuoteWidth"}, {"label", "Compact Width"}, {"input", "number"}, {"step", 1}, {"min", 64}},
                    {{"path", "refreshSeconds"}, {"label", "Refresh Seconds"}, {"input", "number"}, {"step", 1}, {"min", 15}}
                })}
            },
            {
                {"type", typeid(BouncingBallEffect).name()},
                {"label", "Bouncing Balls"},
                {"defaults", json{{"ballCount", 5}, {"ballSize", 1}, {"mirrored", true}, {"erase", true}}},
                {"fields", json::array({
                    {{"path", "ballCount"}, {"label", "Ball Count"}, {"input", "number"}, {"step", 1}, {"min", 1}},
                    {{"path", "ballSize"}, {"label", "Ball Size"}, {"input", "number"}, {"step", 1}, {"min", 1}},
                    {{"path", "mirrored"}, {"label", "Mirrored"}, {"input", "checkbox"}},
                    {{"path", "erase"}, {"label", "Erase"}, {"input", "checkbox"}}
                })}
            },
            {
                {"type", typeid(FireworksEffect).name()},
                {"label", "Fireworks"},
                {"defaults", json{
                    {"maxSpeed", 175.0},
                    {"newParticleProbability", 1.0},
                    {"particlePreignitionTime", 0.0},
                    {"particleIgnition", 0.2},
                    {"particleHoldTime", 0.0},
                    {"particleFadeTime", 2.0},
                    {"particleSize", 1.0}
                }},
                {"fields", json::array({
                    {{"path", "maxSpeed"}, {"label", "Max Speed"}, {"input", "number"}, {"step", 1}},
                    {{"path", "newParticleProbability"}, {"label", "Spawn Probability"}, {"input", "number"}, {"step", 0.05}},
                    {{"path", "particlePreignitionTime"}, {"label", "Pre-Ignition"}, {"input", "number"}, {"step", 0.05}},
                    {{"path", "particleIgnition"}, {"label", "Ignition"}, {"input", "number"}, {"step", 0.05}},
                    {{"path", "particleHoldTime"}, {"label", "Hold Time"}, {"input", "number"}, {"step", 0.05}},
                    {{"path", "particleFadeTime"}, {"label", "Fade Time"}, {"input", "number"}, {"step", 0.05}},
                    {{"path", "particleSize"}, {"label", "Particle Size"}, {"input", "number"}, {"step", 0.1}, {"min", 0.1}}
                })}
            },
            {
                {"type", typeid(MP4PlaybackEffect).name()},
                {"label", "MP4 Playback"},
                {"defaults", json{{"filePath", "./media/mp4/goldendollars.mp4"}}},
                {"fields", json::array({
                    {{"path", "filePath"}, {"label", "File Path"}, {"input", "text"}}
                })}
            }
        })}
    };
}

} // namespace

nlohmann::json BuildEffectCatalog()
{
    static const auto catalog = CreateEffectCatalogJson();

    return catalog;
}

} // namespace ndscpp::api
