#pragma once

#include <limits>
#include <shared_mutex>

#include "controller.h"
#include "crow_all.h"

using namespace std;

inline void PersistController(IController &controller, const string &controllerFileName, const crow::request &req)
{
    if (req.url_params.get("nopersist") != nullptr)
        return;

    controller.WriteToFile(controllerFileName);
}

inline void ApplyCanvasesRequest(
    IController &controller,
    shared_mutex &apiMutex,
    const string &controllerFileName,
    const crow::request &req,
    function<void(shared_ptr<ICanvas>)> func)
{
    nlohmann::json reqJson;

    if (!req.body.empty())
        reqJson = nlohmann::json::parse(req.body);

    unique_lock writeLock(apiMutex);

    if (reqJson.contains("canvasIds"))
    {
        for (auto &canvasId : reqJson["canvasIds"])
            func(controller.GetCanvasById(canvasId));
    }
    else
    {
        for (auto &canvas : controller.Canvases())
            func(canvas);
    }

    PersistController(controller, controllerFileName, req);
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
    IController &controller,
    shared_mutex &apiMutex,
    const string &controllerFileName,
    const crow::request &req)
{
    auto reqJson = nlohmann::json::parse(req.body);
    auto canvas = reqJson.get<shared_ptr<ICanvas>>();

    unique_lock writeLock(apiMutex);
    const uint32_t newId = controller.AddCanvas(canvas);

    if (newId == numeric_limits<uint32_t>::max())
        throw runtime_error("Canvas with that ID already exists.");

    PersistController(controller, controllerFileName, req);
    writeLock.unlock();

    for (auto &feature : canvas->Features())
        feature->Socket()->Start();

    auto &effectsManager = canvas->Effects();
    if (effectsManager.WantsToRun() && effectsManager.EffectCount() > 0)
        effectsManager.Start(*canvas);

    return newId;
}

inline shared_ptr<ICanvas> UpdateCanvasDefinition(
    IController &controller,
    shared_mutex &apiMutex,
    const string &controllerFileName,
    const crow::request &req,
    int canvasId)
{
    auto reqJson = nlohmann::json::parse(req.body);
    auto updatedCanvas = reqJson.get<shared_ptr<ICanvas>>();
    updatedCanvas->SetId(static_cast<uint32_t>(canvasId));

    unique_lock writeLock(apiMutex);
    if (!controller.UpdateCanvas(updatedCanvas))
        throw out_of_range("Canvas not found: " + to_string(canvasId));

    PersistController(controller, controllerFileName, req);
    return updatedCanvas;
}

inline uint32_t CreateFeature(
    IController &controller,
    shared_mutex &apiMutex,
    const string &controllerFileName,
    const crow::request &req,
    int canvasId)
{
    auto reqJson = nlohmann::json::parse(req.body);
    auto feature = reqJson.get<shared_ptr<ILEDFeature>>();

    unique_lock writeLock(apiMutex);
    auto canvas = controller.GetCanvasById(canvasId);
    const auto newId = canvas->AddFeature(feature);
    PersistController(controller, controllerFileName, req);
    writeLock.unlock();

    feature->Socket()->Start();
    return newId;
}

inline void DeleteFeature(
    IController &controller,
    shared_mutex &apiMutex,
    const string &controllerFileName,
    const crow::request &req,
    int canvasId,
    int featureId)
{
    unique_lock writeLock(apiMutex);
    auto canvas = controller.GetCanvasById(canvasId);
    if (!canvas->RemoveFeatureById(featureId))
        throw out_of_range("Feature not found: " + to_string(featureId));

    PersistController(controller, controllerFileName, req);
}

inline void DeleteCanvas(
    IController &controller,
    shared_mutex &apiMutex,
    const string &controllerFileName,
    const crow::request &req,
    int canvasId)
{
    unique_lock writeLock(apiMutex);
    controller.DeleteCanvasById(canvasId);
    PersistController(controller, controllerFileName, req);
}

inline size_t AddEffect(
    IController &controller,
    shared_mutex &apiMutex,
    const string &controllerFileName,
    const crow::request &req,
    int canvasId)
{
    auto reqJson = nlohmann::json::parse(req.body);
    auto effect = reqJson.get<shared_ptr<ILEDEffect>>();

    unique_lock writeLock(apiMutex);
    auto canvas = controller.GetCanvasById(canvasId);
    auto &manager = canvas->Effects();
    const bool wantsToRun = manager.WantsToRun();
    manager.Stop();
    manager.AddEffect(effect);
    const auto effectIndex = manager.EffectCount() - 1;
    PersistController(controller, controllerFileName, req);
    writeLock.unlock();

    if (wantsToRun && manager.EffectCount() > 0)
        manager.Start(*canvas);

    return effectIndex;
}

inline shared_ptr<ILEDEffect> UpdateEffect(
    IController &controller,
    shared_mutex &apiMutex,
    const string &controllerFileName,
    const crow::request &req,
    int canvasId,
    int effectIndex)
{
    auto reqJson = nlohmann::json::parse(req.body);
    auto effect = reqJson.get<shared_ptr<ILEDEffect>>();

    unique_lock writeLock(apiMutex);
    auto canvas = controller.GetCanvasById(canvasId);
    auto &manager = canvas->Effects();
    auto effects = manager.Effects();

    if (effectIndex < 0 || static_cast<size_t>(effectIndex) >= effects.size())
        throw out_of_range("Effect not found: " + to_string(effectIndex));

    const bool wantsToRun = manager.WantsToRun();
    manager.Stop();
    effects[effectIndex] = effect;
    manager.SetEffects(std::move(effects));
    PersistController(controller, controllerFileName, req);
    writeLock.unlock();

    if (wantsToRun && manager.EffectCount() > 0)
        manager.Start(*canvas);

    return effect;
}

inline void DeleteEffect(
    IController &controller,
    shared_mutex &apiMutex,
    const string &controllerFileName,
    const crow::request &req,
    int canvasId,
    int effectIndex)
{
    unique_lock writeLock(apiMutex);
    auto canvas = controller.GetCanvasById(canvasId);
    auto &manager = canvas->Effects();
    auto effects = manager.Effects();

    if (effectIndex < 0 || static_cast<size_t>(effectIndex) >= effects.size())
        throw out_of_range("Effect not found: " + to_string(effectIndex));

    const bool wantsToRun = manager.WantsToRun();
    manager.Stop();
    effects.erase(effects.begin() + effectIndex);

    const auto nextIndex = NormalizeCurrentEffectIndex(manager, effects.size());
    manager.SetEffects(std::move(effects));
    manager.SetCurrentEffectIndex(nextIndex == numeric_limits<size_t>::max() ? -1 : static_cast<int>(nextIndex));

    PersistController(controller, controllerFileName, req);
    writeLock.unlock();

    if (wantsToRun && manager.EffectCount() > 0)
        manager.Start(*canvas);
}

inline nlohmann::json BuildEffectCatalog()
{
    using nlohmann::json;

    const auto paletteDefaults = json{
        {"colors", StandardPalettes::Rainbow},
        {"blend", true}
    };

    return json{
        {"effects", json::array({
            {
                {"type", typeid(SolidColorFill).name()},
                {"label", "Solid Color Fill"},
                {"defaults", json{{"color", CRGB(255, 96, 64)}}},
                {"fields", json::array({
                    {{"path", "color"}, {"label", "Color"}, {"input", "color"}}
                })}
            },
            {
                {"type", typeid(DaveDebugEffect).name()},
                {"label", "Dave Debug"},
                {"defaults", json::object()},
                {"fields", json::array()}
            },
            {
                {"type", typeid(PaletteEffect).name()},
                {"label", "Palette Scroll"},
                {"defaults", json{
                    {"palette", paletteDefaults},
                    {"ledColorPerSecond", 3.0},
                    {"ledScrollSpeed", 0.0},
                    {"density", 1.0},
                    {"everyNthDot", 1.0},
                    {"dotSize", 1},
                    {"rampedColor", false},
                    {"brightness", 1.0},
                    {"mirrored", false}
                }},
                {"fields", json::array({
                    {{"path", "palette.colors"}, {"label", "Palette Colors"}, {"input", "json"}},
                    {{"path", "palette.blend"}, {"label", "Blend Colors"}, {"input", "checkbox"}},
                    {{"path", "ledColorPerSecond"}, {"label", "Color Rate"}, {"input", "number"}, {"step", 0.1}},
                    {{"path", "ledScrollSpeed"}, {"label", "Scroll Speed"}, {"input", "number"}, {"step", 0.1}},
                    {{"path", "density"}, {"label", "Density"}, {"input", "number"}, {"step", 0.01}},
                    {{"path", "everyNthDot"}, {"label", "Every Nth Dot"}, {"input", "number"}, {"step", 0.1}},
                    {{"path", "dotSize"}, {"label", "Dot Size"}, {"input", "number"}, {"step", 1}, {"min", 1}},
                    {{"path", "rampedColor"}, {"label", "Ramped Color"}, {"input", "checkbox"}},
                    {{"path", "brightness"}, {"label", "Brightness"}, {"input", "number"}, {"step", 0.05}, {"min", 0.0}, {"max", 1.0}},
                    {{"path", "mirrored"}, {"label", "Mirrored"}, {"input", "checkbox"}}
                })}
            },
            {
                {"type", typeid(ColorWaveEffect).name()},
                {"label", "Color Wave"},
                {"defaults", json{{"speed", 0.5}, {"waveFrequency", 10.0}}},
                {"fields", json::array({
                    {{"path", "speed"}, {"label", "Speed"}, {"input", "number"}, {"step", 0.1}},
                    {{"path", "waveFrequency"}, {"label", "Wave Frequency"}, {"input", "number"}, {"step", 0.1}}
                })}
            },
            {
                {"type", typeid(StarfieldEffect).name()},
                {"label", "Starfield"},
                {"defaults", json{{"starCount", 100}}},
                {"fields", json::array({
                    {{"path", "starCount"}, {"label", "Star Count"}, {"input", "number"}, {"step", 1}, {"min", 1}}
                })}
            },
            {
                {"type", typeid(BouncingBallEffect).name()},
                {"label", "Bouncing Balls"},
                {"defaults", json{{"ballCount", 5}, {"ballSize", 1}, {"mirrored", true}, {"erase", true}}},
                {"fields", json::array({
                    {{"path", "ballCount"}, {"label", "Ball Count"}, {"input", "number"}, {"step", 1}, {"min", 1}},
                    {{"path", "ballSize"}, {"label", "Ball Size"}, {"input", "number"}, {"step", 1}, {"min", 1}},
                    {{"path", "mirrored"}, {"label", "Mirrored"}, {"input", "checkbox"}},
                    {{"path", "erase"}, {"label", "Erase"}, {"input", "checkbox"}}
                })}
            },
            {
                {"type", typeid(FireworksEffect).name()},
                {"label", "Fireworks"},
                {"defaults", json{
                    {"maxSpeed", 175.0},
                    {"newParticleProbability", 1.0},
                    {"particlePreignitionTime", 0.0},
                    {"particleIgnition", 0.2},
                    {"particleHoldTime", 0.0},
                    {"particleFadeTime", 2.0},
                    {"particleSize", 1.0}
                }},
                {"fields", json::array({
                    {{"path", "maxSpeed"}, {"label", "Max Speed"}, {"input", "number"}, {"step", 1}},
                    {{"path", "newParticleProbability"}, {"label", "Spawn Probability"}, {"input", "number"}, {"step", 0.05}},
                    {{"path", "particlePreignitionTime"}, {"label", "Pre-Ignition"}, {"input", "number"}, {"step", 0.05}},
                    {{"path", "particleIgnition"}, {"label", "Ignition"}, {"input", "number"}, {"step", 0.05}},
                    {{"path", "particleHoldTime"}, {"label", "Hold Time"}, {"input", "number"}, {"step", 0.05}},
                    {{"path", "particleFadeTime"}, {"label", "Fade Time"}, {"input", "number"}, {"step", 0.05}},
                    {{"path", "particleSize"}, {"label", "Particle Size"}, {"input", "number"}, {"step", 0.1}, {"min", 0.1}}
                })}
            },
            {
                {"type", typeid(MP4PlaybackEffect).name()},
                {"label", "MP4 Playback"},
                {"defaults", json{{"filePath", "./media/mp4/goldendollars.mp4"}}},
                {"fields", json::array({
                    {{"path", "filePath"}, {"label", "File Path"}, {"input", "text"}}
                })}
            }
        })}
    };
}
