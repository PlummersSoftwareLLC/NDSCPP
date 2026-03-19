#pragma once

#include <filesystem>
#include <fstream>
#include <future>
#include <shared_mutex>
#include <sstream>

#include "apihelpers.h"
#include "crow_all.h"
#include "json.hpp"

using namespace std;

class DashboardServer
{
    shared_mutex & _apiMutex;
    IController & _controller;
    string _controllerFileName;
    filesystem::path _assetRoot;
    crow::SimpleApp _crowApp;
    future<void> _serverFuture;
    bool _routesRegistered = false;

    void PersistController(const crow::request &req)
    {
        ::PersistController(_controller, _controllerFileName, req);
    }

    void ApplyCanvasesRequest(const crow::request &req, function<void(shared_ptr<ICanvas>)> func)
    {
        ::ApplyCanvasesRequest(_controller, _apiMutex, _controllerFileName, req, std::move(func));
    }

    static string ContentTypeFor(const filesystem::path &path)
    {
        const auto extension = path.extension().string();

        if (extension == ".html")
            return "text/html; charset=utf-8";
        if (extension == ".css")
            return "text/css; charset=utf-8";
        if (extension == ".js")
            return "application/javascript; charset=utf-8";
        if (extension == ".json")
            return "application/json; charset=utf-8";

        return "text/plain; charset=utf-8";
    }

    crow::response ServeAsset(const filesystem::path &path) const
    {
        ifstream file(path, ios::binary);
        if (!file.is_open())
        {
            logger->error("Unable to open dashboard asset: {}", path.string());
            return crow::response(crow::NOT_FOUND);
        }

        ostringstream buffer;
        buffer << file.rdbuf();

        crow::response response(crow::OK);
        response.set_header("Content-Type", ContentTypeFor(path));
        response.set_header("Cache-Control", "no-store");
        response.write(buffer.str());
        return response;
    }

public:
    DashboardServer(IController &controller, string controllerFileName, shared_mutex &apiMutex, filesystem::path assetRoot)
        : _apiMutex(apiMutex),
          _controller(controller),
          _controllerFileName(std::move(controllerFileName)),
          _assetRoot(std::move(assetRoot))
    {
    }

    void StartAsync()
    {
        if (_routesRegistered)
            return;

        _routesRegistered = true;

        CROW_ROUTE(_crowApp, "/")([this]()
        {
            return ServeAsset(_assetRoot / "index.html");
        });

        CROW_ROUTE(_crowApp, "/index.html")([this]()
        {
            return ServeAsset(_assetRoot / "index.html");
        });

        CROW_ROUTE(_crowApp, "/styles.css")([this]()
        {
            return ServeAsset(_assetRoot / "styles.css");
        });

        CROW_ROUTE(_crowApp, "/app.js")([this]()
        {
            return ServeAsset(_assetRoot / "app.js");
        });

        CROW_ROUTE(_crowApp, "/favicon.ico")([]()
        {
            return crow::response(crow::NO_CONTENT);
        });

        CROW_ROUTE(_crowApp, "/api/controller")
            .methods(crow::HTTPMethod::GET)([this]() -> crow::response
            {
                try
                {
                    shared_lock readLock(_apiMutex);
                    crow::response response(crow::OK, nlohmann::json{{"controller", _controller}}.dump());
                    response.set_header("Content-Type", "application/json");
                    return response;
                }
                catch (const exception &e)
                {
                    logger->error("Error in dashboard /api/controller: {}", e.what());
                    return {crow::BAD_REQUEST, string("Error: ") + e.what()};
                }
            });

        // Replace the entire controller state (load config)
        CROW_ROUTE(_crowApp, "/api/controller")
            .methods(crow::HTTPMethod::PUT)([this](const crow::request &req) -> crow::response
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
                catch (const exception &e)
                {
                    logger->error("Error in dashboard /api/controller PUT: {}", e.what());
                    return {crow::BAD_REQUEST, string("Error: ") + e.what()};
                }
            });

        // Clear all canvases (new config)
        CROW_ROUTE(_crowApp, "/api/controller/reset")
            .methods(crow::HTTPMethod::POST)([this](const crow::request &) -> crow::response
            {
                try
                {
                    unique_lock writeLock(_apiMutex);
                    _controller.ClearAllCanvases();
                    _controller.WriteToFile(_controllerFileName);
                    return crow::response(crow::OK);
                }
                catch (const exception &e)
                {
                    logger->error("Error in dashboard /api/controller/reset: {}", e.what());
                    return {crow::BAD_REQUEST, string("Error: ") + e.what()};
                }
            });

        CROW_ROUTE(_crowApp, "/api/canvases")
            .methods(crow::HTTPMethod::GET)([this]() -> crow::response
            {
                try
                {
                    shared_lock readLock(_apiMutex);
                    crow::response response(crow::OK, nlohmann::json(_controller.Canvases()).dump());
                    response.set_header("Content-Type", "application/json");
                    return response;
                }
                catch (const exception &e)
                {
                    logger->error("Error in dashboard /api/canvases: {}", e.what());
                    return {crow::BAD_REQUEST, string("Error: ") + e.what()};
                }
            });

        CROW_ROUTE(_crowApp, "/api/effects/catalog")
            .methods(crow::HTTPMethod::GET)([this]() -> crow::response
            {
                try
                {
                    crow::response response(crow::OK, BuildEffectCatalog().dump());
                    response.set_header("Content-Type", "application/json");
                    return response;
                }
                catch (const exception &e)
                {
                    logger->error("Error in dashboard /api/effects/catalog: {}", e.what());
                    return {crow::BAD_REQUEST, string("Error: ") + e.what()};
                }
            });

        CROW_ROUTE(_crowApp, "/api/canvases/start")
            .methods(crow::HTTPMethod::POST)([this](const crow::request &req) -> crow::response
            {
                try
                {
                    ApplyCanvasesRequest(req, [](shared_ptr<ICanvas> canvas) { canvas->Effects().Start(*canvas); });
                    crow::response response(crow::OK);
                    response.set_header("Content-Type", "application/json");
                    return response;
                }
                catch (const exception &e)
                {
                    logger->error("Error in dashboard /api/canvases/start: {}", e.what());
                    return {crow::BAD_REQUEST, string("Error: ") + e.what()};
                }
            });

        CROW_ROUTE(_crowApp, "/api/canvases/stop")
            .methods(crow::HTTPMethod::POST)([this](const crow::request &req) -> crow::response
            {
                try
                {
                    ApplyCanvasesRequest(req, [](shared_ptr<ICanvas> canvas) { canvas->Effects().Stop(); });
                    crow::response response(crow::OK);
                    response.set_header("Content-Type", "application/json");
                    return response;
                }
                catch (const exception &e)
                {
                    logger->error("Error in dashboard /api/canvases/stop: {}", e.what());
                    return {crow::BAD_REQUEST, string("Error: ") + e.what()};
                }
            });

        CROW_ROUTE(_crowApp, "/api/canvases")
            .methods(crow::HTTPMethod::POST)([this](const crow::request &req) -> crow::response
            {
                try
                {
                    const auto newId = CreateCanvas(_controller, _apiMutex, _controllerFileName, req);
                    crow::response response(crow::CREATED, nlohmann::json{{"id", newId}}.dump());
                    response.set_header("Content-Type", "application/json");
                    return response;
                }
                catch (const exception &e)
                {
                    logger->error("Error in dashboard /api/canvases POST: {}", e.what());
                    return {crow::BAD_REQUEST, string("Error: ") + e.what()};
                }
            });

        CROW_ROUTE(_crowApp, "/api/canvases/<int>")
            .methods(crow::HTTPMethod::PUT)([this](const crow::request &req, int canvasId) -> crow::response
            {
                try
                {
                    auto canvas = UpdateCanvasDefinition(_controller, _apiMutex, _controllerFileName, req, canvasId);
                    crow::response response(crow::OK, nlohmann::json(*canvas).dump());
                    response.set_header("Content-Type", "application/json");
                    return response;
                }
                catch (const out_of_range &e)
                {
                    logger->error("Error in dashboard /api/canvases/{} PUT: {}", canvasId, e.what());
                    return {crow::NOT_FOUND, string("Error: ") + e.what()};
                }
                catch (const exception &e)
                {
                    logger->error("Error in dashboard /api/canvases/{} PUT: {}", canvasId, e.what());
                    return {crow::BAD_REQUEST, string("Error: ") + e.what()};
                }
            });

        CROW_ROUTE(_crowApp, "/api/canvases/<int>")
            .methods(crow::HTTPMethod::DELETE)([this](const crow::request &req, int canvasId) -> crow::response
            {
                try
                {
                    DeleteCanvas(_controller, _apiMutex, _controllerFileName, req, canvasId);
                    crow::response response(crow::OK);
                    response.set_header("Content-Type", "application/json");
                    return response;
                }
                catch (const exception &e)
                {
                    logger->error("Error in dashboard /api/canvases/{} DELETE: {}", canvasId, e.what());
                    return {crow::BAD_REQUEST, string("Error: ") + e.what()};
                }
            });

        CROW_ROUTE(_crowApp, "/api/canvases/<int>/features")
            .methods(crow::HTTPMethod::POST)([this](const crow::request &req, int canvasId) -> crow::response
            {
                try
                {
                    const auto newId = CreateFeature(_controller, _apiMutex, _controllerFileName, req, canvasId);
                    crow::response response(crow::OK, nlohmann::json{{"id", newId}}.dump());
                    response.set_header("Content-Type", "application/json");
                    return response;
                }
                catch (const exception &e)
                {
                    logger->error("Error in dashboard /api/canvases/{}/features POST: {}", canvasId, e.what());
                    return {crow::BAD_REQUEST, string("Error: ") + e.what()};
                }
            });

        CROW_ROUTE(_crowApp, "/api/canvases/<int>/features/<int>")
            .methods(crow::HTTPMethod::DELETE)([this](const crow::request &req, int canvasId, int featureId) -> crow::response
            {
                try
                {
                    DeleteFeature(_controller, _apiMutex, _controllerFileName, req, canvasId, featureId);
                    crow::response response(crow::OK);
                    response.set_header("Content-Type", "application/json");
                    return response;
                }
                catch (const out_of_range &e)
                {
                    logger->error("Error in dashboard /api/canvases/{}/features/{} DELETE: {}", canvasId, featureId, e.what());
                    return {crow::NOT_FOUND, string("Error: ") + e.what()};
                }
                catch (const exception &e)
                {
                    logger->error("Error in dashboard /api/canvases/{}/features/{} DELETE: {}", canvasId, featureId, e.what());
                    return {crow::BAD_REQUEST, string("Error: ") + e.what()};
                }
            });

        CROW_ROUTE(_crowApp, "/api/canvases/<int>/effects")
            .methods(crow::HTTPMethod::POST)([this](const crow::request &req, int canvasId) -> crow::response
            {
                try
                {
                    const auto effectIndex = AddEffect(_controller, _apiMutex, _controllerFileName, req, canvasId);
                    crow::response response(crow::OK, nlohmann::json{{"index", effectIndex}}.dump());
                    response.set_header("Content-Type", "application/json");
                    return response;
                }
                catch (const exception &e)
                {
                    logger->error("Error in dashboard /api/canvases/{}/effects POST: {}", canvasId, e.what());
                    return {crow::BAD_REQUEST, string("Error: ") + e.what()};
                }
            });

        CROW_ROUTE(_crowApp, "/api/canvases/<int>/effects/<int>")
            .methods(crow::HTTPMethod::PUT)([this](const crow::request &req, int canvasId, int effectIndex) -> crow::response
            {
                try
                {
                    auto effect = UpdateEffect(_controller, _apiMutex, _controllerFileName, req, canvasId, effectIndex);
                    crow::response response(crow::OK, nlohmann::json(*effect).dump());
                    response.set_header("Content-Type", "application/json");
                    return response;
                }
                catch (const out_of_range &e)
                {
                    logger->error("Error in dashboard /api/canvases/{}/effects/{} PUT: {}", canvasId, effectIndex, e.what());
                    return {crow::NOT_FOUND, string("Error: ") + e.what()};
                }
                catch (const exception &e)
                {
                    logger->error("Error in dashboard /api/canvases/{}/effects/{} PUT: {}", canvasId, effectIndex, e.what());
                    return {crow::BAD_REQUEST, string("Error: ") + e.what()};
                }
            });

        CROW_ROUTE(_crowApp, "/api/canvases/<int>/effects/<int>")
            .methods(crow::HTTPMethod::DELETE)([this](const crow::request &req, int canvasId, int effectIndex) -> crow::response
            {
                try
                {
                    DeleteEffect(_controller, _apiMutex, _controllerFileName, req, canvasId, effectIndex);
                    crow::response response(crow::OK);
                    response.set_header("Content-Type", "application/json");
                    return response;
                }
                catch (const out_of_range &e)
                {
                    logger->error("Error in dashboard /api/canvases/{}/effects/{} DELETE: {}", canvasId, effectIndex, e.what());
                    return {crow::NOT_FOUND, string("Error: ") + e.what()};
                }
                catch (const exception &e)
                {
                    logger->error("Error in dashboard /api/canvases/{}/effects/{} DELETE: {}", canvasId, effectIndex, e.what());
                    return {crow::BAD_REQUEST, string("Error: ") + e.what()};
                }
            });

        _crowApp.signal_clear();
        _serverFuture = _crowApp.port(_controller.GetWebUIPort()).multithreaded().run_async();

        if (_crowApp.wait_for_server_start(std::chrono::milliseconds(3000)) == std::cv_status::timeout)
            throw runtime_error("Timed out starting dashboard server on port " + to_string(_controller.GetWebUIPort()));
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
