#include <cstdlib>
#include <iostream>

#include <SDL2/SDL.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "usage: tutorial_02 input_file" << std::endl;

        av_log(nullptr, AV_LOG_FATAL,
               "\nAn input file must be specified\n");
        return -1;
    }

    // Register all formats and codecs
    av_register_all();

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        av_log(nullptr, AV_LOG_FATAL,
               "Could not initialize SDL - %s\n", SDL_GetError());
        std::exit(EXIT_FAILURE);
    }

    AVFormatContext* format_context = nullptr;

    // Open video file
    if ( 0 != avformat_open_input ( &format_context, argv[1],
                                    nullptr, nullptr ) )
        return -1; // Couldn't open file

    // Retrieve stream information
    if ( avformat_find_stream_info(format_context, nullptr) < 0 )
        return -1;

    // Dump information about file onto standard error
    av_dump_format(format_context, 0, argv[1], 0);

    // Find the first video stream
    int video_stream_index = av_find_best_stream(format_context,
                             AVMEDIA_TYPE_VIDEO, 0, 0, nullptr, 0);
    if (video_stream_index < 0)
        return -1; // Didn't find a video stream

    // Get a pointer to codec context for the video stream
    AVCodecContext* codec_context =
        format_context->streams[video_stream_index]->codec;

    // Find the decoder for the video stream
    AVCodec* codec = avcodec_find_decoder(codec_context->codec_id);
    if (nullptr == codec) {
        av_log(nullptr, AV_LOG_FATAL, "Unsupported codec!\n");
        return -1; // Codec not found
    }

    // Open codec
    if (avcodec_open2(codec_context, codec, nullptr) < 0)
        return -1; // Could not open codec

    SDL_Window* window = SDL_CreateWindow("player",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          codec_context->width,
                                          codec_context->height,
                                          SDL_WINDOW_OPENGL);
    if (nullptr == window) {
        av_log(nullptr, AV_LOG_FATAL,
               "Could not create window - %s\n", SDL_GetError());
        std::exit(EXIT_FAILURE);
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
                             SDL_RENDERER_ACCELERATED);
    if (nullptr == renderer) {
        av_log(nullptr, AV_LOG_FATAL,
               "Could not create renderer - %s\n", SDL_GetError());
        std::exit(EXIT_FAILURE);
    }

    SDL_Texture* texture = SDL_CreateTexture(renderer,
                           SDL_PIXELFORMAT_YV12,
                           SDL_TEXTUREACCESS_STREAMING,
                           codec_context->width,
                           codec_context->height);
    if (nullptr == texture) {
        av_log(nullptr, AV_LOG_FATAL,
               "Could not create texture - %s\n", SDL_GetError());
        std::exit(EXIT_FAILURE);
    }

    SDL_Rect rect = {0, 0, codec_context->width, codec_context->height};

    // Allocate video frame
    AVFrame* frame = av_frame_alloc();
    
    int got_picture_ptr;
    AVPacket packet;
    SDL_Event event;
    
    while (0 == av_read_frame(format_context, &packet)) {
        // Is this a packet from the video stream?
        if (packet.stream_index == video_stream_index) {
            // Decode video frame
            avcodec_decode_video2(codec_context, frame,
                                  &got_picture_ptr, &packet);

            // Did we get a video frame?
            if (got_picture_ptr != 0) {
                SDL_UpdateYUVTexture(texture,
                                     &rect,
                                     frame->data[0],
                                     frame->linesize[0],
                                     frame->data[1],
                                     frame->linesize[1],
                                     frame->data[2],
                                     frame->linesize[2]);
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, &rect, &rect);
                SDL_RenderPresent(renderer);
            }
        }

        // Free the packet that was allocated by av_read_frame
        av_free_packet(&packet);

        SDL_PollEvent(&event);
        if (SDL_QUIT == event.type)
            break;
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyTexture(texture);
    SDL_DestroyWindow(window);

    SDL_Quit();

    // Free the YUV frame
    av_free(frame);

    // Close the codec
    avcodec_close(codec_context);

    avformat_close_input (&format_context);
}

