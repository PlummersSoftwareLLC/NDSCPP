// Main.cpp
//
// This file is the main entry point for the NDSCPP LED Matrix Server application.
// It creates a number of Canvases and adds an effect to each, after which they enter
// a loop where it renders the effect to the canvas, compresses the data, and sends
// it to the relevant LED controller via a SocketChannel.  The program will continue
// to run until it receives a SIGINT signal (Ctrl-C).

#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <atomic>
#include <stdexcept>
#include <string>
#include <memory>
#include <shared_mutex>
#include <thread>
#include <filesystem>
#include <optional>
#include <getopt.h>

using namespace std;
using namespace chrono;

#include "crow_all.h"
#include "global.h"
#include "canvas.h"
#include "socketchannel.h"
#include "ledfeature.h"
#include "webserver.h"
#include "controller.h"

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
    string filename = "config.led";

    // Parse command-line options
    int opt;
    const option longOptions[] =
    {
        {"port", required_argument, nullptr, 'p'},
        {"config", required_argument, nullptr, 'c'},
        {"help", no_argument, nullptr, 'h'},
        {nullptr, 0, nullptr, 0}
    };

    while ((opt = getopt_long(argc, argv, "p:c:h", longOptions, nullptr)) != -1)
    {
        switch (opt)
        {
            case 'p':
                apiPortOverride = ParsePortOption(optarg, "Port");
                break;
            case 'c':
                filename = optarg;
                break;
            case 'h':
                cerr << "Usage: " << argv[0] << " [-p <port>] [-c <configfile>]" << endl;
                return EXIT_SUCCESS;
            default:
                cerr << "Usage: " << argv[0] << " [-p <port>] [-c <configfile>]" << endl;
                return EXIT_FAILURE;
        }
    }

    // Load the canvases from the configuration file or use hard-coded table defaults
    // depending on USE_DEMO_DATA being defined or not.

    #define USE_DEMO_DATA 0

    #if USE_DEMO_DATA
        unique_ptr<Controller> ptrController = make_unique<Controller>(7777);
        ptrController->LoadSampleCanvases();
    #else
        unique_ptr<Controller> ptrController = Controller::CreateFromFile(filename);
    #endif

    if (apiPortOverride)
        ptrController->SetPort(*apiPortOverride);

    signal(SIGINT, HandleSignal);
    signal(SIGTERM, HandleSignal);


    ptrController->Connect();
    ptrController->Start(true); // Consider if effect managers want to run

    const auto executableDir = filesystem::weakly_canonical(filesystem::absolute(argv[0])).parent_path();
    const auto assetRoot = executableDir / "webui";

    crow::logger::setLogLevel(crow::LogLevel::WARNING);
    shared_mutex apiMutex;
    WebServer server(*ptrController.get(), filename, apiMutex, assetRoot);

    try
    {
        server.StartAsync();
    }
    catch (const exception &e)
    {
        logger->error("Failed to start server: {}", e.what());
        server.Stop();
        server.Wait();
        ptrController->Stop();
        ptrController->Disconnect();
        return EXIT_FAILURE;
    }

    logger->info("Dashboard listening on http://localhost:{}/", ptrController->GetPort());
    logger->info("API listening on http://localhost:{}/api", ptrController->GetPort());

    while (!gShouldExit)
        this_thread::sleep_for(100ms);

    cout << "Shutting down..." << endl;

    server.Stop();
    server.Wait();

    // Shut down rendering and communications
    ptrController->Stop();
    ptrController->Disconnect();

    ptrController = nullptr;

    cout << "Shut down complete." << endl;

    return EXIT_SUCCESS;
}
