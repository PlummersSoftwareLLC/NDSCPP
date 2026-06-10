#pragma once
using namespace std;
using namespace chrono;

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

class LEDFeature : public ILEDFeature
{
    const ICanvas   * _canvas = nullptr; // Associated canvas
    uint32_t    _width;
    uint32_t    _height;
    uint32_t    _offsetX;
    uint32_t    _offsetY;
    bool        _reversed;
    uint8_t     _channel;
    bool        _redGreenSwap;
    uint32_t    _clientBufferCount;
    shared_ptr<ISocketChannel> _ptrSocketChannel;
    static atomic<uint32_t> _nextId;
    uint32_t _id;    

public:
    LEDFeature(const string & hostName,
               const string & friendlyName,
               uint16_t       port,
               uint32_t       width,
               uint32_t       height = 1,
               uint32_t       offsetX = 0,
               uint32_t       offsetY = 0,
               bool           reversed = false,
               uint8_t        channel = 0,
               bool           redGreenSwap = false,
               uint32_t       clientBufferCount = 24)
        : _width(width),
          _height(height),
          _offsetX(offsetX),
          _offsetY(offsetY),
          _reversed(reversed),
          _channel(channel),
          _redGreenSwap(redGreenSwap),
          _clientBufferCount(clientBufferCount),
          _id(_nextId++)
    {
        _ptrSocketChannel = make_shared<SocketChannel>(hostName, friendlyName, port);
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

    void SetCanvas(const ICanvas * canvas) override
    {
        if (_canvas)
            throw runtime_error("Canvas is already set for this LEDFeature.");

        _canvas = canvas;
    }

    double TimeOffset () const override
    {
        // 0.0s lead forces the client to display frames immediately.
        // This is the safest setting to verify the protocol is working.
        return 0.0;
    }
    
    virtual shared_ptr<ISocketChannel> Socket() override 
    {
        return _ptrSocketChannel;
    }

    virtual const shared_ptr<ISocketChannel> Socket() const override 
    {
        return _ptrSocketChannel;
    }
    
    vector<uint8_t> GetPixelData() const override 
    {
        static_assert(sizeof(CRGB) == 3, "CRGB must be 3 bytes in size for this code to work.");

        if (!_canvas)
            throw runtime_error("LEDFeature must be associated with a canvas to retrieve pixel data.");

        const auto& graphics = _canvas->Graphics();

        // Fast path for full canvas. 
        if (__builtin_expect(_width == graphics.Width() && _height == graphics.Height() && _offsetX == 0 && _offsetY == 0, 1))
            return Utilities::ConvertPixelsToByteArray(graphics.GetPixels(), _reversed, _redGreenSwap);

        // Pre-calculate the final buffer size (3 bytes per pixel)
        vector<uint8_t> result(_width * _height * sizeof(CRGB));
        
        for (uint32_t y = 0; y < _height; ++y)
        {
            for (uint32_t x = 0; x < _width; ++x)
            {
                uint32_t canvasX = x + _offsetX;
                uint32_t canvasY = y + _offsetY;
                
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
                    result[byteIndex] = 0xFF;
                    result[byteIndex + 1] = 0x00;
                    result[byteIndex + 2] = 0xFF;
                }
            }
        }

        if (_reversed)
        {
            const size_t numPixels = result.size() / 3;
            for (size_t i = 0; i < numPixels / 2; ++i) {
                size_t front = i * 3;
                size_t back = (numPixels - 1 - i) * 3;
                swap(result[front], result[back]);
                swap(result[front + 1], result[back + 1]);
                swap(result[front + 2], result[back + 2]);
            }
        }

        return result;
    }

    vector<uint8_t> GetDataFrame(system_clock::time_point targetTime) const override
    {
        // Standard Type 3 NightDriver Protocol Header
        auto futureTime = targetTime + microseconds(static_cast<long long>(TimeOffset() * 1000000.0));
        auto epoch = duration_cast<microseconds>(futureTime.time_since_epoch()).count();
        
        uint64_t seconds = epoch / 1'000'000;
        uint64_t microseconds = epoch % 1'000'000;

        auto pixelData = GetPixelData();

        return Utilities::CombineByteArrays(
            Utilities::WORDToBytes(3),
            Utilities::WORDToBytes(_channel),
            Utilities::DWORDToBytes(_width * _height),
            Utilities::ULONGToBytes(seconds),
            Utilities::ULONGToBytes(microseconds),
            std::move(pixelData));
    }
};

inline void to_json(nlohmann::json& j, const ILEDFeature & feature) 
{
    j = {
            {"id",                feature.Id()},
            {"hostName",          feature.Socket()->HostName()},
            {"friendlyName",      feature.Socket()->FriendlyName()},
            {"port",              feature.Socket()->Port()},
            {"width",             feature.Width()},
            {"height",            feature.Height()},
            {"offsetX",           feature.OffsetX()},
            {"offsetY",           feature.OffsetY()},
            {"reversed",          feature.Reversed()},
            {"channel",           feature.Channel()},
            {"redGreenSwap",      feature.RedGreenSwap()},
            {"clientBufferCount", feature.ClientBufferCount()},
            {"timeOffset",        feature.TimeOffset()},
            {"bytesPerSecond",    feature.Socket()->GetLastBytesPerSecond()},
            {"isConnected",       feature.Socket()->IsConnected()},
            {"queueDepth",        feature.Socket()->GetCurrentQueueDepth()},
            {"queueMaxSize",      feature.Socket()->GetQueueMaxSize()},
            {"reconnectCount",    feature.Socket()->GetReconnectCount()}
        };

    const auto &response = feature.Socket()->LastClientResponse();
    if (response.size == sizeof(ClientResponse))
        j["lastClientResponse"] = response;
}

inline void from_json(const nlohmann::json& j, shared_ptr<ILEDFeature> & feature) 
{
    feature = make_shared<LEDFeature>(
        j.at("hostName").get<string>(),
        j.at("friendlyName").get<string>(),
        j.value("port", uint16_t(49152)),
        j.at("width").get<uint32_t>(),
        j.value("height", uint32_t(1)),
        j.value("offsetX", uint32_t(0)),
        j.value("offsetY", uint32_t(0)),
        j.value("reversed", false),
        j.value("channel", uint8_t(0)),
        j.value("redGreenSwap", false),
        j.value("clientBufferCount", uint32_t(24))
    );
}
