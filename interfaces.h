#pragma once

// Interfaces
// 
// This file contains the interfaces for the various classes in the project.  The interfaces
// are used to define the methods that must be implemented by the classes that use them.  This
// allows the classes to be decoupled from each other, and allows for easier testing and
// maintenance of the code.  It also presumably makes it easier in the future to interop
// with other languages, etc.

#include "pixeltypes.h"
#include <vector>
#include <map>
#include <chrono>
#include <string>
#include "json.hpp"

using namespace std;
using namespace chrono;

// ILEDGraphics 
//
// Represents a 2D drawing surface that can be used to render pixel data.  Provides methods for
// setting and getting pixel values, drawing shapes, and clearing the surface.

class ILEDGraphics 
{
public:
    virtual ~ILEDGraphics() = default;

    virtual const vector<CRGB> & GetPixels() const = 0;
    virtual uint32_t Width() const = 0;
    virtual uint32_t Height() const = 0;
    virtual void SetPixel(uint32_t x, uint32_t y, const CRGB& color) = 0;
    virtual void SetPixelsF(float fPos, float count, CRGB c, bool bMerge = false) = 0;
    virtual void FadePixelToBlackBy(uint32_t x, uint32_t y, float amount) = 0;
    virtual CRGB GetPixel(uint32_t x, uint32_t y) const = 0;
    virtual void Clear(const CRGB& color) = 0;
    virtual void FadeFrameBy(uint8_t dimAmount) = 0;
    virtual void FillRectangle(uint32_t x, uint32_t y, uint32_t width, uint32_t height, const CRGB& color) = 0;
    virtual void DrawLine(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, const CRGB& color) = 0;
    virtual void DrawCircle(uint32_t x, uint32_t y, uint32_t radius, const CRGB& color) = 0;
    virtual void FillCircle(uint32_t x, uint32_t y, uint32_t radius, const CRGB& color) = 0;
    virtual void DrawRectangle(uint32_t x, uint32_t y, uint32_t width, uint32_t height, const CRGB& color) = 0;
};

// ISchedule Interface
//
// Represents a class that determine whether the effect involved can/should be run at the current time

class ISchedule 
{
public:

    // Days of week as a bitmask using powers of 2.

    enum DayOfWeek : uint8_t 
    {
        Sunday    = 0x01,  // 1
        Monday    = 0x02,  // 2
        Tuesday   = 0x04,  // 4
        Wednesday = 0x08,  // 8
        Thursday  = 0x10,  // 16
        Friday    = 0x20,  // 32
        Saturday  = 0x40   // 64
    };

    virtual ~ISchedule() = default;

    // Setters for optional schedule properties.
    virtual void SetDaysOfWeek(uint8_t days) = 0;
    virtual void SetStartTime(const string& time) = 0;   // Format: "HH:MM:SS"
    virtual void SetStopTime(const string& time) = 0;    // Format: "HH:MM:SS"
    virtual void SetStartDate(const string& date) = 0;   // Format: "YYYY-MM-DD"
    virtual void SetStopDate(const string& date) = 0;    // Format: "YYYY-MM-DD"

    // Getters for schedule properties.
    virtual optional<uint8_t>     GetDaysOfWeek() const = 0;
    virtual optional<string> GetStartTime() const = 0;
    virtual optional<string> GetStopTime() const = 0;
    virtual optional<string> GetStartDate() const = 0;
    virtual optional<string> GetStopDate() const = 0;

    // Methods to manipulate individual days.
    virtual void AddDay(DayOfWeek day) = 0;
    virtual void RemoveDay(DayOfWeek day) = 0;
    virtual bool HasDay(DayOfWeek day) const = 0;

    // Determines if the schedule is currently active.
    // Optional fields (days, start/stop times, start/stop dates) are considered a match if not present.
    virtual bool IsActive() const = 0;
};

struct ClientResponse;
class ICanvas;

// ILEDEffect
//
// Defines lifecycle hooks (`Start` and `Update`) for applying visual effects on LED canvases.  

class ILEDEffect
{
public:
    virtual ~ILEDEffect() = default;

    // Get the name of the effect
    virtual const string& Name() const = 0;

    // Called when the effect starts
    virtual void Start(ICanvas& canvas) = 0;

    // Called to update the effect, given a canvas and timestamp
    virtual void Update(ICanvas& canvas, milliseconds deltaTime) = 0;

    virtual void SetSchedule(const shared_ptr<ISchedule> pSchedule) = 0;
    virtual const shared_ptr<ISchedule> GetSchedule() const = 0;
};

// IEffectsManager
//
// Manages a collection of LED effects, allowing for cycling through effects, starting and stopping them,
// and updating the current effect.  Provides methods for adding, removing, and clearing effects.

class IEffectsManager
{
public:
    virtual ~IEffectsManager() = default;
    
    virtual void AddEffect(shared_ptr<ILEDEffect> effect) = 0;
    virtual void RemoveEffect(shared_ptr<ILEDEffect> & effect) = 0;
    virtual void StartCurrentEffect(ICanvas& canvas) = 0;
    virtual void SetCurrentEffect(size_t index, ICanvas& canvas) = 0;
    virtual size_t GetCurrentEffect() const = 0;
    virtual size_t EffectCount() const = 0;
    virtual vector<shared_ptr<ILEDEffect>> Effects() const = 0;
    virtual void UpdateCurrentEffect(ICanvas& canvas, milliseconds millisDelta) = 0;
    virtual void NextEffect() = 0;
    virtual void PreviousEffect() = 0;
    virtual string CurrentEffectName() const = 0;
    virtual void ClearEffects() = 0;
    virtual bool WantsToRun() const = 0;
    virtual void WantToRun(bool wantsToRun) = 0;
    virtual bool IsRunning() const = 0;
    virtual void Start(ICanvas& canvas) = 0;
    virtual void Stop() = 0;
    virtual void SetFPS(uint16_t fps) = 0;
    virtual uint16_t GetFPS() const = 0;
    virtual void SetEffects(vector<shared_ptr<ILEDEffect>> effects) = 0;
    virtual void SetCurrentEffectIndex(int index) = 0;    
};

// ISocketChannel
//
// Defines a communication protocol for managing socket connections and sending data to a server.  
// Provides methods for enqueuing frames, retrieving connection status, and tracking performance metrics.

class ISocketChannel
{
public:
    virtual ~ISocketChannel() = default;

    // Accessors for channel details
    virtual const string& HostName() const = 0;
    virtual const string& FriendlyName() const = 0;
    virtual uint16_t Port() const = 0;

    virtual uint32_t Id() const = 0;

    // Data transfer methods
    virtual bool EnqueueFrame(vector<uint8_t>&& frameData) = 0;
    virtual vector<uint8_t> CompressFrame(const vector<uint8_t>& data) = 0;

    // Connection status
    virtual bool IsConnected() const = 0;
    virtual uint64_t GetLastBytesPerSecond() const = 0;
    virtual ClientResponse LastClientResponse() const = 0;
    virtual uint32_t GetReconnectCount() const = 0;
    virtual size_t GetCurrentQueueDepth() const = 0;
    virtual size_t GetQueueMaxSize() const = 0;

    // Start and stop operations
    virtual void Start() = 0;
    virtual void Stop() = 0;
};

// ILEDFeature
//
// Represents a 2D collection of LEDs with positioning, rendering, and configuration capabilities.  
// Provides APIs for interacting with its parent canvas and retrieving its assigned color data.

class ILEDFeature 
{
public:
    virtual ~ILEDFeature() = default;

    virtual uint32_t Id() const = 0;

    // Accessor methods
    virtual uint32_t Width() const = 0;
    virtual uint32_t Height() const = 0;
    virtual uint32_t OffsetX() const = 0;
    virtual uint32_t OffsetY() const = 0;
    virtual bool     Reversed() const = 0;
    virtual uint8_t  Channel() const = 0;
    virtual bool     RedGreenSwap() const = 0;
    virtual uint32_t ClientBufferCount() const = 0;
    virtual double   TimeOffset () const = 0;

    // Canvas association
    virtual void SetCanvas(const ICanvas * canvas) = 0;

    // Data retrieval
    virtual vector<uint8_t> GetPixelData() const = 0;
    virtual vector<uint8_t> GetDataFrame() const = 0;    

    virtual shared_ptr<ISocketChannel> Socket() = 0;
    virtual const shared_ptr<ISocketChannel> Socket() const = 0;

};

// ICanvas
//
// Represents a 2D drawing surface that manages LED features and provides rendering capabilities.  
// Can contain multiple `ILEDFeature` instances, with features mapped to specific regions of the canvas

class ICanvas 
{
public:
    virtual ~ICanvas() = default;
    
    virtual uint32_t Id() const = 0;
    virtual uint32_t SetId(uint32_t id) = 0;
    virtual string Name() const = 0;
    virtual uint32_t AddFeature(shared_ptr<ILEDFeature> feature) = 0;
    virtual bool RemoveFeatureById(uint16_t featureId) = 0;

    virtual vector<shared_ptr<ILEDFeature>>  Features() = 0;
    virtual const vector<shared_ptr<ILEDFeature>>  Features() const = 0;


    virtual ILEDGraphics & Graphics() = 0;
    virtual const ILEDGraphics& Graphics() const = 0;

    virtual IEffectsManager & Effects() = 0;
    virtual const IEffectsManager & Effects() const = 0;
};

class IController
{
public:
    virtual ~IController() = default;

    virtual void Connect() = 0;
    virtual void Disconnect() = 0;
    virtual void Start(bool respectWantsToRun = false) = 0;
    virtual void Stop() = 0;

    virtual void WriteToFile(const string& filePath) const = 0;

    virtual uint16_t GetPort() const = 0;
    virtual void     SetPort(uint16_t port) = 0;

    virtual vector<shared_ptr<ICanvas>> Canvases() const = 0;
    virtual uint32_t AddCanvas(shared_ptr<ICanvas> ptrCanvas) = 0;
    virtual bool DeleteCanvasById(uint32_t id) = 0;
    virtual bool UpdateCanvas(shared_ptr<ICanvas> ptrCanvas) = 0;
    virtual bool AddFeatureToCanvas(uint16_t canvasId, shared_ptr<ILEDFeature> feature) = 0;
    virtual void RemoveFeatureFromCanvas(uint16_t canvasId, uint16_t featureId) = 0;
    virtual shared_ptr<ICanvas> GetCanvasById(uint16_t id) const = 0;
    virtual const shared_ptr<ISocketChannel> GetSocketById(uint16_t id) const = 0;
    virtual vector<shared_ptr<ISocketChannel>> GetSockets() const = 0;
};