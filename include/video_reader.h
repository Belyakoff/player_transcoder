#ifndef video_reader_hpp
#define video_reader_hpp

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <inttypes.h>
}

typedef struct StreamingParams {
    char copy_video;
    char copy_audio;
    char* output_extension;
    char* muxer_opt_key;
    char* muxer_opt_value;
    char* video_codec;
    char* audio_codec;
    char* codec_priv_key;
    char* codec_priv_value;
} StreamingParams;


typedef struct StreamingContext {
    int width, height;
    AVRational time_base;

    AVFormatContext* avfc;

    const AVCodec* video_avc;
    const AVCodec* audio_avc;
    AVStream* video_avs;
    AVStream* audio_avs;

    AVCodecContext* video_avcc;
    AVCodecContext* audio_avcc;

    int video_index;
    int audio_index;
    char* filename;

    AVFrame* av_frame;
    AVPacket* av_packet;

    SwsContext* sws_scaler_ctx;
} StreamingContext;


bool video_reader_open(StreamingContext* sc, StreamingContext* encoder, const char* filename);
bool video_reader_read_frame(StreamingContext* decoder, StreamingContext* encoder, uint8_t* frame_buffer, int64_t* pts);
bool video_reader_seek_frame(StreamingContext* sc, int64_t ts);
void video_reader_close(StreamingContext* decoder, StreamingContext* encoder);

#endif