#pragma once
#include <memory>
#include <shared_mutex>
#include <future>
#include <functional>
#include <utility>
#include "json.hpp"
#include "crow_all.h"
#include "apihelpers.h"

using namespace std;
namespace api = ndscpp::api;

class WebServer
{
    shared_mutex & _apiMutex;
    string _controllerFileName;
    future<void> _serverFuture;
    bool _routesRegistered = false;

    struct HeaderMiddleware
    {
        struct context
        {
        };

        void before_handle(crow::request &req, crow::response &res, context &ctx)
        {
        }

        void after_handle(crow::request &req, crow::response &res, context &ctx)
        {
            if (res.get_header_value("Content-Type").empty())
                res.set_header("Content-Type", "application/json");
            res.add_header("Access-Control-Allow-Origin", "*");
            res.add_header("Access-Control-Allow-Methods", "GET, OPTIONS, POST, PUT, DELETE");
            res.add_header("Access-Control-Allow-Headers", "Content-Type");
        }
    };

    IController & _controller; // Reference to all canvases
    crow::App<HeaderMiddleware> _crowApp;

    api::ApiRequestContext MakeRequestContext(const crow::request &req)
    {
        return api::ApiRequestContext{_controller, _apiMutex, _controllerFileName, req};
    }

    template <typename Func, typename... Args>
    decltype(auto) WithContext(const crow::request &req, Func &&func, Args &&...args)
    {
        auto context = MakeRequestContext(req);
        return std::invoke(
            std::forward<Func>(func),
            context,
            std::forward<Args>(args)...);
    }

public:
    WebServer(IController & controller, string controllerFileName, shared_mutex &apiMutex)
        : _apiMutex(apiMutex),
          _controllerFileName(std::move(controllerFileName)),
          _controller(controller)
    {
    }

    ~WebServer()
    {
    }

    void StartAsync()
    {
        if (_routesRegistered)
            return;

        _routesRegistered = true;

        // The main controller, the most info you can get in a single call

        CROW_ROUTE(_crowApp, "/api/controller")
            .methods(crow::HTTPMethod::GET)([&]() -> crow::response
            {
                try
                {
                    shared_lock readLock(_apiMutex);
                    return nlohmann::json{{"controller", _controller}}.dump();
                }
                catch(const exception& e)
                {
                    logger->error("Error in /api/controller: {}", e.what());
                    return {crow::BAD_REQUEST, string("Error: ") + e.what()};
                }
            });

        // Replace the entire controller state (load config)
        CROW_ROUTE(_crowApp, "/api/controller")
            .methods(crow::HTTPMethod::PUT)([&](const crow::request& req) -> crow::response
            {
                try
                {
                    auto reqJson = nlohmann::json::parse(req.body);
                    const auto &canvasesJson = reqJson.is_array()
                        ? reqJson
                        : reqJson.value("canvases", nlohmann::json::array());
                    unique_lock writeLock(_apiMutex);

                    _controller.ClearAllCanvases();

                    for (const auto &canvasJson : canvasesJson)
                        _controller.AddCanvas(canvasJson.get<shared_ptr<ICanvas>>());

                    _controller.Connect();
                    _controller.Start(true);

                    _controller.WriteToFile(_controllerFileName);
                    return crow::response(crow::OK);
                }
                catch(const exception& e)
                {
                    logger->error("Error in /api/controller PUT: {}", e.what());
                    return {crow::BAD_REQUEST, string("Error: ") + e.what()};
                }
            });

        // Clear all canvases (new config)
        CROW_ROUTE(_crowApp, "/api/controller/reset")
            .methods(crow::HTTPMethod::POST)([&](const crow::request&) -> crow::response
            {
                try
                {
                    unique_lock writeLock(_apiMutex);
                    _controller.ClearAllCanvases();
                    _controller.WriteToFile(_controllerFileName);
                    return crow::response(crow::OK);
                }
                catch(const exception& e)
                {
                    logger->error("Error in /api/controller/reset: {}", e.what());
                    return {crow::BAD_REQUEST, string("Error: ") + e.what()};
                }
            });

        // Enumerate just the sockets

        CROW_ROUTE(_crowApp, "/api/sockets")
            .methods(crow::HTTPMethod::GET)([&]() -> crow::response
            {
                try
                {
                    shared_lock readLock(_apiMutex);
                    return nlohmann::json{{"sockets", _controller.GetSockets()}}.dump();
                }
                catch(const exception& e)
                {
                    logger->error("Error in /api/sockets: {}", e.what());
                    return {crow::BAD_REQUEST, string("Error: ") + e.what()};
                }
            });


        // Detail a single socket

        CROW_ROUTE(_crowApp, "/api/sockets/<int>")
            .methods(crow::HTTPMethod::GET)([&](int socketId) -> crow::response
            {
                try
                {
                    shared_lock readLock(_apiMutex);
                    return nlohmann::json{{"socket", _controller.GetSocketById(socketId)}}.dump();
                }
                catch(const exception& e)
                {
                    logger->error("Error in /api/sockets/{}: {}", socketId, e.what());
                    return {crow::BAD_REQUEST, string("Error: ") + e.what()};
                }
            });

        // Enumerate all the canvases

        CROW_ROUTE(_crowApp, "/api/canvases")
            .methods(crow::HTTPMethod::GET)([&]() -> crow::response
            {
                try
                {
                    shared_lock readLock(_apiMutex);
                    return nlohmann::json(_controller.Canvases()).dump();
                }
                catch(const exception& e)
                {
                    logger->error("Error in /api/canvases: {}", e.what());
                    return {crow::BAD_REQUEST, string("Error: ") + e.what()};
                }
            });

        // Detail a single canvas

        CROW_ROUTE(_crowApp, "/api/canvases/<int>")
            .methods(crow::HTTPMethod::GET)([&](int id) -> crow::response
            {
                try
                {
                    shared_lock readLock(_apiMutex);
                    return nlohmann::json(*_controller.GetCanvasById(id)).dump();
                }
                catch(const exception& e)
                {
                    logger->error("Error in /api/canvases/{}: {}", id, e.what());
                    return {crow::BAD_REQUEST, string("Error: ") + e.what()};
                }

            });

        CROW_ROUTE(_crowApp, "/api/effects/catalog")
            .methods(crow::HTTPMethod::GET)([&]() -> crow::response
            {
                try
                {
                    return api::BuildEffectCatalog().dump();
                }
                catch (const exception &e)
                {
                    logger->error("Error in /api/effects/catalog: {}", e.what());
                    return {crow::BAD_REQUEST, string("Error: ") + e.what()};
                }
            });

        CROW_ROUTE(_crowApp, "/api/canvases/start")
            .methods(crow::HTTPMethod::POST)([&](const crow::request& req) -> crow::response
            {
                try
                {
                    WithContext(req, api::ApplyCanvasesRequest, [](shared_ptr<ICanvas> canvas)
                    {
                        canvas->Effects().Start(*canvas);
                    });
                    return crow::response(crow::OK);
                }
                catch(const exception& e)
                {
                    logger->error("Error in /api/canvases/start: {}", e.what());
                    return {crow::BAD_REQUEST, string("Error: ") + e.what()};
                }
            });

        CROW_ROUTE(_crowApp, "/api/canvases/stop")
            .methods(crow::HTTPMethod::POST)([&](const crow::request& req) -> crow::response
            {
                try
                {
                    WithContext(req, api::ApplyCanvasesRequest, [](shared_ptr<ICanvas> canvas)
                    {
                        canvas->Effects().Stop();
                    });
                    return crow::response(crow::OK);
                }
                catch(const exception& e)
                {
                    logger->error("Error in /api/canvases/stop: {}", e.what());
                    return {crow::BAD_REQUEST, string("Error: ") + e.what()};
                }
            });

        // Set the current effect index on a canvas (lightweight, no teardown)
        CROW_ROUTE(_crowApp, "/api/canvases/<int>/currentEffect")
            .methods(crow::HTTPMethod::POST)([&](const crow::request& req, int canvasId) -> crow::response
            {
                try
                {
                    auto reqJson = nlohmann::json::parse(req.body);
                    int effectIndex = reqJson.at("effectIndex").get<int>();

                    unique_lock writeLock(_apiMutex);
                    auto canvas = _controller.GetCanvasById(static_cast<uint16_t>(canvasId));
                    canvas->Effects().SetCurrentEffect(static_cast<size_t>(effectIndex), *canvas);
                    WithContext(req, api::PersistController);
                    return crow::response(crow::OK);
                }
                catch (const out_of_range& e)
                {
                    logger->error("Error setting current effect on canvas {}: {}", canvasId, e.what());
                    return crow::response(crow::NOT_FOUND, string("Error: ") + e.what());
                }
                catch (const exception& e)
                {
                    logger->error("Error setting current effect on canvas {}: {}", canvasId, e.what());
                    return crow::response(crow::BAD_REQUEST, string("Error: ") + e.what());
                }
            });

        // Create new canvas
        CROW_ROUTE(_crowApp, "/api/canvases")
            .methods(crow::HTTPMethod::POST)([&](const crow::request& req) -> crow::response
            {
                try
                {
                    const auto newID = WithContext(req, api::CreateCanvas);
                    return crow::response(201, nlohmann::json{{"id", newID}}.dump());
                }
                catch (const exception& e)
                {
                    logger->error("Error in /api/canvases POST: {}", e.what());
                    return {crow::BAD_REQUEST, string("Error: ") + e.what()};
                }
            });

            // Create feature and add to canvas
            CROW_ROUTE(_crowApp, "/api/canvases/<int>/features")
                .methods(crow::HTTPMethod::POST)([&](const crow::request& req, int canvasId) -> crow::response
                {
                    try
                    {
                        const auto newId = WithContext(req, api::CreateFeature, canvasId);
                        return nlohmann::json{{"id", newId}}.dump();

                    }
                    catch (const exception& e)
                    {
                        logger->error("Error in /api/canvases/{}/features POST: {}", canvasId, e.what());
                        return {crow::BAD_REQUEST, string("Error: ") + e.what()};
                    }
                });


            // Delete feature from canvas
            CROW_ROUTE(_crowApp, "/api/canvases/<int>/features/<int>")
                .methods(crow::HTTPMethod::DELETE)([&](const crow::request& req, int canvasId, int featureId)
                {
                    try
                    {
                        WithContext(req, api::DeleteFeature, canvasId, featureId);
                        return crow::response(crow::OK);
                    }
                    catch(const exception& e)
                    {
                        logger->error("Error in /api/canvases/{}/features/{} DELETE: {}", canvasId, featureId, e.what());
                        return crow::response(crow::BAD_REQUEST, string("Error: ") + e.what());
                    }
                });


            // Delete canvas
            CROW_ROUTE(_crowApp, "/api/canvases/<int>")
                .methods(crow::HTTPMethod::DELETE)([&](const crow::request& req, int id)
                {
                    try
                    {
                        WithContext(req, api::DeleteCanvas, id);
                        return crow::response(crow::OK);
                    }
                    catch(const exception& e)
                    {
                        logger->error("Error in /api/canvases/{} DELETE: {}", id, e.what());
                        return crow::response(crow::BAD_REQUEST, string("Error: ") + e.what());
                }
            });

        CROW_ROUTE(_crowApp, "/api/canvases/<int>")
            .methods(crow::HTTPMethod::PUT)([&](const crow::request &req, int id) -> crow::response
            {
                try
                {
                    auto canvas = WithContext(req, api::UpdateCanvasDefinition, id);
                    return crow::response(crow::OK, nlohmann::json(*canvas).dump());
                }
                catch (const out_of_range &e)
                {
                    logger->error("Error in /api/canvases/{} PUT: {}", id, e.what());
                    return crow::response(crow::NOT_FOUND, string("Error: ") + e.what());
                }
                catch (const exception &e)
                {
                    logger->error("Error in /api/canvases/{} PUT: {}", id, e.what());
                    return crow::response(crow::BAD_REQUEST, string("Error: ") + e.what());
                }
            });

        // Add effect to canvas
        CROW_ROUTE(_crowApp, "/api/canvases/<int>/effects")
            .methods(crow::HTTPMethod::POST)([&](const crow::request& req, int canvasId) -> crow::response
            {
                try
                {
                    const auto effectIndex = WithContext(req, api::AddEffect, canvasId);
                    return crow::response(crow::OK, nlohmann::json{{"index", effectIndex}}.dump());
                }
                catch (const exception& e)
                {
                    logger->error("Error adding effect to canvas {}: {}", canvasId, e.what());
                    return crow::response(crow::BAD_REQUEST, string("Error: ") + e.what());
                }
            });

        CROW_ROUTE(_crowApp, "/api/canvases/<int>/effects/<int>")
            .methods(crow::HTTPMethod::PUT)([&](const crow::request &req, int canvasId, int effectIndex) -> crow::response
            {
                try
                {
                    auto effect = WithContext(req, api::UpdateEffect, canvasId, effectIndex);
                    return crow::response(crow::OK, nlohmann::json(*effect).dump());
                }
                catch (const out_of_range &e)
                {
                    logger->error("Error updating effect {} on canvas {}: {}", effectIndex, canvasId, e.what());
                    return crow::response(crow::NOT_FOUND, string("Error: ") + e.what());
                }
                catch (const exception &e)
                {
                    logger->error("Error updating effect {} on canvas {}: {}", effectIndex, canvasId, e.what());
                    return crow::response(crow::BAD_REQUEST, string("Error: ") + e.what());
                }
            });

        CROW_ROUTE(_crowApp, "/api/canvases/<int>/effects/<int>")
            .methods(crow::HTTPMethod::DELETE)([&](const crow::request &req, int canvasId, int effectIndex) -> crow::response
            {
                try
                {
                    WithContext(req, api::DeleteEffect, canvasId, effectIndex);
                    return crow::response(crow::OK);
                }
                catch (const out_of_range &e)
                {
                    logger->error("Error deleting effect {} on canvas {}: {}", effectIndex, canvasId, e.what());
                    return crow::response(crow::NOT_FOUND, string("Error: ") + e.what());
                }
                catch (const exception &e)
                {
                    logger->error("Error deleting effect {} on canvas {}: {}", effectIndex, canvasId, e.what());
                    return crow::response(crow::BAD_REQUEST, string("Error: ") + e.what());
                }
            });

        // Binary pixel data for canvas preview
        // Header: uint16_t width, uint16_t height, uint16_t fps
        // Body:   width*height * 3 bytes (RGB triplets, row-major)
        CROW_ROUTE(_crowApp, "/api/canvases/<int>/pixels")
            .methods(crow::HTTPMethod::GET)([&](int canvasId) -> crow::response
            {
                try
                {
                    shared_lock readLock(_apiMutex);
                    auto canvas = _controller.GetCanvasById(canvasId);
                    const auto &gfx = canvas->Graphics();
                    const auto &pixels = gfx.GetPixels();
                    uint16_t w = static_cast<uint16_t>(gfx.Width());
                    uint16_t h = static_cast<uint16_t>(gfx.Height());
                    uint16_t fps = static_cast<uint16_t>(canvas->Effects().GetFPS());

                    string body;
                    body.resize(6 + pixels.size() * 3);
                    memcpy(&body[0], &w, 2);
                    memcpy(&body[2], &h, 2);
                    memcpy(&body[4], &fps, 2);
                    memcpy(&body[6], pixels.data(), pixels.size() * 3);

                    crow::response response(crow::OK, std::move(body));
                    response.set_header("Content-Type", "application/octet-stream");
                    response.set_header("Cache-Control", "no-store");
                    return response;
                }
                catch (const out_of_range &e)
                {
                    return crow::response(crow::NOT_FOUND, string("Error: ") + e.what());
                }
                catch (const exception &e)
                {
                    return crow::response(crow::BAD_REQUEST, string("Error: ") + e.what());
                }
            });

        // Start the server
        _crowApp.signal_clear();
        _serverFuture = _crowApp.port(_controller.GetPort()).multithreaded().run_async();

        if (_crowApp.wait_for_server_start(std::chrono::milliseconds(3000)) == std::cv_status::timeout)
            throw runtime_error("Timed out starting API server on port " + to_string(_controller.GetPort()));
    }

    void Stop()
    {
        _crowApp.stop();
    }

    void Wait()
    {
        if (_serverFuture.valid())
            _serverFuture.get();
    }
};
