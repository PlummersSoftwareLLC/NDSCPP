#pragma once
using namespace std;
using namespace std::chrono;

#include "effects/colorwaveeffect.h"
#include "effects/fireworkseffect.h"
#include "effects/misceffects.h"
#include "effects/paletteeffect.h"
#include "effects/starfield.h"
#include "effects/videoeffect.h"
#include "effects/bouncingballeffect.h"

// EffectsManager
//
// Manages a collection of ILEDEffect objects.  The EffectsManager is responsible for
// starting and stopping the effects, and for switching between them.  The EffectsManager
// can also be used to clear all effects.

#include "interfaces.h"
#include <vector>
#include <mutex>

class EffectsManager : public IEffectsManager
{
    uint16_t      _fps;
    int           _currentEffectIndex; // Index of the current effect
    atomic<bool>  _running;
    bool          _wantsToRun;
    mutable mutex _effectsMutex;  // Add mutex as member
    vector<shared_ptr<ILEDEffect>> _effects;
    thread        _workerThread;

public:
    EffectsManager(uint16_t fps) : _fps(fps), _currentEffectIndex(-1), _wantsToRun(true), _running(false) // No effect selected initially
    {
    }

    ~EffectsManager()
    {
        Stop(); // Ensure the worker thread is stopped when the manager is destroyed
    }

    void SetFPS(uint16_t fps) override
    {
        _fps = fps;
    }

    uint16_t GetFPS() const override
    {
        return _fps;
    }

    size_t GetCurrentEffect() const override
    {
        return _currentEffectIndex;
    }

    size_t EffectCount() const override
    {
        return _effects.size();
    }

    vector<shared_ptr<ILEDEffect>> Effects() const override
    {
        lock_guard lock(_effectsMutex);
        return _effects;
    }

    // Add an effect to the manager
    void AddEffect(shared_ptr<ILEDEffect> effect) override
    {
        lock_guard lock(_effectsMutex);

        if (!effect)
            throw invalid_argument("Cannot add a null effect.");
        _effects.push_back(effect);

        // Automatically set the first effect as current if none is selected
        if (_currentEffectIndex == -1)
            _currentEffectIndex = 0;
    }

    // Remove an effect from the manager
    void RemoveEffect(shared_ptr<ILEDEffect> &effect) override
    {
        lock_guard lock(_effectsMutex);

        if (!effect)
            throw invalid_argument("Cannot remove a null effect.");

        auto it = remove(_effects.begin(), _effects.end(), effect);
        if (it != _effects.end())
        {
            auto index = distance(_effects.begin(), it);
            _effects.erase(it);

            // Adjust the current effect index
            if (index <= _currentEffectIndex)
                _currentEffectIndex = (_currentEffectIndex > 0) ? _currentEffectIndex - 1 : -1;

            // If no effects remain, reset the current index
            if (_effects.empty())
                _currentEffectIndex = -1;
        }
    }

    // Start the current effect
    void StartCurrentEffect(ICanvas &canvas) override
    {
        if (_running && IsEffectSelected())
            _effects[_currentEffectIndex]->Start(canvas);
    }

    void SetCurrentEffect(size_t index, ICanvas &canvas) override
    {
        if (index >= _effects.size())
            throw out_of_range("Effect index out of range.");

        _currentEffectIndex = index;

        StartCurrentEffect(canvas);
    }

    // Update the current effect and render it to the canvas
    void UpdateCurrentEffect(ICanvas &canvas, milliseconds millisDelta) override
    {
        if (_running && IsEffectSelected())
            _effects[_currentEffectIndex]->Update(canvas, millisDelta);
    }

    // Switch to the next effect
    void NextEffect() override
    {
        if (!_effects.empty())
            _currentEffectIndex = (_currentEffectIndex + 1) % _effects.size();
    }

    // Switch to the previous effect
    void PreviousEffect() override
    {
        if (!_effects.empty())
            _currentEffectIndex = (_currentEffectIndex == 0) ? _effects.size() - 1 : _currentEffectIndex - 1;
    }

    // Get the name of the current effect
    string CurrentEffectName() const override
    {
        if (IsEffectSelected())
            return _effects[_currentEffectIndex]->Name();
        return "No Effect Selected";
    }

    // Clear all effects

    void ClearEffects() override
    {
        lock_guard lock(_effectsMutex);
        _effects.clear();
        _currentEffectIndex = -1;
    }


    bool WantsToRun() const override
    {
        return _wantsToRun;
    }

    void WantToRun(bool wantsToRun) override
    {
        _wantsToRun = wantsToRun;
    }

    bool IsRunning() const override
    {
        return _running;
    }

    // Start the worker thread to update effects

    void Start(ICanvas &canvas) override
    {
        logger->debug("Starting effects manager with {} effects at {} FPS", _effects.size(), _fps);

        if (_running.exchange(true))
            return; // Already running

        _workerThread = thread([this, &canvas]()
        {
            auto frameDuration = 1000ms / _fps; // Target duration per frame
            auto nextFrameTime = steady_clock::now();
            constexpr auto bUseCompression = true;

            // Starting the canvas should start the effect at least one time, as many effects
            // have one-time setup in their Start() method
            
            StartCurrentEffect(canvas);

            while (_running)
            {
                {
                    lock_guard lock(_effectsMutex);

                    // Update the effects and enqueue frames
                    UpdateCurrentEffect(canvas, frameDuration);
                    for (const auto &feature : canvas.Features())
                    {
                        auto frame = feature->GetDataFrame();
                        if (bUseCompression)
                        {
                            auto compressedFrame = feature->Socket()->CompressFrame(frame);
                            feature->Socket()->EnqueueFrame(std::move(compressedFrame));
                        }
                        else
                        {
                            feature->Socket()->EnqueueFrame(std::move(frame));
                        }
                    }
                }
                
                // We wait here while periodically checking _running
                
                auto now = steady_clock::now();
                while (now < nextFrameTime && _running) {
                    this_thread::sleep_for(min(steady_clock::duration(10ms), nextFrameTime - now));
                    now = steady_clock::now(); // Update 'now' to avoid an infinite loop
                }

                // Set the next frame target
                nextFrameTime += frameDuration;
            } });
    }

    // Stop the worker thread
    void Stop() override
    {
        logger->debug("Stopping effects manager");
        if (!_running.exchange(false))
            return; // Not running

        if (_workerThread.joinable())
            _workerThread.join();
    }

    void SetEffects(vector<shared_ptr<ILEDEffect>> effects) override
    {
        lock_guard lock(_effectsMutex);
        _effects = std::move(effects);
    }

    void SetCurrentEffectIndex(int index) override
    {
        _currentEffectIndex = index;
    }

private:
    bool IsEffectSelected() const
    {
        return _currentEffectIndex >= 0 && _currentEffectIndex < static_cast<int>(_effects.size());
    }
};

// Define type aliases for effect (de)serialization functions for legibility reasons
using EffectSerializer = function<void(nlohmann::json &, const ILEDEffect &)>;
using EffectDeserializer = function<shared_ptr<ILEDEffect>(const nlohmann::json &)>;

// Factory function to create a pair of effect (de)serialization functions for a given type
template <typename T>
pair<string, pair<EffectSerializer, EffectDeserializer>> jsonPair()
{
    EffectSerializer serializer = [](nlohmann::json &j, const ILEDEffect &effect)
    {
        to_json(j, dynamic_cast<const T &>(effect));
    };

    EffectDeserializer deserializer = [](const nlohmann::json &j)
    {
        return j.get<shared_ptr<T>>();
    };

    return make_pair(typeid(T).name(), make_pair(serializer, deserializer));
}

// Map with effect (de)serialization functions

static const map<string, pair<EffectSerializer, EffectDeserializer>> to_from_json_map =
{
        jsonPair<BouncingBallEffect>(),
        jsonPair<ColorWaveEffect>(),
        jsonPair<FireworksEffect>(),
        jsonPair<SolidColorFill>(),
        jsonPair<PaletteEffect>(),
        jsonPair<StarfieldEffect>(),
        jsonPair<MP4PlaybackEffect>()
};

// Dynamically serialize an effect to JSON based on its actual type

inline void to_json(nlohmann::json &j, const ILEDEffect &effect)
{
    std::string type = typeid(effect).name();
    auto it = to_from_json_map.find(type);
    if (it == to_from_json_map.end())
    {
        logger->error("Unknown effect type for serialization: {}", type);
        throw runtime_error("Unknown effect type for serialization: " + type);
    }
    it->second.first(j, effect);
    j["type"] = type;
}

// Dynamically deserialize an effect from JSON based on its indicated type
// and return it on the unique pointer out reference

// ILEDEffect <-- JSON

inline void from_json(const nlohmann::json &j, shared_ptr<ILEDEffect> & effect)
{
    auto it = to_from_json_map.find(j["type"]);
    if (it == to_from_json_map.end())
    {
        logger->error("Unknown effect type for deserialization: {}, replacing with magenta fill", j["type"].get<string>());
        effect = make_shared<SolidColorFill>("Unknown Effect Type", CRGB::Magenta);
        return;
    }

    effect = it->second.second(j);

    // Deserialize schedule if present
    if (j.contains("schedule")) {
        Schedule schedule = j["schedule"].get<Schedule>();
        effect->SetSchedule(schedule);
    }
}

// IEffectsManager <-- JSON

inline void to_json(nlohmann::json &j, const IEffectsManager &manager)
{
    j = 
    {
        {"fps", manager.GetFPS()},
        {"currentEffectIndex", manager.GetCurrentEffect()},
        {"running", manager.IsRunning()}
    };
        
    for (const auto &effect : manager.Effects())
        j["effects"].push_back(*effect);
};

// IEffectManager --> JSON

inline void from_json(const nlohmann::json &j, IEffectsManager &manager)
{
    manager.SetFPS(j.at("fps").get<uint16_t>());
    manager.SetEffects(j.at("effects").get<vector<shared_ptr<ILEDEffect>>>());
    manager.SetCurrentEffectIndex(j.at("currentEffectIndex").get<int>());
    
    // We deserialize the running state to a running *preference*. Directly starting the manager after
    // deserialization could create problems, and without having the canvas we can't start it anyway.
    if (j.contains("running"))
        manager.WantToRun(j.at("running").get<bool>());
}
