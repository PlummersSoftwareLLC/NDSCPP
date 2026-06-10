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
    static constexpr char kRouteController[]      = "/api/controller";
    static constexpr char kRouteControllerReset[] = "/api/controller/reset";
    static constexpr char kRouteSockets[]         = "/api/sockets";
    static constexpr char kRouteSocket[]          = "/api/sockets/<int>";
    static constexpr char kRouteCanvases[]        = "/api/canvases";
    static constexpr char kRouteCanvas[]          = "/api/canvases/<int>";
    static constexpr char kRouteCanvasCurEffect[] = "/api/canvases/<int>/currentEffect";
    static constexpr char kRouteCanvasFeatures[]  = "/api/canvases/<int>/features";
    static constexpr char kRouteCanvasFeature[]   = "/api/canvases/<int>/features/<int>";
    static constexpr char kRouteCanvasEffects[]   = "/api/canvases/<int>/effects";
    static constexpr char kRouteCanvasEffect[]    = "/api/canvases/<int>/effects/<int>";
    static constexpr char kRouteCanvasPixels[]    = "/api/canvases/<int>/pixels";
    static constexpr char kRouteEffectsCatalog[]  = "/api/effects/catalog";
    static constexpr char kRouteCanvasesStart[]   = "/api/canvases/start";
    static constexpr char kRouteCanvasesStop[]    = "/api/canvases/stop";

    static constexpr char kPOST[]   = "POST";
    static constexpr char kPUT[]    = "PUT";
    static constexpr char kDELETE[] = "DELETE";

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

        CROW_ROUTE(_crowApp, kRouteController)
            .methods(crow::HTTPMethod::GET)([&]() -> crow::response
            {
                return HandleRoute(kRouteController, [&]() -> crow::response
                {
                    shared_lock readLock(_apiMutex);
                    return nlohmann::json{{"controller", _controller}}.dump();
                });
            });

        // Replace the entire controller state (load config)
        CROW_ROUTE(_crowApp, kRouteController)
            .methods(crow::HTTPMethod::PUT)([&](const crow::request& req) -> crow::response
            {
                return HandleRoute(Utilities::FormatRoute(kRouteController, kPUT), [&]() -> crow::response
                {
                    WithContext(req, api::ReplaceController);
                    return crow::response(crow::OK);
                });
            });

        // Clear all canvases (new config)
        CROW_ROUTE(_crowApp, kRouteControllerReset)
            .methods(crow::HTTPMethod::POST)([&](const crow::request& req) -> crow::response
            {
                return HandleRoute(Utilities::FormatRoute(kRouteControllerReset, kPOST), [&]() -> crow::response
                {
                    WithContext(req, api::ResetController);
                    return crow::response(crow::OK);
                });
            });

        // Enumerate just the sockets

        CROW_ROUTE(_crowApp, kRouteSockets)
            .methods(crow::HTTPMethod::GET)([&]() -> crow::response
            {
                return HandleRoute(kRouteSockets, [&]() -> crow::response
                {
                    shared_lock readLock(_apiMutex);
                    return nlohmann::json{{"sockets", _controller.GetSockets()}}.dump();
                });
            });


        // Detail a single socket

        CROW_ROUTE(_crowApp, kRouteSocket)
            .methods(crow::HTTPMethod::GET)([&](int socketId) -> crow::response
            {
                return HandleRoute(Utilities::FormatRoute(kRouteSocket, socketId), [&]() -> crow::response
                {
                    shared_lock readLock(_apiMutex);
                    return nlohmann::json{{"socket", _controller.GetSocketById(socketId)}}.dump();
                });
            });

        // Enumerate all the canvases

        CROW_ROUTE(_crowApp, kRouteCanvases)
            .methods(crow::HTTPMethod::GET)([&]() -> crow::response
            {
                return HandleRoute(kRouteCanvases, [&]() -> crow::response
                {
                    shared_lock readLock(_apiMutex);
                    return nlohmann::json(_controller.Canvases()).dump();
                });
            });

        // Detail a single canvas

        CROW_ROUTE(_crowApp, kRouteCanvas)
            .methods(crow::HTTPMethod::GET)([&](int id) -> crow::response
            {
                return HandleRoute(Utilities::FormatRoute(kRouteCanvas, id), [&]() -> crow::response
                {
                    shared_lock readLock(_apiMutex);
                    return nlohmann::json(*_controller.GetCanvasById(id)).dump();
                });
            });

        CROW_ROUTE(_crowApp, kRouteEffectsCatalog)
            .methods(crow::HTTPMethod::GET)([&]() -> crow::response
            {
                return HandleRoute(kRouteEffectsCatalog, [&]() -> crow::response
                {
                    return api::BuildEffectCatalog().dump();
                });
            });

        CROW_ROUTE(_crowApp, kRouteCanvasesStart)
            .methods(crow::HTTPMethod::POST)([&](const crow::request& req) -> crow::response
            {
                return HandleRoute(Utilities::FormatRoute(kRouteCanvasesStart, kPOST), [&]() -> crow::response
                {
                    WithContext(req, api::StartCanvas);
                    return crow::response(crow::OK);
                });
            });

        CROW_ROUTE(_crowApp, kRouteCanvasesStop)
            .methods(crow::HTTPMethod::POST)([&](const crow::request& req) -> crow::response
            {
                return HandleRoute(Utilities::FormatRoute(kRouteCanvasesStop, kPOST), [&]() -> crow::response
                {
                    WithContext(req, api::StopCanvas);
                    return crow::response(crow::OK);
                });
            });

        // Set the current effect index on a canvas (lightweight, no teardown)
        CROW_ROUTE(_crowApp, kRouteCanvasCurEffect)
            .methods(crow::HTTPMethod::POST)([&](const crow::request& req, int canvasId) -> crow::response
            {
                return HandleRouteWithNotFound(Utilities::FormatRoute(kRouteCanvasCurEffect, kPOST, canvasId), [&]() -> crow::response
                {
                    WithContext(req, api::SetCurrentEffect, canvasId);
                    return crow::response(crow::OK);
                });
            });

        // Create new canvas
        CROW_ROUTE(_crowApp, kRouteCanvases)
            .methods(crow::HTTPMethod::POST)([&](const crow::request& req) -> crow::response
            {
                return HandleRoute(Utilities::FormatRoute(kRouteCanvases, kPOST), [&]() -> crow::response
                {
                    const auto newID = WithContext(req, api::CreateCanvas);
                    return crow::response(201, nlohmann::json{{"id", newID}}.dump());
                });
            });

        // Create feature and add to canvas
        CROW_ROUTE(_crowApp, kRouteCanvasFeatures)
            .methods(crow::HTTPMethod::POST)([&](const crow::request& req, int canvasId) -> crow::response
            {
                return HandleRoute(Utilities::FormatRoute(kRouteCanvasFeatures, kPOST, canvasId), [&]() -> crow::response
                {
                    const auto newId = WithContext(req, api::CreateFeature, canvasId);
                    return nlohmann::json{{"id", newId}}.dump();
                });
            });


        // Delete feature from canvas
        CROW_ROUTE(_crowApp, kRouteCanvasFeature)
            .methods(crow::HTTPMethod::DELETE)([&](const crow::request& req, int canvasId, int featureId)
            {
                return HandleRoute(Utilities::FormatRoute(kRouteCanvasFeature, kDELETE, canvasId, featureId), [&]() -> crow::response
                {
                    WithContext(req, api::DeleteFeature, canvasId, featureId);
                    return crow::response(crow::OK);
                });
            });


        // Delete canvas
        CROW_ROUTE(_crowApp, kRouteCanvas)
            .methods(crow::HTTPMethod::DELETE)([&](const crow::request& req, int id)
            {
                return HandleRoute(Utilities::FormatRoute(kRouteCanvas, kDELETE, id), [&]() -> crow::response
                {
                    WithContext(req, api::DeleteCanvas, id);
                    return crow::response(crow::OK);
                });
            });

        CROW_ROUTE(_crowApp, kRouteCanvas)
            .methods(crow::HTTPMethod::PUT)([&](const crow::request &req, int id) -> crow::response
            {
                return HandleRouteWithNotFound(Utilities::FormatRoute(kRouteCanvas, kPUT, id), [&]() -> crow::response
                {
                    auto canvas = WithContext(req, api::UpdateCanvasDefinition, id);
                    return crow::response(crow::OK, nlohmann::json(*canvas).dump());
                });
            });

        // Add effect to canvas
        CROW_ROUTE(_crowApp, kRouteCanvasEffects)
            .methods(crow::HTTPMethod::POST)([&](const crow::request& req, int canvasId) -> crow::response
            {
                return HandleRoute(Utilities::FormatRoute(kRouteCanvasEffects, kPOST, canvasId), [&]() -> crow::response
                {
                    const auto effectIndex = WithContext(req, api::AddEffect, canvasId);
                    return crow::response(crow::OK, nlohmann::json{{"index", effectIndex}}.dump());
                });
            });

        CROW_ROUTE(_crowApp, kRouteCanvasEffect)
            .methods(crow::HTTPMethod::PUT)([&](const crow::request &req, int canvasId, int effectIndex) -> crow::response
            {
                return HandleRouteWithNotFound(Utilities::FormatRoute(kRouteCanvasEffect, kPUT, canvasId, effectIndex), [&]() -> crow::response
                {
                    auto effect = WithContext(req, api::UpdateEffect, canvasId, effectIndex);
                    return crow::response(crow::OK, nlohmann::json(*effect).dump());
                });
            });

        CROW_ROUTE(_crowApp, kRouteCanvasEffect)
            .methods(crow::HTTPMethod::DELETE)([&](const crow::request &req, int canvasId, int effectIndex) -> crow::response
            {
                return HandleRouteWithNotFound(Utilities::FormatRoute(kRouteCanvasEffect, kDELETE, canvasId, effectIndex), [&]() -> crow::response
                {
                    WithContext(req, api::DeleteEffect, canvasId, effectIndex);
                    return crow::response(crow::OK);
                });
            });

        // Binary pixel data for canvas preview
        // Header: uint16_t width, uint16_t height, uint16_t fps
        // Body:   width*height * 3 bytes (RGB triplets, row-major)
        CROW_ROUTE(_crowApp, kRouteCanvasPixels)
            .methods(crow::HTTPMethod::GET)([&](int canvasId) -> crow::response
            {
                return HandleRouteWithNotFound(Utilities::FormatRoute(kRouteCanvasPixels, canvasId), [&]() -> crow::response
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
