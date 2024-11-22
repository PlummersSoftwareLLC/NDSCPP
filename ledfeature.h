#pragma once
using namespace std;
using namespace std::chrono;

#include <nlohmann/json.hpp>

// LEDFeature
//
// Represents one rectangular section of the canvas and is responsiible for producing the
// color data frames for that section of the canvas.  The LEDFeature is associated with a
// specific Canvas object, and it retrieves the pixel data from the Canvas to produce the
// data frame.  The LEDFeature is also responsible for producing the data frame in the
// format that the ESP32 expects.

#include "interfaces.h"
#include "utilities.h"
#include "canvas.h"
#include <string>

class LEDFeature : public ILEDFeature
{
public:
    LEDFeature(shared_ptr<ICanvas> canvas,
               const string &hostName,
               const string &friendlyName,
               uint16_t port,
               uint32_t width,
               uint32_t height = 1,
               uint32_t offsetX = 0,
               uint32_t offsetY = 0,
               bool reversed = false,
               uint8_t channel = 0,
               bool redGreenSwap = false)
        : _canvas(canvas),
          _hostName(hostName),
          _friendlyName(friendlyName),
          _port(port),
          _width(width),
          _height(height),
          _offsetX(offsetX),
          _offsetY(offsetY),
          _reversed(reversed),
          _channel(channel),
          _redGreenSwap(redGreenSwap)
    {
    }

    // Accessor methods
    uint32_t Width() const override { return _width; }
    uint32_t Height() const override { return _height; }
    const string &HostName() const override { return _hostName; }
    const string &FriendlyName() const override { return _friendlyName; }
    uint16_t Port() const override { return _port; }
    uint32_t OffsetX() const override { return _offsetX; }
    uint32_t OffsetY() const override { return _offsetY; }
    bool Reversed() const override { return _reversed; }
    uint8_t Channel() const override { return _channel; }
    bool RedGreenSwap() const override { return _redGreenSwap; }

    // Canvas association
    void SetCanvas(shared_ptr<ICanvas> canvas) { _canvas = canvas; }
    shared_ptr<ICanvas> GetCanvas() const { return _canvas; }

    // Data retrieval
    vector<uint8_t> GetPixelData() const override
    {
        if (!_canvas)
            throw runtime_error("LEDFeature must be associated with a canvas to retrieve pixel data.");

        const auto& graphics = _canvas->Graphics();

        // If the feature matches the canvas in size and position, directly return the canvas pixel data
        if (_width == graphics.Width() && _height == graphics.Height() && _offsetX == 0 && _offsetY == 0)
        {
            return Utilities::ConvertPixelsToByteArray(
                graphics.GetPixels(), 
                _reversed,
                _redGreenSwap
            );
        }

        // Otherwise, manually extract the feature's pixel data
        vector<CRGB> featurePixels;

        for (uint32_t y = 0; y < _height; ++y)
        {
            for (uint32_t x = 0; x < _width; ++x)
            {
                // Map feature (local) coordinates to canvas (global) coordinates
                uint32_t canvasX = x + _offsetX;
                uint32_t canvasY = y + _offsetY;

                // Ensure we don't exceed canvas boundaries
                if (canvasX < graphics.Width() && canvasY < graphics.Height())
                    featurePixels.push_back(graphics.GetPixel(canvasX, canvasY));
                else
                    featurePixels.push_back(CRGB::Magenta); // Out-of-bounds pixels are defaulted to Magenta
            }
        }

        return Utilities::ConvertPixelsToByteArray(featurePixels, _reversed, _redGreenSwap);
    }


    vector<uint8_t> GetDataFrame() const override
    {
        // Calculate epoch time
        auto now = system_clock::now();
        auto epoch = duration_cast<microseconds>(now.time_since_epoch()).count();
        uint64_t seconds = epoch / 1'000'000;
        uint64_t microseconds = epoch % 1'000'000;

        auto pixelData = GetPixelData();

        return Utilities::CombineByteArrays(Utilities::WORDToBytes(3),
                                            Utilities::WORDToBytes(_channel),
                                            Utilities::DWORDToBytes(_width * _height),
                                            Utilities::ULONGToBytes(seconds),
                                            Utilities::ULONGToBytes(microseconds),
                                            pixelData);
    }

private:
    string _hostName;
    string _friendlyName;
    uint16_t    _port;
    uint32_t    _width;
    uint32_t    _height;
    uint32_t    _offsetX;
    uint32_t    _offsetY;
    bool        _reversed;
    uint8_t     _channel;
    bool        _redGreenSwap;
    shared_ptr<ICanvas> _canvas; // Associated canvas
};
