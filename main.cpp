// Main.cpp
//
// This file is the main entry point for the NDSCPP LED Matrix Server application.
// It creates a Canvas, adds a GreenFillEffect to it, and then enters a loop where it
// renders the effect to the canvas, compresses the data, and sends it to the LED
// matrix via a SocketChannel.  The program will continue to run until it receives
// a SIGINT signal (Ctrl-C).

// Main.cpp
//
// This file is the main entry point for the NDSCPP LED Matrix Server application.
// It creates a number of Canvases and adds an effect to each, after which they enter
// a loop where it renders the effect to the canvas, compresses the data, and sends
// it to the relevant LED controller via a SocketChannel.  The program will continue
// to run until it receives a SIGINT signal (Ctrl-C).

#include <csignal>
#include <iostream>
#include <atomic>
#include <chrono>
#include <thread>
#include <filesystem>
#include <optional>
#include <getopt.h>

using namespace std;
using namespace chrono;

#include "crow_all.h"
#include "global.h"
#include "canvas.h"
#include "interfaces.h"
#include "socketchannel.h"
#include "ledfeature.h"
#include "webserver.h"
#include "dashboardserver.h"
#include "controller.h"
#include "schedule.h"

namespace
{
    atomic<bool> gShouldExit{false};

    void HandleSignal(int)
    {
        gShouldExit = true;
    }

    optional<uint16_t> ParsePortOption(const char *value, const char *optionName)
    {
        const auto parsedPort = atoi(value);
        if (parsedPort < 1 || parsedPort > 65535)
            throw runtime_error(string(optionName) + " must be between 1 and 65535");

        return static_cast<uint16_t>(parsedPort);
    }
}

atomic<uint32_t> Canvas::_nextId{0};        // Initialize the static member variable for canvas.h
atomic<uint32_t> LEDFeature::_nextId{0};    // Initialize the static member variable for ledfeature.h
atomic<uint32_t> SocketChannel::_nextId{0}; // Initialize the static member variable for socketchannel.h

shared_ptr<spdlog::logger> logger = spdlog::stdout_color_mt("console");

// Main program entry point. Runs the webServer and starts up the LED processing.
// When SIGINT is received, exits gracefully.

int main(int argc, char *argv[])
{
    logger->set_level(spdlog::level::info);

    optional<uint16_t> apiPortOverride;
    optional<uint16_t> webUiPortOverride;
    string filename = "config.led";

    // Parse command-line options
    int opt;
    const option longOptions[] =
    {
        {"port", required_argument, nullptr, 'p'},
        {"config", required_argument, nullptr, 'c'},
        {"webuiport", required_argument, nullptr, 'w'},
        {"help", no_argument, nullptr, 'h'},
        {nullptr, 0, nullptr, 0}
    };

    while ((opt = getopt_long(argc, argv, "p:c:w:h", longOptions, nullptr)) != -1)
    {
        switch (opt)
        {
            case 'p':
                apiPortOverride = ParsePortOption(optarg, "Port");
                break;
            case 'c':
                filename = optarg;
                break;
            case 'w':
                webUiPortOverride = ParsePortOption(optarg, "Web UI port");
                break;
            case 'h':
                cerr << "Usage: " << argv[0] << " [-p <port>] [-w <webuiport>] [-c <configfile>]" << endl;
                return EXIT_SUCCESS;
            default:
                cerr << "Usage: " << argv[0] << " [-p <port>] [-w <webuiport>] [-c <configfile>]" << endl;
                return EXIT_FAILURE;
        }
    }

    // Load the canvases from the configuration file or use hard-coded table defaults
    // depending on USE_DEMO_DATA being defined or not.

    #define USE_DEMO_DATA 0

    #if USE_DEMO_DATA
        unique_ptr<Controller> ptrController = make_unique<Controller>(7777, 9997);
        ptrController->LoadSampleCanvases();
    #else
        unique_ptr<Controller> ptrController = Controller::CreateFromFile(filename);
    #endif

    if (apiPortOverride)
        ptrController->SetPort(*apiPortOverride);
    if (webUiPortOverride)
        ptrController->SetWebUIPort(*webUiPortOverride);

    if (ptrController->GetPort() == ptrController->GetWebUIPort())
    {
        logger->error("API port {} and web UI port {} cannot be the same", ptrController->GetPort(), ptrController->GetWebUIPort());
        return EXIT_FAILURE;
    }

    signal(SIGINT, HandleSignal);
    signal(SIGTERM, HandleSignal);

    ptrController->Connect();
    ptrController->Start(true); // Consider if effect managers want to run

    const auto executableDir = filesystem::weakly_canonical(filesystem::absolute(argv[0])).parent_path();
    const auto assetRoot = executableDir / "webui";

    crow::logger::setLogLevel(crow::LogLevel::WARNING);
    shared_mutex apiMutex;
    WebServer apiServer(*ptrController.get(), filename, apiMutex);
    DashboardServer dashboardServer(*ptrController.get(), filename, apiMutex, assetRoot);

    try
    {
        apiServer.StartAsync();
        dashboardServer.StartAsync();
    }
    catch (const exception &e)
    {
        logger->error("Failed to start servers: {}", e.what());
        dashboardServer.Stop();
        apiServer.Stop();
        dashboardServer.Wait();
        apiServer.Wait();
        ptrController->Stop();
        ptrController->Disconnect();
        return EXIT_FAILURE;
    }

    logger->info("API listening on http://localhost:{}/api", ptrController->GetPort());
    logger->info("Dashboard listening on http://localhost:{}/", ptrController->GetWebUIPort());

    while (!gShouldExit)
        this_thread::sleep_for(100ms);

    cout << "Shutting down..." << endl;

    dashboardServer.Stop();
    apiServer.Stop();
    dashboardServer.Wait();
    apiServer.Wait();

    // Shut down rendering and communications
    ptrController->Stop();
    ptrController->Disconnect();

    ptrController = nullptr;

    cout << "Shut down complete." << endl;

    return EXIT_SUCCESS;
}
