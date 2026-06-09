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
    filesystem::path _assetRoot;
    crow::SimpleApp _crowApp;
    future<void> _serverFuture;
    bool _routesRegistered = false;

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

    crow::response ServeAppConfig() const
    {
        shared_lock readLock(_apiMutex);

        crow::response response(crow::OK);
        response.set_header("Content-Type", "application/javascript; charset=utf-8");
        response.set_header("Cache-Control", "no-store");
        response.write(str_snprintf("window.NDSCPP_API_PORT = %u;\n", 64, _controller.GetPort()));
        return response;
    }

public:
    DashboardServer(IController &controller, string controllerFileName, shared_mutex &apiMutex, filesystem::path assetRoot)
        : _apiMutex(apiMutex),
          _controller(controller),
          _assetRoot(std::move(assetRoot))
    {
                (void)controllerFileName;
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

        CROW_ROUTE(_crowApp, "/app-config.js")([this]()
        {
            return ServeAppConfig();
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
