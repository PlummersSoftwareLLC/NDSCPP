#pragma once
using namespace std;

// Utilities
//
// This class provides a number of utility functions that are used by the various
// classes in the project.  Most of them relate to managing the data that is sent
// to the ESP32.  The ESP32 expects a specific format for the data that is sent to
// it, and this class provides functions to convert the data into that format.

#include <vector>
#include <array>
#include <random>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <zlib.h>
#include "pixeltypes.h"

class Utilities
{
public:

    static constexpr float constexpr_sqrt(float x, float epsilon = 1e-5f)
    {
        float guess = x / 2.0f;
        float result = (guess + x / guess) / 2.0f;

        while ((result - guess) > epsilon || (result - guess) < -epsilon) 
        {
            guess = result;
            result = (guess + x / guess) / 2.0f;
        }

        return result;
    }

    static double RandomDouble(double min, double max)
    {
        static mt19937 rng(random_device{}());
        uniform_real_distribution<double> dist(min, max);
        return dist(rng);
    }

    static int RandomInt(int min, int max)
    {
        static mt19937 rng(random_device{}());
        uniform_int_distribution<int> dist(min, max);
        return dist(rng);
    }

    static vector<uint8_t> ConvertPixelsToByteArray(const vector<CRGB> &pixels, bool reversed, bool redGreenSwap)
    {
        vector<uint8_t> byteArray(pixels.size() * 3); // Allocate space upfront

        // This code makes all kinds of assumptions that CRGB is three RGB bytes, so let's assert that fact
        static_assert(sizeof(CRGB) == 3);

        if (!reversed && !redGreenSwap)
        {
            memcpy(byteArray.data(), pixels.data(), pixels.size() * 3);
            return byteArray;
        }

        size_t index = 0;
        if (reversed)
        {
            if (redGreenSwap)
            {
                for (auto it = pixels.rbegin(); it != pixels.rend(); ++it)
                {
                    byteArray[index++] = it->g;
                    byteArray[index++] = it->r;
                    byteArray[index++] = it->b;
                }
            }
            else
            {
                for (auto it = pixels.rbegin(); it != pixels.rend(); ++it)
                {
                    byteArray[index++] = it->r;
                    byteArray[index++] = it->g;
                    byteArray[index++] = it->b;
                }
            }
        }
        else
        {
            if (redGreenSwap)
            {
                for (const auto &pixel : pixels)
                {
                    byteArray[index++] = pixel.g;
                    byteArray[index++] = pixel.r;
                    byteArray[index++] = pixel.b;
                }
            }
            else
            {
                // This case is actually covered by the memcpy above, but kept for completeness
                // or if memcpy is somehow not preferred.
                for (const auto &pixel : pixels)
                {
                    byteArray[index++] = pixel.r;
                    byteArray[index++] = pixel.g;
                    byteArray[index++] = pixel.b;
                }
            }
        }

        return byteArray;
    }

    // The following XXXXToBytes functions produce a bytestream in the little-endian
    // that the original ESP32 code expects

    // Converts a uint16_t to an array of bytes in little-endian order
    static constexpr array<uint8_t, 2> WORDToBytes(uint16_t value)
    {
        if constexpr (endian::native == endian::little)
        {
            return {
                static_cast<uint8_t>(value & 0xFF),
                static_cast<uint8_t>((value >> 8) & 0xFF)};
        }
        else
        {
            return {
                static_cast<uint8_t>(__builtin_bswap16(value) & 0xFF),
                static_cast<uint8_t>((__builtin_bswap16(value) >> 8) & 0xFF)};
        }
        if constexpr (endian::native == endian::little)
        {
            return {
                static_cast<uint8_t>(value & 0xFF),
                static_cast<uint8_t>((value >> 8) & 0xFF)};
        }
        else
        {
            return {
                static_cast<uint8_t>(__builtin_bswap16(value) & 0xFF),
                static_cast<uint8_t>((__builtin_bswap16(value) >> 8) & 0xFF)};
        }
    }

    // Converts a uint32_t to an array of bytes in little-endian order
    static constexpr array<uint8_t, 4> DWORDToBytes(uint32_t value)
    {
        if constexpr (endian::native == endian::little)
        {
            return {
                static_cast<uint8_t>(value & 0xFF),
                static_cast<uint8_t>((value >> 8) & 0xFF),
                static_cast<uint8_t>((value >> 16) & 0xFF),
                static_cast<uint8_t>((value >> 24) & 0xFF)};
        }
        else
        {
            uint32_t swapped = __builtin_bswap32(value);
            return {
                static_cast<uint8_t>(swapped & 0xFF),
                static_cast<uint8_t>((swapped >> 8) & 0xFF),
                static_cast<uint8_t>((swapped >> 16) & 0xFF),
                static_cast<uint8_t>((swapped >> 24) & 0xFF)};
        }
    }

    // Converts a uint64_t to an array of bytes in little-endian order
    static constexpr array<uint8_t, 8> ULONGToBytes(uint64_t value)
    {
        if constexpr (endian::native == endian::little)
        {
            return {
                static_cast<uint8_t>(value & 0xFF),
                static_cast<uint8_t>((value >> 8) & 0xFF),
                static_cast<uint8_t>((value >> 16) & 0xFF),
                static_cast<uint8_t>((value >> 24) & 0xFF),
                static_cast<uint8_t>((value >> 32) & 0xFF),
                static_cast<uint8_t>((value >> 40) & 0xFF),
                static_cast<uint8_t>((value >> 48) & 0xFF),
                static_cast<uint8_t>((value >> 56) & 0xFF)};
        }
        else
        {
            uint64_t swapped = __builtin_bswap64(value);
            return {
                static_cast<uint8_t>(swapped & 0xFF),
                static_cast<uint8_t>((swapped >> 8) & 0xFF),
                static_cast<uint8_t>((swapped >> 16) & 0xFF),
                static_cast<uint8_t>((swapped >> 24) & 0xFF),
                static_cast<uint8_t>((swapped >> 32) & 0xFF),
                static_cast<uint8_t>((swapped >> 40) & 0xFF),
                static_cast<uint8_t>((swapped >> 48) & 0xFF),
                static_cast<uint8_t>((swapped >> 56) & 0xFF)};
        }
    }

    // Combines multiple byte arrays into one.  My masterpiece for the day :-)

    template <typename... Arrays>
    static vector<uint8_t> CombineByteArrays(Arrays &&...arrays)
    {
        vector<uint8_t> combined;

        // Calculate the total size of the combined array using a fold expression
        size_t totalSize = (arrays.size() + ...);
        combined.reserve(totalSize);

        // Append each array to the combined vector using a comma fold expression
        (combined.insert(
             combined.end(),
             make_move_iterator(arrays.begin()),
             make_move_iterator(arrays.end())),
         ...);

        return combined;
    }

    static vector<uint8_t> Compress(const vector<uint8_t> &data)
    {
        // Allocate initial buffer size
        constexpr size_t bufferIncrement = 1024;
        vector<uint8_t> compressedData(bufferIncrement);

        // Initialize zlib stream
        z_stream stream{};
        stream.zalloc = Z_NULL;
        stream.zfree = Z_NULL;
        stream.opaque = Z_NULL;

        // Set input data
        stream.next_in = const_cast<Bytef *>(data.data());
        stream.avail_in = static_cast<uInt>(data.size());

        // Initialize deflate process with optimal compression level
        if (deflateInit(&stream, Z_BEST_SPEED) != Z_OK)
        {
            throw runtime_error("Failed to initialize zlib compression");
        }

        // Compress the data
        int result;
        do
        { // Ensure the output buffer is large enough
            if (stream.total_out >= compressedData.size())
                compressedData.resize(compressedData.size() + bufferIncrement);

            // Set the output buffer
            stream.next_out = compressedData.data() + stream.total_out;
            stream.avail_out = static_cast<uInt>(compressedData.size() - stream.total_out);

            // Perform the compression
            result = deflate(&stream, Z_FINISH);
            if (result == Z_STREAM_ERROR)
            {
                deflateEnd(&stream);
                throw runtime_error("Error during zlib compression");
            }
        } while (result != Z_STREAM_END);

        // Finalize and clean up
        deflateEnd(&stream);

        // Resize the vector to the actual size of the compressed data
        compressedData.resize(stream.total_out);

        return compressedData;
    }
};

