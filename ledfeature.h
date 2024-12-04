#pragma once
using namespace std;
using namespace std::chrono;

#include "json.hpp"


// LEDFeature
//
// Represents one rectangular section of the canvas and is responsible for producing the
// color data frames for that section of the canvas.  The LEDFeature is associated with a
// specific Canvas object, and it retrieves the pixel data from the Canvas to produce the
// data frame.  The LEDFeature is also responsible for producing the data frame in the
// format that the ESP32 expects.

#include "interfaces.h"
#include "utilities.h"
#include "socketchannel.h"
#include "canvas.h"

class LEDFeature : public ILEDFeature
{
public:
    LEDFeature(const Canvas * canvas,
               const string & hostName,
               const string & friendlyName,
               uint16_t       port,
               uint32_t       width,
               uint32_t       height = 1,
               uint32_t       offsetX = 0,
               uint32_t       offsetY = 0,
               bool           reversed = false,
               uint8_t        channel = 0,
               bool           redGreenSwap = false,
               uint32_t       clientBufferCount = 8)
        : _canvas(canvas),
          _width(width),
          _height(height),
          _offsetX(offsetX),
          _offsetY(offsetY),
          _reversed(reversed),
          _channel(channel),
          _redGreenSwap(redGreenSwap),
          _clientBufferCount(clientBufferCount),
          _socketChannel(hostName, friendlyName, port),
          _id(_nextId++)
    {
    }

    uint32_t Id() const override 
    { 
        return _id; 
    }

    // Accessor methods
    uint32_t        Width()             const override { return _width; }
    uint32_t        Height()            const override { return _height; }
    uint32_t        OffsetX()           const override { return _offsetX; }
    uint32_t        OffsetY()           const override { return _offsetY; }
    bool            Reversed()          const override { return _reversed; }
    uint8_t         Channel()           const override { return _channel; }
    bool            RedGreenSwap()      const override { return _redGreenSwap; }
    uint32_t        ClientBufferCount() const override { return _clientBufferCount; }

    double TimeOffset () const override
    {
        constexpr auto kBufferFillRatio = 0.80;
        return(_clientBufferCount * kBufferFillRatio) / _canvas->Effects().GetFPS();
    }
    
    virtual ISocketChannel & Socket() override 
    {
        return _socketChannel;
    }

    virtual const ISocketChannel & Socket() const override 
    {
        return _socketChannel;
    }
    
    vector<uint8_t> GetPixelData() const override 
    {
        static_assert(sizeof(CRGB) == 3, "CRGB must be 3 bytes in size for this code to work.");

        if (!_canvas)
            throw runtime_error("LEDFeature must be associated with a canvas to retrieve pixel data.");

        const auto& graphics = _canvas->Graphics();

        // Fast path for full canvas.  We assume this is the default case and optimize for it by telling the compiler to expect it.
        if (__builtin_expect(_width == graphics.Width() && _height == graphics.Height() && _offsetX == 0 && _offsetY == 0, 1))
            return Utilities::ConvertPixelsToByteArray(graphics.GetPixels(), _reversed, _redGreenSwap);

        // Pre-calculate the final buffer size (3 bytes per pixel)
        vector<uint8_t> result(_width * _height * sizeof(CRGB));
        
        // Direct byte manipulation instead of intermediate CRGB vector
        for (uint32_t y = 0; y < _height; ++y)
        {
            for (uint32_t x = 0; x < _width; ++x)
            {
                uint32_t canvasX = x + _offsetX;
                uint32_t canvasY = y + _offsetY;
                
                // Calculate output position directly in bytes
                uint32_t byteIndex = (y * _width + x) * sizeof(CRGB);
                
                if (canvasX < graphics.Width() && canvasY < graphics.Height())
                {
                    const CRGB& pixel = graphics.GetPixel(canvasX, canvasY);
                    if (_redGreenSwap)
                    {
                        result[byteIndex] = pixel.g;
                        result[byteIndex + 1] = pixel.r;
                        result[byteIndex + 2] = pixel.b;
                    }
                    else 
                    {
                        result[byteIndex] = pixel.r;
                        result[byteIndex + 1] = pixel.g;
                        result[byteIndex + 2] = pixel.b;
                    }
                }
                else 
                {
                    // Magenta for out of bounds (0xFF, 0x00, 0xFF)
                    result[byteIndex] = 0xFF;
                    result[byteIndex + 1] = 0x00;
                    result[byteIndex + 2] = 0xFF;
                }
            }
        }

        if (_reversed)
        {
            // In-place reversal of RGB groups
            for (size_t i = 0; i < result.size() / 2; i += sizeof(CRGB))
            {
                size_t j = result.size() - i - sizeof(CRGB);
                std::swap(result[i], result[j]);
                std::swap(result[i + 1], result[j + 1]);
                std::swap(result[i + 2], result[j + 2]);
            }
        }

        return result;
    }

    vector<uint8_t> GetDataFrame() const override
    {
        // Calculate epoch time
        auto now = system_clock::now();
        auto epoch = duration_cast<microseconds>(now.time_since_epoch()).count();
        uint64_t seconds = epoch / 1'000'000 + TimeOffset();
        uint64_t microseconds = epoch % 1'000'000;

        auto pixelData = GetPixelData();

        return Utilities::CombineByteArrays(Utilities::WORDToBytes(3),
                                            Utilities::WORDToBytes(_channel),
                                            Utilities::DWORDToBytes(_width * _height),
                                            Utilities::ULONGToBytes(seconds),
                                            Utilities::ULONGToBytes(microseconds),
                                            std::move(pixelData));
    }

private:
    uint32_t    _width;
    uint32_t    _height;
    uint32_t    _offsetX;
    uint32_t    _offsetY;
    bool        _reversed;
    uint8_t     _channel;
    bool        _redGreenSwap;
    uint32_t    _clientBufferCount;
    const ICanvas   * _canvas; // Associated canvas
    SocketChannel _socketChannel;
    static atomic<uint32_t> _nextId;
    uint32_t _id;    
};


inline void from_json(const nlohmann::json& j, std::unique_ptr<ILEDFeature>& feature) 
{
    if (j.at("type").get<std::string>() != "LEDFeature") 
    {
        throw std::runtime_error("Invalid feature type in JSON");
    }

    feature = std::make_unique<LEDFeature>(
        nullptr,  // Canvas pointer needs to be set after deserialization
        j.at("hostName").get<std::string>(),
        j.at("friendlyName").get<std::string>(),
        j.at("port").get<uint16_t>(),
        j.at("width").get<uint32_t>(),
        j.value("height", 1u),
        j.value("offsetX", 0u),
        j.value("offsetY", 0u),
        j.value("reversed", false),
        j.value("channel", uint8_t(0)),
        j.value("redGreenSwap", false),
        j.value("clientBufferCount", 8u)
    );
}
