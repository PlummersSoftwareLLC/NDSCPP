#include <curl/curl.h>
#include <chrono>
#include <thread>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <wchar.h>

#include "monitor.h"

using json = nlohmann::json;

const std::vector<std::pair<std::string, int>> COLUMNS =
    {
        {"Canvas", 10},   // Canvas name (matched to Feature width)
        {"Feature", 10},  // Feature name
        {"Host", 14},     // Hostname
        {"Size", 7},      // Dimensions
        {"Cx", 3},        // Reconnects
        {"FPS", 6},       // Frames per second (X/Y format)
        {"Queue", 5},     // Queue depth
        {"Buf", 8},       // Buffer usage
        {"Signal", 6},    // WiFi signal
        {"Data", 7},      // Bandwidth
        {"Delta", 12},    // Clock delta
        {"Flash", 5},     // Flash version
        {"Status", 7},    // Connection status
        {"Effect", 16}    // Current effect name
};

void Monitor::drawContent()
{
    werase(contentWin);

    try
    {
        std::string response = httpGet(baseUrl + "/api/canvases");
        auto j = json::parse(response);
        auto currentTime = system_clock::now().time_since_epoch().count() / 1000000.0; // Current time in ms

        int row = 0;
        for (const auto &canvasJson : j)
        {
            std::string canvasName = canvasJson["name"].get<std::string>();
            int canvasFps = canvasJson["effectsManager"]["fps"].get<int>();
            std::string currentEffect = canvasJson.contains("currentEffectName")
                                          ? canvasJson.at("currentEffectName").get<std::string>()
                                          : "---";

            if (currentEffect.length() > (size_t)COLUMNS[13].second)
                currentEffect = currentEffect.substr(0, COLUMNS[13].second);

            for (const auto &featureJson : canvasJson["features"])
            {
                bool isConnected = featureJson["isConnected"].get<bool>();

                if (row >= scrollOffset && row < scrollOffset + contentHeight)
                {
                    int x = 1;

                    // Canvas name
                    std::string displayName = canvasName;
                    if (displayName.length() > (size_t)COLUMNS[0].second)
                        displayName = displayName.substr(0, COLUMNS[0].second);
                    wattron(contentWin, A_BOLD | COLOR_PAIR(4)); // Assuming 5 is your cyan color pair
                    mvwprintw(contentWin, row - scrollOffset, x, "%-*s", COLUMNS[0].second, displayName.c_str());
                    wattroff(contentWin, A_BOLD | COLOR_PAIR(4));
                    x += COLUMNS[0].second + 1;

                    // Feature name
                    std::string featureName = featureJson["friendlyName"].get<std::string>();
                    if (featureName.length() > (size_t)COLUMNS[1].second)
                        featureName = featureName.substr(0, COLUMNS[1].second);
                    mvwprintw(contentWin, row - scrollOffset, x, "%-*s", COLUMNS[1].second, featureName.c_str());
                    x += COLUMNS[1].second + 1;

                    // Hostname
                    mvwprintw(contentWin, row - scrollOffset, x, "%-*s", COLUMNS[2].second, featureJson["hostName"].get<std::string>().c_str());
                    x += COLUMNS[2].second + 1;


                    std::string size = std::to_string(featureJson["width"].get<size_t>()) + "x" +
                                       std::to_string(featureJson["height"].get<size_t>());
                    mvwprintw(contentWin, row - scrollOffset, x, "%-*s",
                              COLUMNS[3].second, size.c_str());
                    x += COLUMNS[3].second + 1;

                    // Reconnects count
                    if (featureJson.contains("reconnectCount") && !featureJson["reconnectCount"].is_null())
                    {
                        int reconnects = featureJson["reconnectCount"].get<int>();
                        int colorPair = (reconnects < 3) ? 1 : // Green for 0
                                        (reconnects < 10) ? 6 :    // Yellow for 1-2
                                        2;                        // Red for 3+
                        wattron(contentWin, COLOR_PAIR(colorPair));
                        mvwprintw(contentWin, row - scrollOffset, x, "%-*d", COLUMNS[4].second, reconnects);
                        wattroff(contentWin, COLOR_PAIR(colorPair));
                    } else {
                        mvwprintw(contentWin, row - scrollOffset, x, "%-*s", COLUMNS[4].second, "---");
                    }
                    x += COLUMNS[4].second + 1;

                    // FPS (only if connected and has client response)
                    if (isConnected && featureJson.contains("lastClientResponse"))
                    {
                        const auto &stats = featureJson["lastClientResponse"];
                        int featureFps = stats["fpsDrawing"].get<int>();

                        int colorPair = (featureFps < 0.8 * canvasFps) ? 6 : 1;
                        wattron(contentWin, COLOR_PAIR(colorPair));
                        mvwprintw(contentWin, row - scrollOffset, x, "%d", featureFps);
                        wattroff(contentWin, COLOR_PAIR(colorPair));

                        mvwprintw(contentWin, row - scrollOffset, x + std::to_string(featureFps).length(), "/%d", canvasFps);

                        int totalWidth = std::to_string(featureFps).length() + 1 + std::to_string(canvasFps).length();
                        mvwprintw(contentWin, row - scrollOffset, x + totalWidth, "%*s", COLUMNS[5].second - totalWidth, "");
                    }
                    else
                    {
                        mvwprintw(contentWin, row - scrollOffset, x, "%-*s", COLUMNS[5].second, "---");
                    }
                    x += COLUMNS[5].second + 1;

                    // Queue depth
                    if (featureJson.contains("queueDepth") && !featureJson["queueDepth"].is_null())
                    {
                        constexpr auto kFatQueue = 25;
                        try
                        {
                            size_t queueDepth = featureJson["queueDepth"].get<size_t>();
                            int queueColor = queueDepth < 100 ? 1 :
                                           queueDepth < 250 ? 6 : 2;

                            std::wstring bar = buildProgressBar(queueDepth, kFatQueue, 6);
                            wattron(contentWin, COLOR_PAIR(queueColor));
                            mvwaddwstr(contentWin, row - scrollOffset, x, bar.c_str());
                            // If the queue is too big to show by bar alone, add a numeric indicator atop it
                            if (queueDepth > kFatQueue)
                            {
                                wattron(contentWin, A_REVERSE);
                                mvwprintw(contentWin, row - scrollOffset, x, " %-*zu", COLUMNS[6].second, queueDepth);
                                wattroff(contentWin, A_REVERSE);
                            }
                            wattroff(contentWin, COLOR_PAIR(queueColor));
                        }
                        catch (const json::exception &)
                        {
                            mvwprintw(contentWin, row - scrollOffset, x, "%-*s", COLUMNS[6].second, "---");
                        }
                    }
                    else
                    {
                        mvwprintw(contentWin, row - scrollOffset, x, "%-*s", COLUMNS[6].second, "---");
                    }
                    x += COLUMNS[6].second + 1;

                    // Rest of columns for connected devices
                    if (isConnected && featureJson.contains("lastClientResponse"))
                    {
                        const auto &stats = featureJson["lastClientResponse"];

                        // Buffer usage
                        size_t bufferPos = stats["bufferPos"].get<size_t>();
                        size_t bufferSize = stats["bufferSize"].get<size_t>();
                        double ratio = static_cast<double>(bufferPos) / bufferSize;

                        int colorPair = (ratio >= 0.25 && ratio <= 0.85) ? 1 :
                                      (ratio > 0.95) ? 2 : 3;

                        wattron(contentWin, COLOR_PAIR(colorPair));
                        mvwprintw(contentWin, row - scrollOffset, x, "%zu", bufferPos);
                        wattroff(contentWin, COLOR_PAIR(colorPair));

                        mvwprintw(contentWin, row - scrollOffset, x + std::to_string(bufferPos).length(), "/%zu", bufferSize);
                        x += COLUMNS[7].second + 1;

                        // WiFi Signal
                        double signal = std::abs(stats["wifiSignal"].get<double>());
                        int signalColor = (signal >= 100) ? 0 :
                                        (signal < 70) ? 1 :
                                        (signal < 80) ? 6 : 2;

                        wattron(contentWin, COLOR_PAIR(signalColor));
                        mvwprintw(contentWin, row - scrollOffset, x, "%-*s", COLUMNS[8].second, formatWifiSignal(stats["wifiSignal"].get<double>()).c_str());
                        wattroff(contentWin, COLOR_PAIR(signalColor));
                        x += COLUMNS[8].second + 1;

                        // Bandwidth
                        mvwprintw(contentWin, row - scrollOffset, x, "%-*s", COLUMNS[9].second, formatBytes(featureJson["bytesPerSecond"].get<double>()).c_str());
                        x += COLUMNS[9].second + 1;

                        // Clock Delta
                        if (stats.contains("currentClock"))
                        {
                            try
                            {
                                double clockDelta = stats["currentClock"].get<double>() - currentTime;
                                double deltaSeconds = std::abs(clockDelta);
                                int deltaColor = deltaSeconds < 2.0 ? 1 :
                                               deltaSeconds < 3.0 ? 6 : 3;

                                std::string deltaStr = formatTimeDelta(clockDelta);
                                wattron(contentWin, COLOR_PAIR(deltaColor));
                                mvwprintw(contentWin, row - scrollOffset, x, "%-*s", COLUMNS[10].second, deltaStr.c_str());
                                wattroff(contentWin, COLOR_PAIR(deltaColor));
                            }
                            catch (const json::exception &)
                            {
                                mvwprintw(contentWin, row - scrollOffset, x, "%-*s", COLUMNS[10].second, "---");
                            }
                        }
                        else
                        {
                            mvwprintw(contentWin, row - scrollOffset, x, "%-*s", COLUMNS[10].second, "---");
                        }
                        x += COLUMNS[10].second + 1;

                        // Flash Version
                        if (stats.contains("flashVersion"))
                        {
                            try
                            {
                                std::string flashVer = "v";
                                if (stats["flashVersion"].is_string())
                                    flashVer += stats["flashVersion"].get<std::string>();
                                else if (stats["flashVersion"].is_number())
                                    flashVer += std::to_string(stats["flashVersion"].get<int>());
                                else
                                    flashVer = "---";
                                mvwprintw(contentWin, row - scrollOffset, x, "%-*s", COLUMNS[11].second, flashVer.c_str());
                            }
                            catch (const json::exception &)
                            {
                                mvwprintw(contentWin, row - scrollOffset, x, "%-*s", COLUMNS[11].second, "---");
                            }
                        }
                        else
                        {
                            mvwprintw(contentWin, row - scrollOffset, x, "%-*s", COLUMNS[11].second, "---");
                        }
                        x += COLUMNS[11].second + 1;

                        // Connection status
                        wattron(contentWin, COLOR_PAIR(1));
                        mvwprintw(contentWin, row - scrollOffset, x, "%-*s", COLUMNS[12].second, "ONLINE");
                        wattroff(contentWin, COLOR_PAIR(1));
                    }
                    else
                    {
                        // Fill empty spaces for offline devices
                        for (size_t i = 7; i <= 11; i++)
                        {
                            mvwprintw(contentWin, row - scrollOffset, x, "%-*s", COLUMNS[i].second, "---");
                            x += COLUMNS[i].second + 1;
                        }

                        // Connection status
                        wattron(contentWin, COLOR_PAIR(2));
                        mvwprintw(contentWin, row - scrollOffset, x, "%-*s", COLUMNS[12].second, "OFFLINE");
                        wattroff(contentWin, COLOR_PAIR(2));
                    }
                    // Current Effect
                    x += COLUMNS[12].second + 1;
                    mvwprintw(contentWin, row - scrollOffset, x, "%-*s", COLUMNS[13].second, currentEffect.c_str());

                }
                row++;
            }
        }
    }
    catch (const std::exception &e)
    {
        mvwprintw(contentWin, 0, 1, "Error fetching data: %s", e.what());
    }

    wrefresh(contentWin);
}
