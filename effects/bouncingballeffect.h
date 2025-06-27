#pragma once

using namespace std;
using namespace std::chrono;

#include "../interfaces.h"
#include "../ledeffectbase.h"
#include "../pixeltypes.h"
#include <vector>
#include <random>
#include <cmath>

class BouncingBallEffect : public LEDEffectBase
{
private:
    size_t _ballCount;
    size_t _ballSize;
    bool _mirrored;
    bool _erase;

    vector<double> _clockTimeSinceLastBounce;
    vector<double> _timeSinceLastBounce;
    vector<float> _height;
    vector<float> _impactVelocity;
    vector<float> _dampening;
    vector<CRGB> _colors;

    static constexpr float Gravity = -0.25f;
    static constexpr float StartHeight = 1.0f;
    static constexpr float ImpactVelocityStart = Utilities::constexpr_sqrt(-2.0f * Gravity * StartHeight);
    static constexpr std::array<CRGB, 7> BallColors = {{
        CRGB::Green, CRGB::Red, CRGB::Blue, CRGB::Orange,
        CRGB::Purple, CRGB::Yellow, CRGB::Indigo
    }};


public:
    BouncingBallEffect(const string& name, size_t ballCount = 5, size_t ballSize = 1, bool mirrored = true, bool erase = true)
        : LEDEffectBase(name), _ballCount(ballCount), _ballSize(ballSize), _mirrored(mirrored), _erase(erase)
    {
    }

    void Start(ICanvas& canvas) override
    {
        size_t length = canvas.Graphics().Width(); // Assuming 1D for simplicity; adapt for 2D if needed

        _clockTimeSinceLastBounce.resize(_ballCount);
        _timeSinceLastBounce.resize(_ballCount);
        _height.resize(_ballCount);
        _impactVelocity.resize(_ballCount);
        _dampening.resize(_ballCount);
        _colors.resize(_ballCount);

        for (size_t i = 0; i < _ballCount; ++i)
        {
            _height[i] = StartHeight;
            _impactVelocity[i] = ImpactVelocityStart;
            _clockTimeSinceLastBounce[i] = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
            _dampening[i] = 1.0f - static_cast<float>(i) / powf(static_cast<float>(_ballCount), 2.0f);
            _timeSinceLastBounce[i] = 0;
            _colors[i] = BallColors[i % BallColors.size()];
        }
    }

    void Update(ICanvas& canvas, milliseconds deltaTime) override
    {
        auto& graphics = canvas.Graphics();
        size_t length = graphics.Width();

        // Erase the canvas
        if (_erase)
        {
            graphics.Clear(CRGB::Black);
        }
        else
        {
            for (size_t j = 0; j < length; ++j)
                if (rand() % 10 > 5)
                    graphics.FadePixelToBlackBy(j, 0, 50);
        }

        // Draw each ball
        for (size_t i = 0; i < _ballCount; ++i)
        {
            _timeSinceLastBounce[i] += deltaTime.count() / 1000.0; // Convert to seconds
            _height[i] = 0.5f * Gravity * powf(_timeSinceLastBounce[i], 2.0f) + _impactVelocity[i] * _timeSinceLastBounce[i];

            if (_height[i] < 0)
            {
                _height[i] = 0;
                _impactVelocity[i] *= _dampening[i];
                _timeSinceLastBounce[i] = 0;

                if (_impactVelocity[i] < 0.5f * ImpactVelocityStart)
                {
                    _impactVelocity[i] = ImpactVelocityStart;
                }
            }

            float position = _height[i] * (length - 1) / StartHeight;
            graphics.SetPixel(position, 0, _colors[i]);

            if (_mirrored)
            {
                graphics.SetPixel(length - 1 - position, 0, _colors[i]);
            }
        }
    }

    friend inline void to_json(nlohmann::json& j, const BouncingBallEffect& effect);
    friend inline void from_json(const nlohmann::json& j, shared_ptr<BouncingBallEffect>& effect);
};

inline void to_json(nlohmann::json& j, const BouncingBallEffect& effect) 
{
    j = {
        {"name", effect.Name()},
        {"ballCount", effect._ballCount},
        {"ballSize", effect._ballSize},
        {"mirrored", effect._mirrored},
        {"erase", effect._erase}
    };
}

inline void from_json(const nlohmann::json& j, shared_ptr<BouncingBallEffect>& effect) 
{
    effect = make_shared<BouncingBallEffect>(
        j.at("name").get<string>(),
        j.at("ballCount").get<size_t>(),
        j.at("ballSize").get<size_t>(),
        j.at("mirrored").get<bool>(),
        j.at("erase").get<bool>()
    );
}
