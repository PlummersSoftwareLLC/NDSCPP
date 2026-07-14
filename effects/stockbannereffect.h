#pragma once

using namespace std;
using namespace std::chrono;

#include "../interfaces.h"
#include "../ledeffectbase.h"
#include "../pixeltypes.h"
#include "../secrets.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <cmath>
#include <future>
#include <iomanip>
#include <limits>
#include <netdb.h>
#include <netinet/in.h>
#include <optional>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>
#include <vector>

class StockBanner : public LEDEffectBase
{
public:
    static constexpr const char* TypeName = "StockBanner";

private:
    struct Quote
    {
        string symbol;
        double price = 0.0;
        double change = 0.0;
        double changePercent = 0.0;
        bool stale = false;
        bool valid = false;
        string marketState = "closed";
        string error;
        steady_clock::time_point lastFetch = steady_clock::time_point::min();
    };

    struct FetchResult
    {
        string symbol;
        bool ok = false;
        double price = 0.0;
        double change = 0.0;
        double changePercent = 0.0;
        bool stale = false;
        string marketState = "closed";
        string error;
    };

    vector<string> _symbols;
    vector<Quote> _quotes;
    string _stockServerHost = "127.0.0.1";
    uint16_t _stockServerPort = 8888;
    uint32_t _scrollOffset = 0;
    uint32_t _minQuoteWidth = 64;
    uint32_t _compactQuoteWidth = 96;
    uint32_t _quoteGap = 10;
    milliseconds _refreshInterval = 60s;
    milliseconds _requestSpacing = 250ms;
    milliseconds _requestTimeout = 1500ms;
    steady_clock::time_point _lastRequestStart = steady_clock::time_point::min();
    size_t _nextFetchIndex = 0;

    vector<CRGB> _stripPixels;
    uint32_t _stripWidth = 0;
    uint32_t _stripHeight = 0;
    bool _stripDirty = true;

    future<FetchResult> _pendingFetch;

    static vector<string> DefaultSymbols()
    {
        return {
            "AAPL", "ALK", "AMC", "AMZN", "BA", "BYDDY", "CAD", "CMOPF",
            "CREX", "FBGRX", "FSPTX", "FXAIX", "GE", "GEHC", "GEV", "GLD",
            "INTC", "META", "MSFT", "NVS", "PLTM", "QQQ", "RIVN", "SBUX",
            "SPCX", "TSLA", "WMT"
        };
    }

    static string UpperSymbol(string symbol)
    {
        transform(symbol.begin(), symbol.end(), symbol.begin(), [](unsigned char c)
        {
            return static_cast<char>(toupper(c));
        });
        return symbol;
    }

    static constexpr char GlyphKey(char c)
    {
        return c >= 'a' && c <= 'z'
            ? static_cast<char>(c - 'a' + 'A')
            : c;
    }

    static constexpr array<uint8_t, 7> GlyphRows(char c)
    {
        switch (GlyphKey(c))
        {
            case '0': return {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
            case '1': return {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E};
            case '2': return {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
            case '3': return {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E};
            case '4': return {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
            case '5': return {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E};
            case '6': return {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E};
            case '7': return {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
            case '8': return {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
            case '9': return {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E};

            case 'A': return {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
            case 'B': return {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
            case 'C': return {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
            case 'D': return {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E};
            case 'E': return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
            case 'F': return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
            case 'G': return {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F};
            case 'H': return {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
            case 'I': return {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E};
            case 'J': return {0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C};
            case 'K': return {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
            case 'L': return {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
            case 'M': return {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
            case 'N': return {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
            case 'O': return {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
            case 'P': return {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
            case 'Q': return {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D};
            case 'R': return {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
            case 'S': return {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
            case 'T': return {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
            case 'U': return {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
            case 'V': return {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04};
            case 'W': return {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A};
            case 'X': return {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11};
            case 'Y': return {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04};
            case 'Z': return {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F};

            case '$': return {0x04, 0x0F, 0x14, 0x0E, 0x05, 0x1E, 0x04};
            case '+': return {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00};
            case '-': return {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};
            case '.': return {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C};
            case '%': return {0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03};
            case '*': return {0x00, 0x15, 0x0E, 0x1F, 0x0E, 0x15, 0x00};
            case ' ': return {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
            default: return {0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04};
        }
    }

    static int GlyphPixelWidth(char c)
    {
        return c == ' ' ? 3 : 5;
    }

    static int TextWidth(const string& text, int scale)
    {
        if (text.empty())
            return 0;

        int width = 0;
        for (size_t i = 0; i < text.size(); ++i)
        {
            width += GlyphPixelWidth(text[i]) * scale;
            if (i + 1 < text.size())
                width += scale;
        }
        return width;
    }

    static int FontHeight(int scale)
    {
        return 7 * scale;
    }

    static int LargestTextScale(const string& text, int maxWidth, int maxHeight)
    {
        int best = 1;
        for (int scale = 1; scale <= 4; ++scale)
        {
            if (FontHeight(scale) <= maxHeight && TextWidth(text, scale) <= maxWidth)
                best = scale;
        }
        return best;
    }

    static CRGB Scaled(CRGB color, uint8_t amount)
    {
        color.nscale8(amount);
        return color;
    }

    static CRGB MarketStateColor(const string& state)
    {
        if (state == "open")
            return CRGB(0, 220, 92);
        if (state == "pre")
            return CRGB(0, 185, 255);
        if (state == "post")
            return CRGB(255, 160, 0);
        return CRGB(75, 85, 100);
    }

    static CRGB ChangeColor(double change, bool stale)
    {
        if (stale)
            return CRGB(180, 148, 60);
        if (change > 0.005)
            return CRGB(0, 245, 92);
        if (change < -0.005)
            return CRGB(255, 54, 68);
        return CRGB(190, 198, 210);
    }

    static string FormatMoney(double value)
    {
        ostringstream stream;
        stream << "$" << fixed << setprecision(value >= 1000.0 ? 0 : 2) << value;
        return stream.str();
    }

    static string FormatChange(double value)
    {
        ostringstream stream;
        if (value >= 0.0)
            stream << "+";
        stream << fixed << setprecision(abs(value) >= 100.0 ? 1 : 2) << value;
        return stream.str();
    }

    static string ConfiguredRealtimeQuoteKey()
    {
        #if defined(cszRealtimeQuoteKey)
            return string(cszRealtimeQuoteKey);
        #elif defined(cszRealtiemQuoteKey)
            return string(cszRealtiemQuoteKey);
        #else
            return "";
        #endif
    }

    static bool HasHttpHeaderBreak(const string& value)
    {
        return value.find_first_of("\r\n") != string::npos;
    }

    static void SetBufferPixel(vector<CRGB>& buffer, uint32_t width, uint32_t height, int x, int y, CRGB color, bool blend = false)
    {
        if (x < 0 || y < 0 || x >= static_cast<int>(width) || y >= static_cast<int>(height))
            return;

        auto& pixel = buffer[static_cast<size_t>(y) * width + static_cast<size_t>(x)];
        if (blend)
            pixel += color;
        else
            pixel = color;
    }

    static void DrawText(vector<CRGB>& buffer, uint32_t width, uint32_t height, int x, int y, const string& text, int scale, CRGB color, bool shadow)
    {
        if (shadow)
            DrawText(buffer, width, height, x + max(1, scale / 2), y + max(1, scale / 2), text, scale, Scaled(color, 42), false);

        int cursorX = x;
        for (char ch : text)
        {
            const auto& rows = GlyphRows(ch);
            for (int row = 0; row < 7; ++row)
            {
                for (int col = 0; col < 5; ++col)
                {
                    if ((rows[row] & (1 << (4 - col))) == 0)
                        continue;

                    for (int sy = 0; sy < scale; ++sy)
                        for (int sx = 0; sx < scale; ++sx)
                            SetBufferPixel(buffer, width, height, cursorX + col * scale + sx, y + row * scale + sy, color, true);
                }
            }
            cursorX += (GlyphPixelWidth(ch) + 1) * scale;
        }
    }

    static string HttpGet(const string& host,
                          uint16_t port,
                          const string& path,
                          milliseconds timeout,
                          const vector<pair<string, string>>& headers = {})
    {
        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        addrinfo* addresses = nullptr;
        const string portString = to_string(port);
        const int gai = getaddrinfo(host.c_str(), portString.c_str(), &hints, &addresses);
        if (gai != 0)
            throw runtime_error(string("resolve failed: ") + gai_strerror(gai));

        int fd = -1;
        string lastError;
        for (auto* address = addresses; address != nullptr; address = address->ai_next)
        {
            fd = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
            if (fd < 0)
            {
                lastError = strerror(errno);
                continue;
            }

            timeval tv{};
            tv.tv_sec = static_cast<long>(timeout.count() / 1000);
            tv.tv_usec = static_cast<int>((timeout.count() % 1000) * 1000);
            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

            if (connect(fd, address->ai_addr, address->ai_addrlen) == 0)
                break;

            lastError = strerror(errno);
            close(fd);
            fd = -1;
        }
        freeaddrinfo(addresses);

        if (fd < 0)
            throw runtime_error("connect failed: " + lastError);

        string request =
            "GET " + path + " HTTP/1.1\r\n"
            "Host: " + host + ":" + to_string(port) + "\r\n"
            "Connection: close\r\n"
            "Accept: application/json\r\n";

        for (const auto& [name, value] : headers)
        {
            if (name.empty() || value.empty())
                continue;
            if (HasHttpHeaderBreak(name) || HasHttpHeaderBreak(value))
                throw runtime_error("invalid HTTP header");

            request += name + ": " + value + "\r\n";
        }

        request += "\r\n";

        size_t sent = 0;
        while (sent < request.size())
        {
            ssize_t n = send(fd, request.data() + sent, request.size() - sent, 0);
            if (n <= 0)
            {
                string error = strerror(errno);
                close(fd);
                throw runtime_error("send failed: " + error);
            }
            sent += static_cast<size_t>(n);
        }

        string response;
        array<char, 4096> buffer{};
        while (true)
        {
            ssize_t n = recv(fd, buffer.data(), buffer.size(), 0);
            if (n == 0)
                break;
            if (n < 0)
            {
                string error = strerror(errno);
                close(fd);
                throw runtime_error("recv failed: " + error);
            }
            response.append(buffer.data(), static_cast<size_t>(n));
            if (response.size() > 128 * 1024)
                break;
        }
        close(fd);

        const auto headerEnd = response.find("\r\n\r\n");
        if (headerEnd == string::npos)
            throw runtime_error("invalid HTTP response");

        const auto statusEnd = response.find("\r\n");
        const string statusLine = response.substr(0, statusEnd);
        if (statusLine.find(" 200 ") == string::npos)
            throw runtime_error(statusLine);

        return response.substr(headerEnd + 4);
    }

    FetchResult FetchQuote(string symbol) const
    {
        FetchResult result;
        result.symbol = symbol;

        try
        {
            const string path = "/?ticker=" + symbol + "&v=2&profile=hub75&points=1";
            vector<pair<string, string>> headers;
            const string realtimeQuoteKey = ConfiguredRealtimeQuoteKey();
            if (!realtimeQuoteKey.empty())
                headers.emplace_back("X-Stockserver-Realtime-Key", realtimeQuoteKey);

            const auto body = HttpGet(_stockServerHost, _stockServerPort, path, _requestTimeout, headers);
            const auto json = nlohmann::json::parse(body);

            result.price = json.at("p").get<double>();
            result.change = json.value("chg", 0.0);
            result.changePercent = json.value("chgp", 0.0);
            result.stale = json.value("stale", false);
            result.marketState = json.value("st", string("closed"));
            result.ok = true;
        }
        catch (const exception& e)
        {
            result.error = e.what();
        }

        return result;
    }

    void EnsureQuoteList()
    {
        if (_symbols.empty())
            _symbols = DefaultSymbols();

        vector<Quote> newQuotes;
        newQuotes.reserve(_symbols.size());
        for (const auto& rawSymbol : _symbols)
        {
            const auto symbol = UpperSymbol(rawSymbol);
            auto it = find_if(_quotes.begin(), _quotes.end(), [&symbol](const Quote& quote)
            {
                return quote.symbol == symbol;
            });

            if (it != _quotes.end())
                newQuotes.push_back(*it);
            else
                newQuotes.push_back(Quote{.symbol = symbol});
        }

        _quotes = std::move(newQuotes);
        _symbols.clear();
        for (const auto& quote : _quotes)
            _symbols.push_back(quote.symbol);
    }

    optional<size_t> NextQuoteToFetch(steady_clock::time_point now)
    {
        if (_quotes.empty())
            return nullopt;

        for (size_t attempt = 0; attempt < _quotes.size(); ++attempt)
        {
            const size_t index = (_nextFetchIndex + attempt) % _quotes.size();
            const auto& quote = _quotes[index];
            if (quote.lastFetch == steady_clock::time_point::min() || now - quote.lastFetch >= _refreshInterval)
            {
                _nextFetchIndex = (index + 1) % _quotes.size();
                return index;
            }
        }

        return nullopt;
    }

    void PollFetches()
    {
        if (!_pendingFetch.valid())
            return;

        if (_pendingFetch.wait_for(0ms) != future_status::ready)
            return;

        auto result = _pendingFetch.get();
        auto now = steady_clock::now();
        auto it = find_if(_quotes.begin(), _quotes.end(), [&result](const Quote& quote)
        {
            return quote.symbol == result.symbol;
        });
        if (it == _quotes.end())
            return;

        it->lastFetch = now;
        if (result.ok)
        {
            it->price = result.price;
            it->change = result.change;
            it->changePercent = result.changePercent;
            it->stale = result.stale;
            it->marketState = result.marketState;
            it->valid = true;
            it->error.clear();
        }
        else
        {
            it->valid = false;
            it->stale = true;
            it->error = result.error;
            logger->warn("StockBanner quote fetch failed for {}: {}", result.symbol, result.error);
        }

        _stripDirty = true;
    }

    void MaybeStartFetch()
    {
        if (_pendingFetch.valid())
            return;

        const auto now = steady_clock::now();
        if (_lastRequestStart != steady_clock::time_point::min() &&
            now - _lastRequestStart < _requestSpacing)
        {
            return;
        }

        auto index = NextQuoteToFetch(now);
        if (!index)
            return;

        const string symbol = _quotes[*index].symbol;
        _lastRequestStart = now;
        _pendingFetch = async(launch::async, [this, symbol]()
        {
            return FetchQuote(symbol);
        });
    }

    string BottomPriceText(const Quote& quote) const
    {
        if (!quote.valid)
            return quote.error.empty() ? "LOADING" : "NO DATA";

        return FormatMoney(quote.price);
    }

    string BottomChangeText(const Quote& quote) const
    {
        if (!quote.valid)
            return "";

        if (quote.stale)
            return "STALE";

        return FormatChange(quote.change);
    }

    int RenderQuoteTile(uint32_t x, const Quote& quote)
    {
        const uint32_t height = _stripHeight;
        const int halfHeight = max(8, static_cast<int>(height / 2));
        const int bottomTop = halfHeight;
        const int bottomHeight = max(1, static_cast<int>(height) - bottomTop);
        const string symbol = quote.symbol;
        const string priceText = BottomPriceText(quote);
        const string changeText = BottomChangeText(quote);

        const int topScale = LargestTextScale(symbol, static_cast<int>(_compactQuoteWidth) - 10, halfHeight - 2);
        const int bottomTextWidth = TextWidth(priceText, 1) + (changeText.empty() ? 0 : 4 + TextWidth(changeText, 1));
        const int bottomScale = LargestTextScale(priceText + (changeText.empty() ? "" : " " + changeText),
                                                 static_cast<int>(_compactQuoteWidth) - 8,
                                                 bottomHeight - 2);
        const int scaledBottomWidth = TextWidth(priceText, bottomScale) +
                                      (changeText.empty() ? 0 : (4 * bottomScale) + TextWidth(changeText, bottomScale));

        const int tileWidth = static_cast<int>(max<uint32_t>({
            _minQuoteWidth,
            static_cast<uint32_t>(TextWidth(symbol, topScale) + 12),
            static_cast<uint32_t>(max(bottomTextWidth, scaledBottomWidth) + 10)
        }));

        const CRGB stateColor = MarketStateColor(quote.marketState);
        const CRGB symbolColor = quote.valid ? CRGB(230, 244, 255) : CRGB(80, 100, 120);
        const CRGB priceColor = quote.valid ? CRGB(190, 218, 255) : CRGB(90, 104, 120);
        const CRGB changeColor = ChangeColor(quote.change, quote.stale || !quote.valid);
        const int tileX = static_cast<int>(x);

        for (uint32_t y = 0; y < height; ++y)
        {
            const double fade = 0.25 + 0.18 * (static_cast<double>(y) / max(1U, height - 1));
            const CRGB shade(
                static_cast<uint8_t>(6 + stateColor.r * fade / 18.0),
                static_cast<uint8_t>(8 + stateColor.g * fade / 18.0),
                static_cast<uint8_t>(12 + stateColor.b * fade / 18.0));
            for (int px = 0; px < tileWidth; ++px)
                SetBufferPixel(_stripPixels, _stripWidth, _stripHeight, tileX + px, y, shade);
        }

        for (uint32_t y = 0; y < height; ++y)
        {
            SetBufferPixel(_stripPixels, _stripWidth, _stripHeight, tileX, y, Scaled(stateColor, 150));
            SetBufferPixel(_stripPixels, _stripWidth, _stripHeight, tileX + tileWidth - 1, y, CRGB(18, 24, 32));
        }
        for (int px = 2; px < tileWidth - 2; ++px)
        {
            SetBufferPixel(_stripPixels, _stripWidth, _stripHeight, tileX + px, halfHeight - 1, Scaled(stateColor, 32));
            SetBufferPixel(_stripPixels, _stripWidth, _stripHeight, tileX + px, static_cast<int>(height) - 1, Scaled(stateColor, 70));
        }

        const int symbolX = tileX + (tileWidth - TextWidth(symbol, topScale)) / 2;
        const int symbolY = (halfHeight - FontHeight(topScale)) / 2;
        DrawText(_stripPixels, _stripWidth, _stripHeight, symbolX, symbolY, symbol, topScale, symbolColor, true);

        const int totalBottomWidth = scaledBottomWidth;
        const int bottomX = tileX + (tileWidth - totalBottomWidth) / 2;
        const int bottomY = bottomTop + (bottomHeight - FontHeight(bottomScale)) / 2;
        DrawText(_stripPixels, _stripWidth, _stripHeight, bottomX, bottomY, priceText, bottomScale, priceColor, true);
        if (!changeText.empty())
        {
            const int changeX = bottomX + TextWidth(priceText, bottomScale) + 4 * bottomScale;
            DrawText(_stripPixels, _stripWidth, _stripHeight, changeX, bottomY, changeText, bottomScale, changeColor, true);
        }

        return tileWidth;
    }

    void RebuildStrip(uint32_t canvasWidth, uint32_t canvasHeight)
    {
        EnsureQuoteList();

        _stripHeight = max(1U, canvasHeight);

        uint32_t totalWidth = 0;
        for (const auto& quote : _quotes)
        {
            const int halfHeight = max(8, static_cast<int>(_stripHeight / 2));
            const int bottomHeight = max(1, static_cast<int>(_stripHeight) - halfHeight);
            const string priceText = BottomPriceText(quote);
            const string changeText = BottomChangeText(quote);
            const int topScale = LargestTextScale(quote.symbol, static_cast<int>(_compactQuoteWidth) - 10, halfHeight - 2);
            const int bottomScale = LargestTextScale(priceText + (changeText.empty() ? "" : " " + changeText),
                                                     static_cast<int>(_compactQuoteWidth) - 8,
                                                     bottomHeight - 2);
            const int width = static_cast<int>(max<uint32_t>({
                _minQuoteWidth,
                static_cast<uint32_t>(TextWidth(quote.symbol, topScale) + 12),
                static_cast<uint32_t>(TextWidth(priceText, bottomScale) +
                                      (changeText.empty() ? 0 : 4 * bottomScale + TextWidth(changeText, bottomScale)) + 10)
            }));
            totalWidth += static_cast<uint32_t>(width) + _quoteGap;
        }

        _stripWidth = max(totalWidth, canvasWidth + _minQuoteWidth);
        _stripPixels.assign(static_cast<size_t>(_stripWidth) * _stripHeight, CRGB::Black);

        uint32_t x = 0;
        for (const auto& quote : _quotes)
        {
            const int tileWidth = RenderQuoteTile(x, quote);
            x += static_cast<uint32_t>(tileWidth);
            for (uint32_t gap = 0; gap < _quoteGap && x + gap < _stripWidth; ++gap)
            {
                for (uint32_t y = 0; y < _stripHeight; ++y)
                    SetBufferPixel(_stripPixels, _stripWidth, _stripHeight, x + gap, y, CRGB(0, 0, 0));
            }
            x += _quoteGap;
        }

        _scrollOffset %= _stripWidth;
        _stripDirty = false;
    }

    void DrawWindow(ICanvas& canvas)
    {
        auto& graphics = canvas.Graphics();
        const uint32_t canvasWidth = graphics.Width();
        const uint32_t canvasHeight = graphics.Height();

        if (_stripDirty || _stripWidth == 0 || _stripHeight != canvasHeight)
            RebuildStrip(canvasWidth, canvasHeight);

        if (_stripPixels.empty())
        {
            graphics.Clear(CRGB::Black);
            return;
        }

        const uint32_t start = _scrollOffset % _stripWidth;
        for (uint32_t y = 0; y < canvasHeight; ++y)
        {
            for (uint32_t x = 0; x < canvasWidth; ++x)
            {
                const uint32_t srcX = (start + x) % _stripWidth;
                graphics.SetPixel(x, y, _stripPixels[static_cast<size_t>(y) * _stripWidth + srcX]);
            }
        }
    }

public:
    StockBanner(const string& name = "StockBanner",
                vector<string> symbols = DefaultSymbols(),
                string stockServerHost = "127.0.0.1",
                uint16_t stockServerPort = 8888,
                uint32_t minQuoteWidth = 64,
                uint32_t compactQuoteWidth = 96,
                uint32_t refreshSeconds = 60)
        : LEDEffectBase(name, TypeName),
          _symbols(std::move(symbols)),
          _stockServerHost(std::move(stockServerHost)),
          _stockServerPort(stockServerPort),
          _minQuoteWidth(minQuoteWidth),
          _compactQuoteWidth(max(compactQuoteWidth, minQuoteWidth)),
          _refreshInterval(seconds(refreshSeconds))
    {
        EnsureQuoteList();
    }

    void Start(ICanvas& canvas) override
    {
        _scrollOffset = 0;
        _nextFetchIndex = 0;
        _lastRequestStart = steady_clock::time_point::min();
        for (auto& quote : _quotes)
            quote.lastFetch = steady_clock::time_point::min();

        _stripDirty = true;
        MaybeStartFetch();
        RebuildStrip(canvas.Graphics().Width(), canvas.Graphics().Height());
    }

    void Update(ICanvas& canvas, microseconds deltaTime) override
    {
        (void) deltaTime;

        EnsureQuoteList();
        PollFetches();
        MaybeStartFetch();

        if (_stripWidth > 0)
            _scrollOffset = (_scrollOffset + 1) % _stripWidth;

        DrawWindow(canvas);
    }

    friend inline void to_json(nlohmann::json& j, const StockBanner& effect);
    friend inline void from_json(const nlohmann::json& j, shared_ptr<StockBanner>& effect);
};

inline void to_json(nlohmann::json& j, const StockBanner& effect)
{
    j = {
        {"name", effect.Name()},
        {"symbols", effect._symbols},
        {"stockServerHost", effect._stockServerHost},
        {"stockServerPort", effect._stockServerPort},
        {"minQuoteWidth", effect._minQuoteWidth},
        {"compactQuoteWidth", effect._compactQuoteWidth},
        {"refreshSeconds", static_cast<uint32_t>(duration_cast<seconds>(effect._refreshInterval).count())}
    };
}

inline void from_json(const nlohmann::json& j, shared_ptr<StockBanner>& effect)
{
    effect = make_shared<StockBanner>(
        j.value("name", string("StockBanner")),
        j.value("symbols", StockBanner::DefaultSymbols()),
        j.value("stockServerHost", string("127.0.0.1")),
        j.value("stockServerPort", uint16_t(8888)),
        j.value("minQuoteWidth", uint32_t(64)),
        j.value("compactQuoteWidth", uint32_t(96)),
        j.value("refreshSeconds", uint32_t(60))
    );
}
