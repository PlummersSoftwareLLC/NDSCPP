#pragma once
using namespace std;
using namespace std::chrono;

// EffectsManager
//
// Manages a collection of ILEDEffect objects.  The EffectsManager is responsible for
// starting and stopping the effects, and for switching between them.  The EffectsManager
// can also be used to clear all effects.

#include "interfaces.h"
#include <vector>

class EffectsManager : public IEffectsManager
{
public:
    EffectsManager(uint16_t fps = 30) : _fps(fps), _currentEffectIndex(-1), _running(false) // No effect selected initially
    {
    }

    ~EffectsManager()
    {
        Stop(); // Ensure the worker thread is stopped when the manager is destroyed
    }

    void SetFPS(uint16_t fps)
    {
        _fps = fps;
    }

    uint16_t GetFPS() const
    {
        return _fps;
    }

    // Add an effect to the manager
    void AddEffect(unique_ptr<ILEDEffect> effect)
    {
        if (!effect)
            throw invalid_argument("Cannot add a null effect.");
        _effects.push_back(std::move(effect));

        // Automatically set the first effect as current if none is selected
        if (_currentEffectIndex == -1)
            _currentEffectIndex = 0;
    }

    // Remove an effect from the manager
    void RemoveEffect(unique_ptr<ILEDEffect> & effect)
    {
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
    void StartCurrentEffect(ICanvas& canvas)
    {
        if (IsEffectSelected())
            _effects[_currentEffectIndex]->Start(canvas);
    }

    void SetCurrentEffect(size_t index, ICanvas& canvas)
    {
        if (index >= _effects.size())
            throw out_of_range("Effect index out of range.");

        _currentEffectIndex = index;

        StartCurrentEffect(canvas);
   }

    // Update the current effect and render it to the canvas
    void UpdateCurrentEffect(ICanvas& canvas, milliseconds millisDelta)
    {
        if (IsEffectSelected())
            _effects[_currentEffectIndex]->Update(canvas, millisDelta);
    }

    // Switch to the next effect
    void NextEffect()
    {
        if (!_effects.empty())
            _currentEffectIndex = (_currentEffectIndex + 1) % _effects.size();
    }

    // Switch to the previous effect
    void PreviousEffect()
    {
        if (!_effects.empty())
            _currentEffectIndex = (_currentEffectIndex == 0) ? _effects.size() - 1 : _currentEffectIndex - 1;
    }

    // Get the name of the current effect
    string CurrentEffectName() const
    {
        if (IsEffectSelected())
            return _effects[_currentEffectIndex]->Name();
        return "No Effect Selected";
    }

    // Clear all effects
    void ClearEffects()
    {
        _effects.clear();
        _currentEffectIndex = -1;
    }

    // Start the worker thread to update effects
    void Start(ICanvas& canvas)
    {
        if (_running.exchange(true))
            return; // Already running

        _workerThread = thread([this, &canvas]() 
        {
            auto frameDuration = 1000ms / _fps; // Target duration per frame
            auto nextFrameTime = steady_clock::now();
            constexpr auto bUseCompression = true;

            while (_running)
            {
                // Update the effects and enqueue frames
                UpdateCurrentEffect(canvas, frameDuration);
                for (const auto &feature : canvas.Features())
                {
                    auto frame = feature->GetDataFrame();
                    if (bUseCompression)
                    {
                        auto compressedFrame = feature->Socket().CompressFrame(frame);
                        feature->Socket().EnqueueFrame(std::move(compressedFrame));
                    }
                    else
                    {
                        feature->Socket().EnqueueFrame(std::move(frame));
                    }
                }

                auto now = steady_clock::now();
                while (now < nextFrameTime && _running) {
                    this_thread::sleep_for(min(steady_clock::duration(10ms), nextFrameTime - now));
                    now = steady_clock::now(); // Update 'now' to avoid an infinite loop
                }

                // Set the next frame target
                nextFrameTime += frameDuration;
            }
        });
    }

    // Stop the worker thread
    void Stop()
    {
        if (!_running.exchange(false))
            return; // Not running

        if (_workerThread.joinable())
            _workerThread.join();
    }

private:
    vector<unique_ptr<ILEDEffect>> _effects;
    int _currentEffectIndex; // Index of the current effect
    thread _workerThread;
    atomic<bool> _running;
    uint16_t _fps;

    bool IsEffectSelected() const
    {
        return _currentEffectIndex >= 0 && _currentEffectIndex < static_cast<int>(_effects.size());
    }
};

