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
        return invoke(
            std::forward<Func>(func),
            context,
            std::forward<Args>(args)...);
    }

    template <typename Func>
    crow::response HandleRoute(const string &context, Func &&handler)
    {
        try
        {
            return handler();
        }
        catch (const exception &e)
        {
            logger->error("Error in {}: {}", context, e.what());
            return {crow::BAD_REQUEST, string("Error: ") + e.what()};
        }
    }

    template <typename Func>
    crow::response HandleRouteWithNotFound(const string &context, Func &&handler)
    {
        try
        {
            return handler();
        }
        catch (const out_of_range &e)
        {
            logger->error("Error in {}: {}", context, e.what());
            return {crow::NOT_FOUND, string("Error: ") + e.what()};
        }
        catch (const exception &e)
        {
            logger->error("Error in {}: {}", context, e.what());
            return {crow::BAD_REQUEST, string("Error: ") + e.what()};
        }
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
                return HandleRoute("/api/controller", [&]() -> crow::response
                {
                    shared_lock readLock(_apiMutex);
                    return nlohmann::json{{"controller", _controller}}.dump();
                });
            });

        // Replace the entire controller state (load config)
        CROW_ROUTE(_crowApp, "/api/controller")
            .methods(crow::HTTPMethod::PUT)([&](const crow::request& req) -> crow::response
            {
                return HandleRoute("/api/controller PUT", [&]() -> crow::response
                {
                    WithContext(req, api::ReplaceController);
                    return crow::response(crow::OK);
                });
            });

        // Clear all canvases (new config)
        CROW_ROUTE(_crowApp, "/api/controller/reset")
            .methods(crow::HTTPMethod::POST)([&](const crow::request& req) -> crow::response
            {
                return HandleRoute("/api/controller/reset", [&]() -> crow::response
                {
                    WithContext(req, api::ResetController);
                    return crow::response(crow::OK);
                });
            });

        // Enumerate just the sockets

        CROW_ROUTE(_crowApp, "/api/sockets")
            .methods(crow::HTTPMethod::GET)([&]() -> crow::response
            {
                return HandleRoute("/api/sockets", [&]() -> crow::response
                {
                    shared_lock readLock(_apiMutex);
                    return nlohmann::json{{"sockets", _controller.GetSockets()}}.dump();
                });
            });


        // Detail a single socket

        CROW_ROUTE(_crowApp, "/api/sockets/<int>")
            .methods(crow::HTTPMethod::GET)([&](int socketId) -> crow::response
            {
                return HandleRoute("/api/sockets/" + to_string(socketId), [&]() -> crow::response
                {
                    shared_lock readLock(_apiMutex);
                    return nlohmann::json{{"socket", _controller.GetSocketById(socketId)}}.dump();
                });
            });

        // Enumerate all the canvases

        CROW_ROUTE(_crowApp, "/api/canvases")
            .methods(crow::HTTPMethod::GET)([&]() -> crow::response
            {
                return HandleRoute("/api/canvases", [&]() -> crow::response
                {
                    shared_lock readLock(_apiMutex);
                    return nlohmann::json(_controller.Canvases()).dump();
                });
            });

        // Detail a single canvas

        CROW_ROUTE(_crowApp, "/api/canvases/<int>")
            .methods(crow::HTTPMethod::GET)([&](int id) -> crow::response
            {
                return HandleRoute("/api/canvases/" + to_string(id), [&]() -> crow::response
                {
                    shared_lock readLock(_apiMutex);
                    return nlohmann::json(*_controller.GetCanvasById(id)).dump();
                });
            });

        CROW_ROUTE(_crowApp, "/api/effects/catalog")
            .methods(crow::HTTPMethod::GET)([&]() -> crow::response
            {
                return HandleRoute("/api/effects/catalog", [&]() -> crow::response
                {
                    return api::BuildEffectCatalog().dump();
                });
            });

        CROW_ROUTE(_crowApp, "/api/canvases/start")
            .methods(crow::HTTPMethod::POST)([&](const crow::request& req) -> crow::response
            {
                return HandleRoute("/api/canvases/start", [&]() -> crow::response
                {
                    WithContext(req, api::StartCanvas);
                    return crow::response(crow::OK);
                });
            });

        CROW_ROUTE(_crowApp, "/api/canvases/stop")
            .methods(crow::HTTPMethod::POST)([&](const crow::request& req) -> crow::response
            {
                return HandleRoute("/api/canvases/stop", [&]() -> crow::response
                {
                    WithContext(req, api::StopCanvas);
                    return crow::response(crow::OK);
                });
            });

        // Set the current effect index on a canvas (lightweight, no teardown)
        CROW_ROUTE(_crowApp, "/api/canvases/<int>/currentEffect")
            .methods(crow::HTTPMethod::POST)([&](const crow::request& req, int canvasId) -> crow::response
            {
                return HandleRouteWithNotFound("/api/canvases/" + to_string(canvasId) + "/currentEffect", [&]() -> crow::response
                {
                    WithContext(req, api::SetCurrentEffect, canvasId);
                    return crow::response(crow::OK);
                });
            });

        // Create new canvas
        CROW_ROUTE(_crowApp, "/api/canvases")
            .methods(crow::HTTPMethod::POST)([&](const crow::request& req) -> crow::response
            {
                return HandleRoute("/api/canvases POST", [&]() -> crow::response
                {
                    const auto newID = WithContext(req, api::CreateCanvas);
                    return crow::response(201, nlohmann::json{{"id", newID}}.dump());
                });
            });

            // Create feature and add to canvas
            CROW_ROUTE(_crowApp, "/api/canvases/<int>/features")
                .methods(crow::HTTPMethod::POST)([&](const crow::request& req, int canvasId) -> crow::response
                {
                    return HandleRoute("/api/canvases/" + to_string(canvasId) + "/features POST", [&]() -> crow::response
                    {
                        const auto newId = WithContext(req, api::CreateFeature, canvasId);
                        return nlohmann::json{{"id", newId}}.dump();
                    });
                });


            // Delete feature from canvas
            CROW_ROUTE(_crowApp, "/api/canvases/<int>/features/<int>")
                .methods(crow::HTTPMethod::DELETE)([&](const crow::request& req, int canvasId, int featureId)
                {
                    return HandleRoute("/api/canvases/" + to_string(canvasId) + "/features/" + to_string(featureId) + " DELETE", [&]() -> crow::response
                    {
                        WithContext(req, api::DeleteFeature, canvasId, featureId);
                        return crow::response(crow::OK);
                    });
                });


            // Delete canvas
            CROW_ROUTE(_crowApp, "/api/canvases/<int>")
                .methods(crow::HTTPMethod::DELETE)([&](const crow::request& req, int id)
                {
                    return HandleRoute("/api/canvases/" + to_string(id) + " DELETE", [&]() -> crow::response
                    {
                        WithContext(req, api::DeleteCanvas, id);
                        return crow::response(crow::OK);
                    });
            });

        CROW_ROUTE(_crowApp, "/api/canvases/<int>")
            .methods(crow::HTTPMethod::PUT)([&](const crow::request &req, int id) -> crow::response
            {
                return HandleRouteWithNotFound("/api/canvases/" + to_string(id) + " PUT", [&]() -> crow::response
                {
                    auto canvas = WithContext(req, api::UpdateCanvasDefinition, id);
                    return crow::response(crow::OK, nlohmann::json(*canvas).dump());
                });
            });

        // Add effect to canvas
        CROW_ROUTE(_crowApp, "/api/canvases/<int>/effects")
            .methods(crow::HTTPMethod::POST)([&](const crow::request& req, int canvasId) -> crow::response
            {
                return HandleRoute("adding effect to canvas " + to_string(canvasId), [&]() -> crow::response
                {
                    const auto effectIndex = WithContext(req, api::AddEffect, canvasId);
                    return crow::response(crow::OK, nlohmann::json{{"index", effectIndex}}.dump());
                });
            });

        CROW_ROUTE(_crowApp, "/api/canvases/<int>/effects/<int>")
            .methods(crow::HTTPMethod::PUT)([&](const crow::request &req, int canvasId, int effectIndex) -> crow::response
            {
                return HandleRouteWithNotFound("/api/canvases/" + to_string(canvasId) + "/effects/" + to_string(effectIndex) + " PUT", [&]() -> crow::response
                {
                    auto effect = WithContext(req, api::UpdateEffect, canvasId, effectIndex);
                    return crow::response(crow::OK, nlohmann::json(*effect).dump());
                });
            });

        CROW_ROUTE(_crowApp, "/api/canvases/<int>/effects/<int>")
            .methods(crow::HTTPMethod::DELETE)([&](const crow::request &req, int canvasId, int effectIndex) -> crow::response
            {
                return HandleRouteWithNotFound("/api/canvases/" + to_string(canvasId) + "/effects/" + to_string(effectIndex) + " DELETE", [&]() -> crow::response
                {
                    WithContext(req, api::DeleteEffect, canvasId, effectIndex);
                    return crow::response(crow::OK);
                });
            });

        // Binary pixel data for canvas preview
        // Header: uint16_t width, uint16_t height, uint16_t fps
        // Body:   width*height * 3 bytes (RGB triplets, row-major)
        CROW_ROUTE(_crowApp, "/api/canvases/<int>/pixels")
            .methods(crow::HTTPMethod::GET)([&](int canvasId) -> crow::response
            {
                return HandleRouteWithNotFound("/api/canvases/" + to_string(canvasId) + "/pixels", [&]() -> crow::response
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
                });
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
