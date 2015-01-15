#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

std::stringstream string_stream;

void save_frame ( AVFrame* frame, const int& width, const int& height,
                  const int& frame_index ) {
    std::cout << "Saving frame " << frame_index << " to disk ";

    std::string file_name;

    string_stream.clear();
    string_stream << "frame" << frame_index << ".ppm";
    string_stream >> file_name;

    // Open file
    std::ofstream file(file_name, std::ios::out | std::ios::binary);

    // Write header
    file << "P6" << std::endl << width << " "
         << height << std::endl << 255 << std::endl;

    // Write pixel data
    int num_data = width*3;
    for (int i = 0; i < height; ++i)
        file.write(reinterpret_cast<char*>(frame->data[0]+i
                                           *frame->linesize[0]),
                   num_data);

    std::cout << "............................... [Done]" << std::endl;
}

int main ( int argc, char* argv[] ) {
    if(argc < 2) {
        std::cout << "Please provide a movie file" << std::endl;
        return EXIT_FAILURE;
    }

    av_register_all();

    AVFormatContext* format_context = nullptr;

    // Open video file
    if ( 0 != avformat_open_input ( &format_context, argv[1],
                                    nullptr, nullptr ) )
        return EXIT_FAILURE; // Couldn't open file

    // Retrieve stream information
    if ( avformat_find_stream_info(format_context, nullptr) < 0 )
        return EXIT_FAILURE;

    // Dump information about file onto standard error
    av_dump_format(format_context, 0, argv[1], 0);
    
    std::cout << std::endl;

    // Find the first video stream
    int video_stream_index = av_find_best_stream(format_context,
                             AVMEDIA_TYPE_VIDEO, 0, 0, nullptr, 0);
    if (video_stream_index < 0)
        return EXIT_FAILURE; // Didn't find a video stream

    // Get a pointer to codec context for the video stream
    AVCodecContext* codec_context =
        format_context->streams[video_stream_index]->codec;

    // Find the decoder for the video stream
    AVCodec* codec = avcodec_find_decoder(codec_context->codec_id);
    if (nullptr == codec) {
        std::cerr << "Unsupported codec!\n";
        return EXIT_FAILURE; // Codec not found
    }
    // Open codec
    if (avcodec_open2(codec_context, codec, nullptr) < 0)
        return EXIT_FAILURE; // Could not open codec

    // Allocate video frame
    AVFrame* frame = av_frame_alloc();

    // Allocate an AVFrame structure
    AVFrame* rgb_frame = av_frame_alloc();
    if (nullptr == rgb_frame)
        return EXIT_FAILURE;

    int num_bytes = avpicture_get_size(AV_PIX_FMT_RGB24,
                                       codec_context->width,
                                       codec_context->height);
    std::uint8_t* buffer =
        (std::uint8_t*)av_malloc(num_bytes*sizeof(std::uint8_t));

    /* Assign appropriate parts of buffer to image planes in pFrameRGB
    Note that pFrameRGB is an AVFrame, but AVFrame is a superset
    of AVPicture */
    avpicture_fill((AVPicture*)(rgb_frame), buffer,AV_PIX_FMT_RGB24,
                   codec_context->width, codec_context->height);

    SwsContext* sws_context = sws_getContext(codec_context->width,
                              codec_context->height,
                              codec_context->pix_fmt,
                              codec_context->width,
                              codec_context->height,
                              AV_PIX_FMT_RGB24,
                              SWS_BILINEAR,
                              nullptr,
                              nullptr,
                              nullptr);

    int got_picture_pointer;
    AVPacket packet;

    int i = 0;
    while (0 == av_read_frame(format_context, &packet)) {
        // Is this a packet from the video stream?
        if (packet.stream_index == video_stream_index) {
            // Decode video frame
            avcodec_decode_video2(codec_context, frame,
                                  &got_picture_pointer, &packet);

            // Did we get a video frame?
            if (got_picture_pointer != 0) {
                if (++i > 5)
                    break;

                // Convert the image from its native format to RGB
                sws_scale(sws_context, frame->data, frame->linesize,
                          0, codec_context->height, rgb_frame->data,
                          rgb_frame->linesize);

                // Save the frame to disk
                save_frame(rgb_frame, codec_context->width,
                           codec_context->height, i);
            }
        }

        // Free the packet that was allocated by av_read_frame
        av_free_packet(&packet);
    }

    // Free the RGB image
    av_free(buffer);
    av_free(rgb_frame);

    // Free the YUV frame
    av_free ( frame );

    // Close the codec
    avcodec_close(codec_context);

    avformat_close_input ( &format_context );
    std::cout << std::endl << "All Done." << std::endl;
}


