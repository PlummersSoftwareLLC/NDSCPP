#pragma once
using namespace std;
using namespace chrono;

#include "effects/colorwaveeffect.h"
#include "effects/fireworkseffect.h"
#include "effects/misceffects.h"
#include "effects/paletteeffect.h"
#include "effects/starfield.h"
#include "effects/videoeffect.h"
#include "effects/bouncingballeffect.h"
#include "effects/auroraeffect.h"

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
    mutable recursive_mutex _effectsMutex;  // Add recursive_mutex as member
    vector<shared_ptr<ILEDEffect>> _effects;
    thread        _workerThread;
    bool          _lastScheduleState = true; // Track last schedule state to detect transitions

public:
    EffectsManager(uint16_t fps) : _fps(fps), _currentEffectIndex(-1), _wantsToRun(true), _running(false), _lastScheduleState(true) // No effect selected initially
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
    void UpdateCurrentEffect(ICanvas &canvas, microseconds microsDelta) override
    {
        if (IsEffectSelected())
            _effects[_currentEffectIndex]->Update(canvas, microsDelta);
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
            const auto frameDurationNs = nanoseconds(1000000000LL / _fps); 
            const auto startTimeSteady = steady_clock::now();
            const auto startTimeSystem = system_clock::now();

            auto lastFrameTimeSteady = startTimeSteady;
            auto lastHeartbeatTime = startTimeSteady;
            auto lastScheduleCheck = startTimeSteady;
            auto frameCount = 0LL;

            {
                lock_guard lock(_effectsMutex);
                StartCurrentEffect(canvas);
            }

            while (_running)
            {
                auto now = steady_clock::now();

                // When an effect is active, we check every frame. 
                // When no effect is active, we check every 500ms to save CPU.
                bool shouldCheckSchedule = _lastScheduleState || (now - lastScheduleCheck >= 500ms);

                int activeIndex = -1;
                if (shouldCheckSchedule)
                {
                    lastScheduleCheck = now;
                    lock_guard lock(_effectsMutex);
                    for (int i = 0; i < static_cast<int>(_effects.size()); ++i)
                    {
                        auto &effect = _effects[i];
                        if (effect->GetSchedule() == nullptr || effect->GetSchedule()->IsActive())
                        {
                            activeIndex = i;
                            break;
                        }
                    }
                }
                else
                {
                    // If we didn't check, assume the state hasn't changed from "inactive"
                    activeIndex = -1;
                }

                // Calculate the actual target timestamp for this packet based on the wall clock
                frameCount++;
                auto nextFrameTimeSteady = startTimeSteady + (frameDurationNs * frameCount);
                auto packetTimestamp = startTimeSystem + (frameDurationNs * frameCount);

                if (activeIndex != -1) 
                {
                    if (activeIndex != _currentEffectIndex)
                    {
                        lock_guard lock(_effectsMutex);
                        logger->info("Switching to effect '{}' based on schedule.", _effects[activeIndex]->Name());
                        _currentEffectIndex = activeIndex;
                        _effects[_currentEffectIndex]->Start(canvas);
                    }

                    {
                        lock_guard lock(_effectsMutex);
                        auto delta = duration_cast<microseconds>(now - lastFrameTimeSteady);
                        UpdateCurrentEffect(canvas, delta);
                    }

                    for (const auto &feature : canvas.Features())
                    {
                        feature->Socket()->EnqueueFrame(feature->GetDataFrame(time_point_cast<system_clock::duration>(packetTimestamp)));
                    }
                    _lastScheduleState = true;
                    lastHeartbeatTime = now;
                } 
                else 
                {
                    if (_lastScheduleState || (now - lastHeartbeatTime) >= 2s) {
                        {
                            lock_guard lock(_effectsMutex);
                            canvas.Graphics().Clear(CRGB::Black);
                        }
                        for (const auto &feature : canvas.Features())
                        {
                            feature->Socket()->EnqueueFrame(feature->GetDataFrame(time_point_cast<system_clock::duration>(packetTimestamp)));
                        }
                        
                        if (_lastScheduleState)
                            logger->info("No scheduled effects are active for canvas '{}'. Sending heartbeats.", canvas.Name());
                        
                        _lastScheduleState = false;
                        lastHeartbeatTime = now;
                    }
                }
                
                lastFrameTimeSteady = now;

                // Drift detection and re-sync
                auto actualTime = system_clock::now();
                auto drift = duration_cast<microseconds>(packetTimestamp - actualTime).count();
                if (abs(drift) > 200000) // 200ms in microseconds
                {
                    logger->debug("Canvas '{}' clock drift detected: {}us. Re-syncing.", canvas.Name(), drift);
                    // To re-sync, we'd need to adjust startTimeSystem or frameCount, 
                    // but for now we just log it as it should be much rarer now.
                }

                if (nextFrameTimeSteady < now - 1s) // Sane reset
                {
                    logger->warn("Canvas '{}' fell behind, resetting clock.", canvas.Name());
                    // Reset to now
                    return; // Re-entry will handle restart (or we could just reset variables here)
                }

                if (nextFrameTimeSteady > now)
                    this_thread::sleep_until(nextFrameTimeSteady);
            } 
 });
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
        
        if (_currentEffectIndex == -1 && !_effects.empty())
            _currentEffectIndex = 0;
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
        jsonPair<MP4PlaybackEffect>(),
        make_pair("AuroraEffect", jsonPair<AuroraEffect>().second)
};

// Dynamically serialize an effect to JSON based on its actual type

inline void to_json(nlohmann::json &j, const ILEDEffect &effect)
{
    string type = effect.Type();
    auto it = to_from_json_map.find(type);
    if (it == to_from_json_map.end())
    {
        // Fallback to mangled name for backward compatibility and unregistered types
        type = typeid(effect).name();
        it = to_from_json_map.find(type);
    }
    
    if (it == to_from_json_map.end())
    {
        logger->error("Unknown effect type for serialization: {}", type);
        throw runtime_error("Unknown effect type for serialization: " + type);
    }
    it->second.first(j, effect);
    j["type"] = type;

    // Serialize schedule if we have one
    if (effect.GetSchedule())
        j["schedule"] = *effect.GetSchedule();
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
        auto schedule = j["schedule"].get<shared_ptr<ISchedule>>();
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

// IEffectsManager --> JSON

inline void from_json(const nlohmann::json &j, IEffectsManager &manager)
{
    manager.SetFPS(j.value("fps", uint16_t(30)));
    manager.SetEffects(j.value("effects", vector<shared_ptr<ILEDEffect>>()));
    manager.SetCurrentEffectIndex(j.value("currentEffectIndex", -1));

    // We deserialize the running state to a running *preference*. Directly starting the manager after
    // deserialization could create problems, and without having the canvas we can't start it anyway.
    if (j.contains("running"))
        manager.WantToRun(j.at("running").get<bool>());
}
