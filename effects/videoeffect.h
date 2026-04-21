#pragma once
using namespace std;
using namespace std::chrono;

#include "../interfaces.h"
#include "../ledeffectbase.h"
#include "../pixeltypes.h"
#include <string>
#include <iostream>
#include <vector>

extern "C" 
{
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libswscale/swscale.h>
}

class MP4PlaybackEffect : public LEDEffectBase
{
private:
    string                  _filePath;
    AVFormatContext*        _formatCtx = nullptr;
    AVCodecContext*         _codecCtx = nullptr;
    AVFrame*                _frame = nullptr;
    AVPacket*               _packet = nullptr;
    SwsContext*             _swsCtx = nullptr;
    int                     _videoStreamIndex = -1;
    atomic<bool>            _initialized = false;
    mutable recursive_mutex _ffmpegMutex; // recursive_mutex to allow multiple locks by the same thread

    bool AfterInitError(const string& message)
    {
        logger->error(message);
        CleanupFFmpeg();
        return false;
    }

    bool InitializeFFmpeg()
    {
        lock_guard lock(_ffmpegMutex);

        if (_initialized)
            return true;

        // Open the input file
        if (avformat_open_input(&_formatCtx, _filePath.c_str(), nullptr, nullptr) != 0)
            return AfterInitError("Failed to open video file: " + _filePath);
 
        // Retrieve stream information
        if (avformat_find_stream_info(_formatCtx, nullptr) < 0)
            return AfterInitError("Failed to retrieve stream info.");

        // Find the video stream
        for (unsigned i = 0; i < _formatCtx->nb_streams; i++)
        {
            if (_formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                _videoStreamIndex = i;
                break;
            }
        }

        if (_videoStreamIndex == -1)
            return AfterInitError("No video stream found.");

        // Find the decoder for the video stream
        const AVCodec* codec = avcodec_find_decoder(_formatCtx->streams[_videoStreamIndex]->codecpar->codec_id);
        if (!codec)
            return AfterInitError("Codec not found.");

        // Allocate the codec context
        _codecCtx = avcodec_alloc_context3(codec);
        if (!_codecCtx)
            return AfterInitError("Failed to allocate codec context.");

        if (avcodec_parameters_to_context(_codecCtx, _formatCtx->streams[_videoStreamIndex]->codecpar) < 0)
            return AfterInitError("Failed to copy codec parameters to context.");

        // Open the codec
        if (avcodec_open2(_codecCtx, codec, nullptr) < 0)
            return AfterInitError("Failed to open codec.");

        // Allocate frames and packets
        _frame = av_frame_alloc();
        _packet = av_packet_alloc();

        _initialized = true;
        return true;
    }

    void CleanupFFmpeg()
    {
        lock_guard lock(_ffmpegMutex);

        if (_swsCtx) 
            sws_freeContext(_swsCtx);

        if (_frame) 
            av_frame_free(&_frame);

        if (_packet) 
            av_packet_free(&_packet);

        if (_codecCtx) 
            avcodec_free_context(&_codecCtx);

        if (_formatCtx) 
            avformat_close_input(&_formatCtx);

        _initialized = false;
    }

public:

    MP4PlaybackEffect(const string& name, const string& filePath)
        : LEDEffectBase(name), _filePath(filePath) {}

    ~MP4PlaybackEffect()
    {
        CleanupFFmpeg();
    }

    void Start(ICanvas& canvas) override
    {
        if (!InitializeFFmpeg())
        {
            logger->error("Failed to initialize FFmpeg for MP4 playback.");
            return;
        }

        auto& graphics = canvas.Graphics();
        int canvasWidth = graphics.Width();
        int canvasHeight = graphics.Height();

        if (_swsCtx)
            sws_freeContext(_swsCtx);

        _swsCtx = sws_getContext(
            _codecCtx->width, _codecCtx->height, _codecCtx->pix_fmt,
            canvasWidth, canvasHeight, AV_PIX_FMT_RGB24,
            SWS_BILINEAR, nullptr, nullptr, nullptr);
    }

    void Update(ICanvas& canvas, microseconds deltaTime) override 
    {
        if (!_initialized) 
            return;

        while (av_read_frame(_formatCtx, _packet) >= 0)
        {
            if (_packet->stream_index == _videoStreamIndex)
            {
                if (avcodec_send_packet(_codecCtx, _packet) == 0)
                {
                    while (avcodec_receive_frame(_codecCtx, _frame) == 0)
                    {
                        auto& graphics = canvas.Graphics();
                        int canvasWidth = graphics.Width();
                        int canvasHeight = graphics.Height();

                        // Buffer for RGB24 pixels
                        vector<uint8_t> rgbBuffer(canvasWidth * canvasHeight * 3);

                        uint8_t* dstData[1] = { rgbBuffer.data() };
                        int dstLinesize[1] = { 3 * canvasWidth };

                        sws_scale(_swsCtx, _frame->data, _frame->linesize, 0, _codecCtx->height, dstData, dstLinesize);

                        // Render to canvas
                        for (int y = 0; y < canvasHeight; ++y)
                        {
                            for (int x = 0; x < canvasWidth; ++x)
                            {
                                int idx = (y * canvasWidth + x) * 3;
                                graphics.SetPixel(x, y, CRGB(rgbBuffer[idx], rgbBuffer[idx + 1], rgbBuffer[idx + 2]));
                            }
                        }

                        av_packet_unref(_packet);
                        return; // Process one frame per update
                    }
                }
            }
            av_packet_unref(_packet);
        }

        // Loop the video by restarting
        av_seek_frame(_formatCtx, _videoStreamIndex, 0, AVSEEK_FLAG_BACKWARD);
    }

    friend inline void to_json(nlohmann::json& j, const MP4PlaybackEffect & effect);
    friend inline void from_json(const nlohmann::json& j, shared_ptr<MP4PlaybackEffect>& effect);
};

inline void to_json(nlohmann::json& j, const MP4PlaybackEffect & effect) 
{
    j = {
        {"name", effect.Name()},
        {"filePath", effect._filePath}
    };
}

inline void from_json(const nlohmann::json& j, shared_ptr<MP4PlaybackEffect>& effect)
{
    effect = make_shared<MP4PlaybackEffect>(
        j.at("name").get<string>(),
        j.at("filePath").get<string>()
    );
}