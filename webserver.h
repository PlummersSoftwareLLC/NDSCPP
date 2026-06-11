#pragma once
#include <shared_mutex>
#include <future>
#include <functional>
#include <utility>
#include <array>
#include <type_traits>
#include <filesystem>
#include <fstream>
#include <sstream>
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
    filesystem::path _assetRoot;
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

    crow::response ServeAssetRequest(const string& requestPath) const
    {
        if (_assetRoot.empty())
            return crow::response(crow::NOT_FOUND);

        filesystem::path relativePath = filesystem::path(requestPath).lexically_normal();
        if (relativePath.empty() || relativePath == ".")
            relativePath = "index.html";

        if (relativePath.is_absolute())
            return crow::response(crow::NOT_FOUND);

        const string relative = relativePath.generic_string();
        if (relative == ".." ||
            relative.rfind("../", 0) == 0 ||
            relative.find("/../") != string::npos)
        {
            return crow::response(crow::NOT_FOUND);
        }

        const filesystem::path rootPath = _assetRoot.lexically_normal();
        const filesystem::path fullPath = (rootPath / relativePath).lexically_normal();

        const string root = rootPath.generic_string();
        const string full = fullPath.generic_string();
        const bool insideRoot =
            full == root ||
            (full.size() > root.size() &&
             full.compare(0, root.size(), root) == 0 &&
             full[root.size()] == '/');

        if (!insideRoot)
            return crow::response(crow::NOT_FOUND);

        if (!filesystem::exists(fullPath) || !filesystem::is_regular_file(fullPath))
            return crow::response(crow::NOT_FOUND);

        return ServeAsset(fullPath);
    }

    crow::response ServeAppConfig() const
    {
        shared_lock readLock(_apiMutex);

        crow::response response(crow::OK);
        response.set_header("Content-Type", "application/javascript; charset=utf-8");
        response.set_header("Cache-Control", "no-store");
        response.write("window.NDSCPP_API_SAME_PORT = true;\n");
        return response;
    }

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

    template <typename... Ints>
    string FormatRouteImpl(const char *format, const char *suffix, Ints... values)
    {
        string result(format);

        // Replace one placeholder per value, from left to right.
        const array<int, sizeof...(values)> replacements{static_cast<int>(values)...};
        for (int value : replacements)
        {
            size_t pos = result.find("<int>");
            if (pos == string::npos)
                break;

            result.replace(pos, 5, to_string(value));
        }

        if (suffix != nullptr && suffix[0] != '\0')
        {
            result += " ";
            result += suffix;
        }

        return result;
    }

    // Format a route string by replacing <int> placeholders with integer values.
    // Example: FormatRoute("/api/canvases/<int>/features/<int>", 5, 3)
    //          returns "/api/canvases/5/features/3"
    template <size_t N, typename... Args, enable_if_t<(is_same_v<decay_t<Args>, int> && ...), int> = 0>
    string FormatRoute(const char (&format)[N], Args... args)
    {
        return FormatRouteImpl(format, nullptr, args...);
    }

    // Format a route string, then append a suffix separated by a space.
    // Example: FormatRoute("/api/canvases/<int>", "(live)", 5)
    //          returns "/api/canvases/5 (live)"
    template <size_t N, size_t M, typename... Args, enable_if_t<(is_same_v<decay_t<Args>, int> && ...), int> = 0>
    string FormatRoute(const char (&format)[N], const char (&suffix)[M], Args... args)
    {
        return FormatRouteImpl(format, suffix, args...);
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
    WebServer(IController & controller, string controllerFileName, shared_mutex &apiMutex, filesystem::path assetRoot = {})
        : _apiMutex(apiMutex),
          _controllerFileName(std::move(controllerFileName)),
          _assetRoot(std::move(assetRoot)),
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
                return HandleRoute(FormatRoute(kRouteController, kPUT), [&]() -> crow::response
                {
                    WithContext(req, api::ReplaceController);
                    return crow::response(crow::OK);
                });
            });

        // Clear all canvases (new config)
        CROW_ROUTE(_crowApp, kRouteControllerReset)
            .methods(crow::HTTPMethod::POST)([&](const crow::request& req) -> crow::response
            {
                return HandleRoute(FormatRoute(kRouteControllerReset, kPOST), [&]() -> crow::response
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
                return HandleRoute(FormatRoute(kRouteSocket, socketId), [&]() -> crow::response
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
                return HandleRoute(FormatRoute(kRouteCanvas, id), [&]() -> crow::response
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
                return HandleRoute(FormatRoute(kRouteCanvasesStart, kPOST), [&]() -> crow::response
                {
                    WithContext(req, api::StartCanvas);
                    return crow::response(crow::OK);
                });
            });

        CROW_ROUTE(_crowApp, kRouteCanvasesStop)
            .methods(crow::HTTPMethod::POST)([&](const crow::request& req) -> crow::response
            {
                return HandleRoute(FormatRoute(kRouteCanvasesStop, kPOST), [&]() -> crow::response
                {
                    WithContext(req, api::StopCanvas);
                    return crow::response(crow::OK);
                });
            });

        // Set the current effect index on a canvas (lightweight, no teardown)
        CROW_ROUTE(_crowApp, kRouteCanvasCurEffect)
            .methods(crow::HTTPMethod::POST)([&](const crow::request& req, int canvasId) -> crow::response
            {
                return HandleRouteWithNotFound(FormatRoute(kRouteCanvasCurEffect, kPOST, canvasId), [&]() -> crow::response
                {
                    WithContext(req, api::SetCurrentEffect, canvasId);
                    return crow::response(crow::OK);
                });
            });

        // Create new canvas
        CROW_ROUTE(_crowApp, kRouteCanvases)
            .methods(crow::HTTPMethod::POST)([&](const crow::request& req) -> crow::response
            {
                return HandleRoute(FormatRoute(kRouteCanvases, kPOST), [&]() -> crow::response
                {
                    const auto newID = WithContext(req, api::CreateCanvas);
                    return crow::response(201, nlohmann::json{{"id", newID}}.dump());
                });
            });

        // Create feature and add to canvas
        CROW_ROUTE(_crowApp, kRouteCanvasFeatures)
            .methods(crow::HTTPMethod::POST)([&](const crow::request& req, int canvasId) -> crow::response
            {
                return HandleRoute(FormatRoute(kRouteCanvasFeatures, kPOST, canvasId), [&]() -> crow::response
                {
                    const auto newId = WithContext(req, api::CreateFeature, canvasId);
                    return nlohmann::json{{"id", newId}}.dump();
                });
            });


        // Delete feature from canvas
        CROW_ROUTE(_crowApp, kRouteCanvasFeature)
            .methods(crow::HTTPMethod::DELETE)([&](const crow::request& req, int canvasId, int featureId)
            {
                return HandleRoute(FormatRoute(kRouteCanvasFeature, kDELETE, canvasId, featureId), [&]() -> crow::response
                {
                    WithContext(req, api::DeleteFeature, canvasId, featureId);
                    return crow::response(crow::OK);
                });
            });


        // Delete canvas
        CROW_ROUTE(_crowApp, kRouteCanvas)
            .methods(crow::HTTPMethod::DELETE)([&](const crow::request& req, int id)
            {
                return HandleRoute(FormatRoute(kRouteCanvas, kDELETE, id), [&]() -> crow::response
                {
                    WithContext(req, api::DeleteCanvas, id);
                    return crow::response(crow::OK);
                });
            });

        CROW_ROUTE(_crowApp, kRouteCanvas)
            .methods(crow::HTTPMethod::PUT)([&](const crow::request &req, int id) -> crow::response
            {
                return HandleRouteWithNotFound(FormatRoute(kRouteCanvas, kPUT, id), [&]() -> crow::response
                {
                    auto canvas = WithContext(req, api::UpdateCanvasDefinition, id);
                    return crow::response(crow::OK, nlohmann::json(*canvas).dump());
                });
            });

        // Add effect to canvas
        CROW_ROUTE(_crowApp, kRouteCanvasEffects)
            .methods(crow::HTTPMethod::POST)([&](const crow::request& req, int canvasId) -> crow::response
            {
                return HandleRoute(FormatRoute(kRouteCanvasEffects, kPOST, canvasId), [&]() -> crow::response
                {
                    const auto effectIndex = WithContext(req, api::AddEffect, canvasId);
                    return crow::response(crow::OK, nlohmann::json{{"index", effectIndex}}.dump());
                });
            });

        CROW_ROUTE(_crowApp, kRouteCanvasEffect)
            .methods(crow::HTTPMethod::PUT)([&](const crow::request &req, int canvasId, int effectIndex) -> crow::response
            {
                return HandleRouteWithNotFound(FormatRoute(kRouteCanvasEffect, kPUT, canvasId, effectIndex), [&]() -> crow::response
                {
                    auto effect = WithContext(req, api::UpdateEffect, canvasId, effectIndex);
                    return crow::response(crow::OK, nlohmann::json(*effect).dump());
                });
            });

        CROW_ROUTE(_crowApp, kRouteCanvasEffect)
            .methods(crow::HTTPMethod::DELETE)([&](const crow::request &req, int canvasId, int effectIndex) -> crow::response
            {
                return HandleRouteWithNotFound(FormatRoute(kRouteCanvasEffect, kDELETE, canvasId, effectIndex), [&]() -> crow::response
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
                return HandleRouteWithNotFound(FormatRoute(kRouteCanvasPixels, canvasId), [&]() -> crow::response
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

        // Web UI asset routes (everything outside /api/*)
        CROW_ROUTE(_crowApp, "/").methods(crow::HTTPMethod::GET)([&]()
        {
            return ServeAssetRequest("");
        });

        CROW_ROUTE(_crowApp, "/<path>").methods(crow::HTTPMethod::GET)([&](const string& path)
        {
            if (path == "app-config.js")
                return ServeAppConfig();

            if (path == "api" || path.rfind("api/", 0) == 0)
                return crow::response(crow::NOT_FOUND);

            auto response = ServeAssetRequest(path);

            // SPA fallback: unknown client routes (no extension) should load index.html.
            if (response.code == crow::NOT_FOUND)
            {
                const auto lastSlash = path.find_last_of('/');
                const string leaf = (lastSlash == string::npos) ? path : path.substr(lastSlash + 1);
                if (leaf.find('.') == string::npos)
                    return ServeAssetRequest("");
            }

            return response;
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
