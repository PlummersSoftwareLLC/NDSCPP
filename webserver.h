#pragma once
#include <iostream>
#include <vector>
#include <memory>
#include <ranges>
#include <shared_mutex>
#include <future>
#include "json.hpp"
#include "crow_all.h"
#include "apihelpers.h"

using namespace std;

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
            res.set_header("Content-Type", "application/json");
            res.add_header("Access-Control-Allow-Origin", "*");
            res.add_header("Access-Control-Allow-Methods", "GET, OPTIONS, POST, PUT, DELETE");
            res.add_header("Access-Control-Allow-Headers", "Content-Type");
        }
    };

    IController & _controller; // Reference to all canvases
    crow::App<HeaderMiddleware> _crowApp;

    void PersistController(const crow::request& req)
    {
        ::PersistController(_controller, _controllerFileName, req);
    }

    void ApplyCanvasesRequest(const crow::request& req, function<void(shared_ptr<ICanvas>)> func)
    {
        ::ApplyCanvasesRequest(_controller, _apiMutex, _controllerFileName, req, std::move(func));
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
                    return BuildEffectCatalog().dump();
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
                    ApplyCanvasesRequest(req, [](shared_ptr<ICanvas> canvas) { canvas->Effects().Start(*canvas); });
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
                    ApplyCanvasesRequest(req, [](shared_ptr<ICanvas> canvas) { canvas->Effects().Stop(); });
                    return crow::response(crow::OK);
                }
                catch(const exception& e)
                {
                    logger->error("Error in /api/canvases/stop: {}", e.what());
                    return {crow::BAD_REQUEST, string("Error: ") + e.what()};
                }
            });

        // Create new canvas
        CROW_ROUTE(_crowApp, "/api/canvases")
            .methods(crow::HTTPMethod::POST)([&](const crow::request& req) -> crow::response
            {
                try
                {
                    const auto newID = CreateCanvas(_controller, _apiMutex, _controllerFileName, req);
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
                        const auto newId = CreateFeature(_controller, _apiMutex, _controllerFileName, req, canvasId);
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
                        DeleteFeature(_controller, _apiMutex, _controllerFileName, req, canvasId, featureId);
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
                        DeleteCanvas(_controller, _apiMutex, _controllerFileName, req, id);
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
                    auto canvas = UpdateCanvasDefinition(_controller, _apiMutex, _controllerFileName, req, id);
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
                    const auto effectIndex = AddEffect(_controller, _apiMutex, _controllerFileName, req, canvasId);
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
                    auto effect = UpdateEffect(_controller, _apiMutex, _controllerFileName, req, canvasId, effectIndex);
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
                    DeleteEffect(_controller, _apiMutex, _controllerFileName, req, canvasId, effectIndex);
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
