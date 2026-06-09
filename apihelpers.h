#pragma once

#include <functional>
#include <limits>
#include <shared_mutex>
#include <utility>

#include "controller.h"
#include "crow_all.h"

using namespace std;

namespace ndscpp::api
{

struct ApiRequestContext
{
    IController &controller;
    shared_mutex &apiMutex;
    const string &controllerFileName;
    const crow::request &req;

    unique_lock<shared_mutex> Lock() const
    {
        return unique_lock<shared_mutex>(apiMutex);
    }

    void Persist() const
    {
        if (req.url_params.get("nopersist") != nullptr)
            return;

        controller.WriteToFile(controllerFileName);
    }
};

inline void PersistController(const ApiRequestContext &context)
{
    context.Persist();
}

template <typename T>
inline T ParseBodyAs(const ApiRequestContext &context)
{
    return nlohmann::json::parse(context.req.body).get<T>();
}

template <typename MutateFunc>
inline void MutateEffects(ApiRequestContext &context, int canvasId, MutateFunc mutate)
{
    auto writeLock = context.Lock();
    auto canvas = context.controller.GetCanvasById(canvasId);
    auto &manager = canvas->Effects();
    const bool wantsToRun = manager.WantsToRun();

    manager.Stop();
    mutate(manager);

    context.Persist();
    writeLock.unlock();

    if (wantsToRun && manager.EffectCount() > 0)
        manager.Start(*canvas);
}

inline void ApplyCanvasesRequest(
    ApiRequestContext &context,
    function<void(shared_ptr<ICanvas>)> func)
{
    nlohmann::json reqJson;

    if (!context.req.body.empty())
        reqJson = nlohmann::json::parse(context.req.body);

    auto writeLock = context.Lock();

    if (reqJson.contains("canvasIds"))
    {
        for (auto &canvasId : reqJson["canvasIds"])
            func(context.controller.GetCanvasById(canvasId));
    }
    else
    {
        for (auto &canvas : context.controller.Canvases())
            func(canvas);
    }

    context.Persist();
}

inline size_t NormalizeCurrentEffectIndex(const IEffectsManager &manager, size_t effectCount)
{
    if (effectCount == 0)
        return numeric_limits<size_t>::max();

    const auto rawIndex = manager.GetCurrentEffect();
    if (rawIndex == numeric_limits<size_t>::max() || rawIndex >= effectCount)
        return effectCount - 1;

    return rawIndex;
}

inline uint32_t CreateCanvas(
    ApiRequestContext &context)
{
    auto canvas = ParseBodyAs<shared_ptr<ICanvas>>(context);

    auto writeLock = context.Lock();
    const uint32_t newId = context.controller.AddCanvas(canvas);

    if (newId == numeric_limits<uint32_t>::max())
        throw runtime_error("Canvas with that ID already exists.");

    context.Persist();
    writeLock.unlock();

    for (auto &feature : canvas->Features())
        feature->Socket()->Start();

    auto &effectsManager = canvas->Effects();
    if (effectsManager.WantsToRun() && effectsManager.EffectCount() > 0)
        effectsManager.Start(*canvas);

    return newId;
}

inline shared_ptr<ICanvas> UpdateCanvasDefinition(
    ApiRequestContext &context,
    int canvasId)
{
    auto updatedCanvas = ParseBodyAs<shared_ptr<ICanvas>>(context);
    updatedCanvas->SetId(static_cast<uint32_t>(canvasId));

    auto writeLock = context.Lock();
    if (!context.controller.UpdateCanvas(updatedCanvas))
        throw out_of_range("Canvas not found: " + to_string(canvasId));

    context.Persist();
    return updatedCanvas;
}

inline uint32_t CreateFeature(
    ApiRequestContext &context,
    int canvasId)
{
    auto feature = ParseBodyAs<shared_ptr<ILEDFeature>>(context);

    auto writeLock = context.Lock();
    auto canvas = context.controller.GetCanvasById(canvasId);
    const auto newId = canvas->AddFeature(feature);
    context.Persist();
    writeLock.unlock();

    feature->Socket()->Start();
    return newId;
}

inline void DeleteFeature(
    ApiRequestContext &context,
    int canvasId,
    int featureId)
{
    auto writeLock = context.Lock();
    auto canvas = context.controller.GetCanvasById(canvasId);
    if (!canvas->RemoveFeatureById(featureId))
        throw out_of_range("Feature not found: " + to_string(featureId));

    context.Persist();
}

inline void DeleteCanvas(
    ApiRequestContext &context,
    int canvasId)
{
    auto writeLock = context.Lock();
    context.controller.DeleteCanvasById(canvasId);
    context.Persist();
}

inline size_t AddEffect(
    ApiRequestContext &context,
    int canvasId)
{
    auto effect = ParseBodyAs<shared_ptr<ILEDEffect>>(context);
    size_t effectIndex = numeric_limits<size_t>::max();

    MutateEffects(context, canvasId, [&](IEffectsManager &manager)
    {
        manager.AddEffect(effect);
        effectIndex = manager.EffectCount() - 1;
    });

    return effectIndex;
}

inline shared_ptr<ILEDEffect> UpdateEffect(
    ApiRequestContext &context,
    int canvasId,
    int effectIndex)
{
    auto effect = ParseBodyAs<shared_ptr<ILEDEffect>>(context);

    MutateEffects(context, canvasId, [&](IEffectsManager &manager)
    {
        auto effects = manager.Effects();
        if (effectIndex < 0 || static_cast<size_t>(effectIndex) >= effects.size())
            throw out_of_range("Effect not found: " + to_string(effectIndex));

        effects[effectIndex] = effect;
        manager.SetEffects(std::move(effects));
    });

    return effect;
}

inline void DeleteEffect(
    ApiRequestContext &context,
    int canvasId,
    int effectIndex)
{
    MutateEffects(context, canvasId, [&](IEffectsManager &manager)
    {
        auto effects = manager.Effects();
        if (effectIndex < 0 || static_cast<size_t>(effectIndex) >= effects.size())
            throw out_of_range("Effect not found: " + to_string(effectIndex));

        effects.erase(effects.begin() + effectIndex);

        const auto nextIndex = NormalizeCurrentEffectIndex(manager, effects.size());
        manager.SetEffects(std::move(effects));
        manager.SetCurrentEffectIndex(nextIndex == numeric_limits<size_t>::max() ? -1 : static_cast<int>(nextIndex));
    });
}

nlohmann::json BuildEffectCatalog();

} // namespace ndscpp::api
