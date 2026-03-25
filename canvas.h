#pragma once
using namespace std;

// Canvas
//
// Represents the larger drawing surface that is made up from one or more LEDFeatures

#include "json.hpp"
#include "interfaces.h"
#include "basegraphics.h"
#include "ledfeature.h"
#include "effectsmanager.h"
#include <vector>
#include <mutex>

class Canvas : public ICanvas
{
    static atomic<uint32_t> _nextId;
    uint32_t                _id;
    BaseGraphics            _graphics;
    EffectsManager          _effects;
    string                  _name;
    vector<shared_ptr<ILEDFeature>> _features;
    mutable mutex           _featuresMutex;

public:
    Canvas(string name, uint32_t width, uint32_t height, uint16_t fps = 30) : 
        _id(NextId()),
        _graphics(width, height), 
        _effects(fps),
        _name(name)
    {
    }

    static uint32_t NextId()
    {
        return ++_nextId;
    }

    static void EnsureNextIdBeyond(uint32_t id)
    {
        uint32_t current = _nextId.load();
        while (current < id)
        {
            if (_nextId.compare_exchange_weak(current, id))
                break;
        }
    }

    string Name() const override
    {
        return _name;
    }

    uint32_t Id() const override 
    { 
        return _id; 
    }

    uint32_t SetId(uint32_t id) override 
    { 
        _id = id;
        return _id;
    }

    ILEDGraphics & Graphics() override
    {
        lock_guard lock(_featuresMutex);
        return _graphics;
    }
    
    const ILEDGraphics& Graphics() const override 
    { 
        lock_guard lock(_featuresMutex);
        return _graphics; 
    }

    IEffectsManager & Effects() override
    {
        lock_guard lock(_featuresMutex);
        return _effects;
    }

    const IEffectsManager & Effects() const override
    {
        lock_guard lock(_featuresMutex);
        return _effects;
    }

    vector<shared_ptr<ILEDFeature>>  Features() override
    {
        lock_guard lock(_featuresMutex);
        return _features;
    }

    const vector<shared_ptr<ILEDFeature>> Features() const override
    {
        lock_guard lock(_featuresMutex);
        return _features;
    }

    uint32_t AddFeature(shared_ptr<ILEDFeature> feature) override
    {
        lock_guard lock(_featuresMutex);
        if (!feature)
            throw invalid_argument("Cannot add a null feature.");

        feature->SetCanvas(this);
        uint32_t id = feature->Id();
        _features.push_back(feature);
        return id;    
    }

    bool RemoveFeatureById(uint16_t featureId) override
    {
        lock_guard lock(_featuresMutex);
        for (size_t i = 0; i < _features.size(); ++i)
        {
            if (_features[i]->Id() == featureId)
            {
                _features[i]->Socket()->Stop();
                _features.erase(_features.begin() + i);
                return true;
            }
        }
        return false;
    }

    friend void to_json(nlohmann::json& j, const ICanvas & canvas);
    friend void from_json(const nlohmann::json& j, shared_ptr<ICanvas>& canvas);
};

// ICanvas --> JSON

inline void to_json(nlohmann::json& j, const ICanvas& canvas) 
{
    // Serialize the features
    vector<nlohmann::json> jsonFeatures;
    for (const auto& feature : canvas.Features())
        jsonFeatures.push_back(*feature); // Dereference the shared pointer

    j = {
        {"name",              canvas.Name()},
        {"id",                canvas.Id()},
        {"width",             canvas.Graphics().Width()},
        {"height",            canvas.Graphics().Height()},
        {"currentEffectName", canvas.Effects().CurrentEffectName()},
        {"features",          jsonFeatures}, // Serialized feature data
        {"effectsManager",    canvas.Effects()}    // EffectsManager must have a `to_json`
    };
}

inline void to_json(nlohmann::json& j, const shared_ptr<ICanvas>& canvasPtr) 
{
    j = canvasPtr ? nlohmann::json(*canvasPtr) : nullptr;
}

// ICanvas <-- JSON

inline void from_json(const nlohmann::json& j, shared_ptr<ICanvas> & canvas) 
{
    const auto canvasId = j.value("id", uint32_t(0));
    const auto canvasName = j.value("name", str_snprintf("Canvas %u", 32, canvasId));

    // Create canvas with required fields.
    canvas = make_shared<Canvas>(
        canvasName,
        j.at("width").get<uint32_t>(),
        j.at("height").get<uint32_t>()
    );

    if (j.contains("id"))
        canvas->SetId(canvasId);

    // Features()
    for (const auto& featureJson : j.value("features", nlohmann::json::array()))
        canvas->AddFeature(featureJson.get<shared_ptr<ILEDFeature>>());

    // Validate and deserialize EffectsManager
    if (j.contains("effectsManager")) 
        from_json(j.at("effectsManager"), canvas->Effects());
}
