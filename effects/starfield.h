#pragma once

using namespace std;
using namespace std::chrono;

#include "../interfaces.h"
#include "../ledeffectbase.h"
#include "../pixeltypes.h"
#include <vector>
#include <random>
#include <cmath>

class StarfieldEffect : public LEDEffectBase
{
private:
    struct Star
    {
        double x, y;                                    // Current position
        double dx, dy;                                  // Velocity components
        uint8_t brightness;                             // Star brightness
        CRGB color;                                     // Star color (random or white)
    };

    vector<Star> _stars;                                // Active stars
    int _starCount;                                     // Number of stars
    mt19937 _rng;                                       // Random number generator
    uniform_real_distribution<double> _speedDist;       // Speed distribution
    uniform_real_distribution<double> _directionDist;   // Direction distribution
    uniform_int_distribution<uint8_t> _brightnessDist;  // Brightness distribution
    uniform_int_distribution<int> _colorChanceDist;     // Determines if a star gets a random color
    uniform_int_distribution<uint8_t> _colorComponentDist; // Random component for saturated colors

    int _centerX, _centerY;                             // Center of the canvas

public:
    StarfieldEffect(const string& name, int starCount = 100)
        : LEDEffectBase(name), _starCount(starCount), _rng(random_device{}()),
          _speedDist(5.0, 20.0),                        // Increased speed for hyperspace effect
          _directionDist(0, 2 * M_PI),                  // Full 360° angular range
          _brightnessDist(28, 255), _colorChanceDist(0, 1),
          _colorComponentDist(0, 255), _centerX(0), _centerY(0)
    {
    }

    void Start(ICanvas& canvas) override
    {
        _centerX = canvas.Graphics().Width() / 2;
        _centerY = canvas.Graphics().Height() / 2;

        _stars.clear();
        for (int i = 0; i < _starCount; ++i)
        {
            _stars.push_back(CreateRandomStar());
        }
        canvas.Graphics().Clear(CRGB::Black);
    }

    void Update(ICanvas& canvas, microseconds deltaTime) override
    {
        auto& graphics = canvas.Graphics();
        graphics.FadeFrameBy(32);

        double timeFactor = deltaTime.count() / 1000000.0; // Convert delta time to seconds

        for (auto& star : _stars)
        {
            // Update position based on velocity and time
            const auto xScale = (double)graphics.Width() / (double)max(1u, graphics.Height()) / 2.0;
            star.x += star.dx * timeFactor * xScale;
            star.y += star.dy * timeFactor;

            // If the star is out of bounds, respawn it
            if (star.x < 0 || star.x >= graphics.Width() || star.y < 0 || star.y >= graphics.Height())
            {
                star = CreateRandomStar();
            }

            // Draw the star
            int ix = static_cast<int>(star.x);
            int iy = static_cast<int>(star.y);
            graphics.SetPixel(ix, iy, CRGB(star.color.r, star.color.g, star.color.b));
        }
    }

private:
    Star CreateRandomStar()
    {
        // Generate a random speed and direction
        double speed = _speedDist(_rng);
        double angle = _directionDist(_rng);

        // Compute velocity components
        double dx = speed * cos(angle);
        double dy = speed * sin(angle);

        // Determine color: 50% chance for random saturated color, otherwise white
        CRGB color;
        if (_colorChanceDist(_rng) == 0)
        {
            uint8_t red = _colorComponentDist(_rng);
            uint8_t green = _colorComponentDist(_rng);
            uint8_t blue = _colorComponentDist(_rng);

            // Make sure one component is maxed for a fully saturated color
            int maxComponent = max({red, green, blue});
            if (maxComponent == red)
                red = 255;
            else if (maxComponent == green)
                green = 255;
            else
                blue = 255;

            color = CRGB(red, green, blue);
        }
        else
        {
            // White color
            color = CRGB(255, 255, 255);
        }

        return Star{
            static_cast<double>(_centerX),
            static_cast<double>(_centerY),
            dx,
            dy,
            _brightnessDist(_rng),
            color
        };
    }

    friend inline void to_json(nlohmann::json& j, const StarfieldEffect & effect);
    friend inline void from_json(const nlohmann::json& j, shared_ptr<StarfieldEffect>& effect);
};

inline void to_json(nlohmann::json& j, const StarfieldEffect & effect)
{   
    j = {
        {"name", effect.Name()},
        {"starCount", effect._starCount}
    };
}

inline void from_json(const nlohmann::json& j, shared_ptr<StarfieldEffect>& effect)
{
    effect = make_shared<StarfieldEffect>(
        j.at("name").get<string>(),
        j.value("starCount", 100)
    );
}