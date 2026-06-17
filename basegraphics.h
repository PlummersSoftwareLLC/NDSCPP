#pragma once
using namespace std;

// BaseGraphics
//
// A class that can do all the basic drawing functions (pixel, line, circle, etc) of the ILEDGraphics
// interface to its own internal buffer of pixels.  It manipulates the buffer only through SetPixel

#include <vector>
#include <execution>

#include "pixeltypes.h" // Assuming this defines the CRGB structure
#include "interfaces.h"

class BaseGraphics : public ILEDGraphics
{
protected:
    uint32_t _width;
    uint32_t _height;
    vector<CRGB> _pixels;

    virtual inline __attribute__((always_inline)) uint32_t _index(uint32_t x, uint32_t y) const
    {
        return y * _width + x;
    }

    constexpr inline bool _isInBounds(uint32_t x, uint32_t y) const
    {
        return (x < _width && y < _height);
    }

public:
    explicit BaseGraphics(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0)
            throw invalid_argument("Width and height must be greater than 0");
        if (static_cast<uint64_t>(width) * height > numeric_limits<size_t>::max() / sizeof(CRGB))
            throw overflow_error("Requested dimensions too large");

        _width = width;
        _height = height;
        _pixels.resize(static_cast<size_t>(width) * height);
    }

    // ICanvas methods
    uint32_t Width()  const override { return _width; }
    uint32_t Height() const override { return _height; }

    void FadePixelToBlackBy(uint32_t x, uint32_t y, float amount) override
    {
        if (_isInBounds(x, y))
        {
            CRGB& pixel = _pixels[_index(x, y)];
            pixel.nscale8(amount * 255);
        }
    }

    const vector<CRGB>& GetPixels() const override
    {
        return _pixels;
    }

    void SetPixel(uint32_t x, uint32_t y, const CRGB& color) override
    {
        if (_isInBounds(x, y))
            _pixels[_index(x, y)] = color;
    }

    CRGB GetPixel(uint32_t x, uint32_t y) const override
    {
        if (_isInBounds(x, y))
            return _pixels[_index(x, y)];
        return CRGB(0, 0, 0); // Default to black for out-of-bounds
    }

    void Clear(const CRGB& color = CRGB::Black) override
    {
        FillRectangle(0, 0, _width, _height, color);
    }

    void FillRectangle(uint32_t x, uint32_t y, uint32_t width, uint32_t height, const CRGB& color) override
    {
        if (x >= _width || y >= _height) return;
        width = min(width, _width - x);
        height = min(height, _height - y);

        if (color == CRGB::Black) {
            for (uint32_t j = y; j < y + height; ++j)
                memset(&_pixels[_index(x, j)], 0, width * sizeof(CRGB));
        } else {
            for (uint32_t j = y; j < y + height; ++j)
                fill(&_pixels[_index(x, j)], &_pixels[_index(x + width, j)], color);
        }
    }

    void DrawLine(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, const CRGB& color) override
    {
        int32_t dx = abs((int32_t)x2 - (int32_t)x1), dy = abs((int32_t)y2 - (int32_t)y1);
        int32_t sx = (x1 < x2) ? 1 : -1, sy = (y1 < y2) ? 1 : -1;
        int32_t err = dx - dy;

        while (true)
        {
            SetPixel(x1, y1, color);
            if (x1 == x2 && y1 == y2) break;
            int e2 = 2 * err;
            if (e2 > -dy)
            {
                err -= dy;
                x1 += sx;
            }
            if (e2 < dx)
            {
                err += dx;
                y1 += sy;
            }
        }
    }

    void DrawCircle(uint32_t x, uint32_t y, uint32_t radius, const CRGB& color) override
    {
        uint32_t cx = 0, cy = radius;
        int32_t  d = 1 - radius;

        while (cy >= cx)
        {
            SetPixel(x + cx, y + cy, color);
            SetPixel(x - cx, y + cy, color);
            SetPixel(x + cx, y - cy, color);
            SetPixel(x - cx, y - cy, color);
            SetPixel(x + cy, y + cx, color);
            SetPixel(x - cy, y + cx, color);
            SetPixel(x + cy, y - cx, color);
            SetPixel(x - cy, y - cx, color);

            ++cx;
            if (d < 0)
            {
                d += 2 * cx + 1;
            }
            else
            {
                --cy;
                d += 2 * (cx - cy) + 1;
            }
        }
    }

    void FillCircle(uint32_t x, uint32_t y, uint32_t radius, const CRGB& color) override
    {
        // Fill within the bounding box, but only those pixels within radius of the center

        for (uint32_t cy = -radius; cy <= radius; ++cy)
            for (uint32_t cx = -radius; cx <= radius; ++cx)
                if (cx * cx + cy * cy <= radius * radius)
                    SetPixel(x + cx, y + cy, color);
    }

    void DrawRectangle(uint32_t x, uint32_t y, uint32_t width, uint32_t height, const CRGB& color) override
    {
        DrawLine(x, y, x + width - 1, y, color);                            // Top
        DrawLine(x, y, x, y + height - 1, color);                           // Left
        DrawLine(x + width - 1, y, x + width - 1, y + height - 1, color);   // Right
        DrawLine(x, y + height - 1, x + width - 1, y + height - 1, color);  // Bottom
    }

    void FadeFrameBy(uint8_t dimAmount) override
    {
        const uint8_t scale = 255 - dimAmount;
        for_each(_pixels.begin(), _pixels.end(),
            [scale](CRGB& p) {
                p.r = (p.r * scale) >> 8;
                p.g = (p.g * scale) >> 8;
                p.b = (p.b * scale) >> 8;
            });
    }

    void SetPixelsF(float fPos, float count, CRGB c, bool bMerge = false) override
    {
        // Early exit for empty ranges or out-of-bounds start positions
        if (count <= 0 || fPos >= _pixels.size() || fPos + count <= 0)
            return;

        // Pre-calculate common values
        const size_t arraySize = _pixels.size();
        const size_t startIdx = max(0UL, static_cast<size_t>(floor(fPos)));
        const size_t endIdx = min(arraySize, static_cast<size_t>(ceil(fPos + count)));
        const float frac1 = fPos - floor(fPos);
        const uint8_t fade1 = static_cast<uint8_t>((max(frac1, 1.0f - count)) * 255);
        float remainingCount = count - (1.0f - frac1);
        const float lastFrac = remainingCount - floor(remainingCount);
        const uint8_t fade2 = static_cast<uint8_t>((1.0f - lastFrac) * 255);

        if (!bMerge)
        {
            // Non-merging implementation

            if (startIdx < arraySize)
            {
                CRGB c1 = c;
                c1.fadeToBlackBy(fade1);
                _pixels[startIdx] = c1;
            }

            // Middle pixels - use pointer arithmetic for speed
            CRGB *pixel = &_pixels[startIdx + 1];
            const CRGB *end = &_pixels[endIdx - 1];
            while (pixel < end)
                *pixel++ = c;

            // Last pixel if needed
            if (lastFrac > 0 && endIdx - 1 < arraySize)
            {
                CRGB c2 = c;
                c2.fadeToBlackBy(fade2);
                _pixels[endIdx - 1] = c2;
            }
        }
        else
        {
            // Merging implementation
            // First pixel
            if (startIdx < arraySize)
            {
                CRGB c1 = c;
                c1.fadeToBlackBy(fade1);
                _pixels[startIdx] += c1;
            }

            // Middle pixels - use pointer arithmetic for speed
            CRGB *pixel = &_pixels[startIdx + 1];
            const CRGB *end = &_pixels[endIdx - 1];
            while (pixel < end)
                *pixel++ += c;

            // Last pixel if needed
            if (lastFrac > 0 && endIdx - 1 < arraySize)
            {
                CRGB c2 = c;
                c2.fadeToBlackBy(fade2);
                _pixels[endIdx - 1] += c2;
            }
        }
    }
};
