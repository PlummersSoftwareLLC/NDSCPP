#pragma once
#include <iostream>
#include <vector>
#include <memory>
#include <ranges>
#include <shared_mutex>
#include "json.hpp"
#include "crow_all.h"
#include "controller.h"

using namespace std;

class WebServer
{
    mutable shared_mutex _apiMutex;
    string _controllerFileName;

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
            res.add_header("Access-Control-Allow-Methods", "GET, OPTIONS, POST, DELETE");
            res.add_header("Access-Control-Allow-Headers", "Content-Type");
        }
    };

    IController & _controller; // Reference to all canvases
    crow::App<HeaderMiddleware> _crowApp;

    void PersistController(const crow::request& req)
    {
        if (req.url_params.get("nopersist") != nullptr)
            return;

        _controller.WriteToFile(_controllerFileName);
    }

    void ApplyCanvasesRequest(const crow::request& req, function<void(shared_ptr<ICanvas>)> func)
    {
        nlohmann::json reqJson;

        if (!req.body.empty())
            reqJson = nlohmann::json::parse(req.body);

        unique_lock writeLock(_apiMutex);
        
        if (reqJson.contains("canvasIds"))
        {
            for (auto& canvasId : reqJson["canvasIds"])
                func(_controller.GetCanvasById(canvasId));
        }
        else
        {
            for (auto& canvas : _controller.Canvases())
                func(canvas);
        }

        PersistController(req);
    }

public:
    WebServer(IController & controller, string controllerFileName) : _controller(controller), _controllerFileName(controllerFileName)
    {
    }

    ~WebServer()
    {
    }

    void Start()
    {
        // The main controller, the most info you can get in a single call

        CROW_ROUTE(_crowApp, "/api/controller")
            .methods(crow::HTTPMethod::GET)([&]() -> crow::response
            {
                try
                {
                    shared_lock readLock(_apiMutex);
                    return nlohmann::json{{"controller", _controller}}.dump();
                }
                catch(const std::exception& e)
                {
                    logger->error("Error in /api/controller: {}", e.what());
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
                catch(const std::exception& e)
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
                catch(const std::exception& e)
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
                catch(const std::exception& e)
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
                catch(const std::exception& e)
                {
                    logger->error("Error in /api/canvases/{}: {}", id, e.what());
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
                catch(const std::exception& e)
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
                catch(const std::exception& e)
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
                    // Parse and deserialize JSON payload
                    auto reqJson = nlohmann::json::parse(req.body);
                    auto canvas = reqJson.get<shared_ptr<ICanvas>>();

                    unique_lock writeLock(_apiMutex);
                    uint32_t newID = _controller.AddCanvas(canvas);

                    if (newID == -1)
                        return {crow::BAD_REQUEST, "Error, likely canvas with that ID already exists."};

                    PersistController(req);
                    writeLock.unlock();

                    auto& effectsManager = canvas->Effects();
                    // Start the effects manager of the new canvas if it wants to run and has effects
                    if (effectsManager.WantsToRun() && effectsManager.EffectCount() > 0)
                        effectsManager.Start(*canvas);

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
                        auto reqJson = nlohmann::json::parse(req.body);
                        auto feature = reqJson.get<shared_ptr<ILEDFeature>>();

                        unique_lock writeLock(_apiMutex);
                        auto newId = _controller.GetCanvasById(canvasId)->AddFeature(feature);
                        PersistController(req);
                        writeLock.unlock();

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
                        unique_lock writeLock(_apiMutex);
                        auto canvas = _controller.GetCanvasById(canvasId);
                        canvas->RemoveFeatureById(featureId);
                        PersistController(req);
                        writeLock.unlock();
                        
                        return crow::response(crow::OK);
                    }
                    catch(const std::exception& e)
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
                        unique_lock writeLock(_apiMutex);
                        _controller.DeleteCanvasById(id);
                        PersistController(req);
                        writeLock.unlock();

                        return crow::response(crow::OK);
                    }
                    catch(const std::exception& e)
                    {
                        logger->error("Error in /api/canvases/{} DELETE: {}", id, e.what());
                        return crow::response(crow::BAD_REQUEST, string("Error: ") + e.what());
                    }
                });

        // Add effect to canvas
        CROW_ROUTE(_crowApp, "/api/canvases/<int>/effects")
            .methods(crow::HTTPMethod::POST)([&](const crow::request& req, int canvasId) -> crow::response
            {
                try 
                {
                    auto reqJson = nlohmann::json::parse(req.body);
                    auto effect = reqJson.get<shared_ptr<ILEDEffect>>();

                    unique_lock writeLock(_apiMutex);
                    auto canvas = _controller.GetCanvasById(canvasId);
                    
                    // Get current effects
                    auto& effectsManager = canvas->Effects();
                    
                    // Add the new effect
                    effectsManager.AddEffect(effect);
                    
                    PersistController(req);
                    writeLock.unlock();

                    return crow::response(crow::OK);
                } 
                catch (const exception& e) 
                {
                    logger->error("Error adding effect to canvas {}: {}", canvasId, e.what());
                    return crow::response(crow::BAD_REQUEST, string("Error: ") + e.what());
                }
            });

        // Start the server
        _crowApp.port(_controller.GetPort()).multithreaded().run();
    }

    void Stop()
    {
        _crowApp.stop();
    }
};
