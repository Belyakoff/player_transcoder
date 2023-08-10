#include "video_reader.h"

extern "C" {
#include <libavdevice/avdevice.h>
}

#include <iostream>
#include <cassert>

void yuyv_to_yuv420P(uint8_t* in, uint8_t* out, int width, int height, uint8_t** buffer)
{
    uint8_t* y, * u, * v;
    int i, j, offset = 0, yoffset = 0;

    y = out; // yuv420 on the front of y  
    u = out + (width * height); // yuv420 of u y in the
  
    // yuv420 placed after the u;
    v = out + (width * height * 5 / 4);

    // total size = width * height * 3/2  

        for (j = 0; j < height; j++) {
            yoffset = 2 * width * j;

            for (i = 0; i < width * 2; i = i + 4) {
                offset = yoffset + i;
                *(y++) = *(in + offset);
                *(y++) = *(in + offset + 2);
                // std::cout<<" | "<<i<<" | "<< *(y);

                if (j % 2 == 1) // discard UV component of odd rows  
                {
                    *(u++) = *(in + offset + 1);
                    *(v++) = *(in + offset + 3);
                    
                }
            }
        };

        *(buffer) = y;
        *(++buffer) = u;
        *(++buffer) = v;
}

int fill_stream_info(AVStream* avs, const AVCodec** avc, AVCodecContext** avcc) {
   
    *avc = avcodec_find_decoder(avs->codecpar->codec_id);
   
    if (!*avc) { printf("failed to find the codec"); return -1; }

    *avcc = avcodec_alloc_context3(*avc);
    if (!*avcc) { printf("failed to alloc memory for codec context"); return -1; }

    if (avcodec_parameters_to_context(*avcc, avs->codecpar) < 0) { printf("failed to fill codec context"); return -1; }

    if (avcodec_open2(*avcc, *avc, NULL) < 0) { printf("failed to open codec"); return -1; }
    return 0;
}

int open_media_web(const char* in_filename, AVFormatContext** avfc) {

    const AVInputFormat* av_input_format = av_find_input_format("vfwcap"); // dshow; vfwcap;

    if (!av_input_format) {
        printf("Couldn't find dshow input format to get webcam\n");
        return false;
    }

    AVDictionary* options = NULL;
    av_dict_set(&options, "framerate", "30", 0);
    av_dict_set(&options, "video_size", "640x480", 0); 
    av_dict_set(&options, "pix_fmt", "yuyv422", 0);
    av_dict_set(&options, "list_options", "true", 0);

    const char* url = "USB2.0 VGA UVC WebCam";  // USB2.0 VGA UVC WebCam; USB Camera;
    if (avformat_open_input(avfc, url, av_input_format, &options) != 0) {
        printf("Couldn't open video file\n");
        return false;
    }
    
    if (avformat_find_stream_info(*avfc, NULL) < 0) { printf("failed to get stream info"); return -1; }

    return 0;
}

int open_media_file(const char* in_filename, AVFormatContext** avfc) {

    *avfc = avformat_alloc_context();

    if (!*avfc) { printf("failed to alloc memory for format"); return -1; }

    if (avformat_open_input(avfc, in_filename, NULL, NULL) != 0) { printf("failed to open input file %s", in_filename); return -1; }

    if (avformat_find_stream_info(*avfc, NULL) < 0) { printf("failed to get stream info"); return -1; }

    return 0;
}

int prepare_decoder(StreamingContext* decoder) {

    int video_index = -1;
    AVCodecParameters* av_codec_params;

    for (uint16_t i = 0; i < decoder->avfc->nb_streams; ++i) {
        
        av_codec_params = decoder->avfc->streams[i]->codecpar; 

        if (av_codec_params->codec_type == AVMEDIA_TYPE_VIDEO) {
            
            decoder->video_avs = decoder->avfc->streams[i];
            video_index = i;

            decoder->width = decoder->avfc->streams[i]->codecpar->width;
            decoder->height = decoder->avfc->streams[i]->codecpar->height;
           
            if (fill_stream_info(decoder->video_avs, &decoder->video_avc, &decoder->video_avcc)) { return -1; }
        }
    }

    return 0;
}

int prepare_video_encoder(StreamingContext* encoder, AVCodecContext* decoder_ctx, AVRational input_framerate, StreamingParams sp) {
 
    encoder->width = 1280;
    encoder->height = 720;

    encoder->video_avs = avformat_new_stream(encoder->avfc, NULL);

    // encoder->video_avc = avcodec_find_encoder_by_name(sp.video_codec);
    encoder->video_avc = avcodec_find_encoder(AV_CODEC_ID_H265);
    if (!encoder->video_avc) { printf("could not find the proper codec"); return -1; }

    encoder->video_avcc = avcodec_alloc_context3(encoder->video_avc);
    if (!encoder->video_avcc) { printf("could not allocated memory for codec context"); return -1; }

    av_opt_set(encoder->video_avcc->priv_data, "preset", "fast", 0);
    if (sp.codec_priv_key && sp.codec_priv_value)
        av_opt_set(encoder->video_avcc->priv_data, sp.codec_priv_key, sp.codec_priv_value, 0);

    encoder->video_avcc->height = decoder_ctx->height;
    encoder->video_avcc->width = decoder_ctx->width;

    encoder->video_avcc->sample_aspect_ratio = decoder_ctx->sample_aspect_ratio;
    if (encoder->video_avc->pix_fmts)
        encoder->video_avcc->pix_fmt = encoder->video_avc->pix_fmts[0];
    else
        encoder->video_avcc->pix_fmt = decoder_ctx->pix_fmt;
    
    encoder->video_avcc->bit_rate = 2 * 1000 * 1000;
    encoder->video_avcc->rc_buffer_size = 4 * 1000 * 1000;
    encoder->video_avcc->rc_max_rate = 2 * 1000 * 1000;
    encoder->video_avcc->rc_min_rate = 2.5 * 1000 * 1000;

    encoder->video_avcc->time_base = av_inv_q(input_framerate);
    encoder->video_avs->time_base = av_inv_q(input_framerate);

    encoder->video_avcc->sample_aspect_ratio.num = 1;
    encoder->video_avcc->sample_aspect_ratio.den = 1;

 
    if (avcodec_open2(encoder->video_avcc, encoder->video_avc, NULL) < 0) { printf("could not open the codec"); return -1; }
    avcodec_parameters_from_context(encoder->video_avs->codecpar, encoder->video_avcc);
    return 0;
}

int prepare_audio_encoder(StreamingContext* sc, int sample_rate, StreamingParams sp) {

    sc->audio_avs = avformat_new_stream(sc->avfc, NULL);

    sc->audio_avc = avcodec_find_encoder_by_name(sp.audio_codec);
    if (!sc->audio_avc) { printf("could not find the proper codec"); return -1; }

    sc->audio_avcc = avcodec_alloc_context3(sc->audio_avc);
    if (!sc->audio_avcc) { printf("could not allocated memory for codec context"); return -1; }

    int OUTPUT_CHANNELS = 2;
    int OUTPUT_BIT_RATE = 196000;
    sc->audio_avcc->channels = OUTPUT_CHANNELS;
    sc->audio_avcc->channel_layout = av_get_default_channel_layout(OUTPUT_CHANNELS);
    sc->audio_avcc->sample_rate = sample_rate;
    sc->audio_avcc->sample_fmt = sc->audio_avc->sample_fmts[0];
    sc->audio_avcc->bit_rate = OUTPUT_BIT_RATE;
    sc->audio_avcc->time_base = { 1, sample_rate };

    sc->audio_avcc->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

    sc->audio_avs->time_base = sc->audio_avcc->time_base;

    if (avcodec_open2(sc->audio_avcc, sc->audio_avc, NULL) < 0) { printf("could not open the codec"); return -1; }
    avcodec_parameters_from_context(sc->audio_avs->codecpar, sc->audio_avcc);

    return 0;
}

int encode_video(StreamingContext* decoder, StreamingContext* encoder, AVFrame* input_frame) {
    // if (input_frame) input_frame->pict_type = AV_PICTURE_TYPE_NONE;

    AVPacket* output_packet = av_packet_alloc();
    if (!output_packet) { printf("could not allocate memory for output packet"); return -1; }

    int response = avcodec_send_frame(encoder->video_avcc, input_frame);

    while (response >= 0) {
        response = avcodec_receive_packet(encoder->video_avcc, output_packet);
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            break;
        }
        else if (response < 0) {
            printf("Error while receiving packet from encoder: ");
            return -1;
        }

        output_packet->stream_index = decoder->video_index;
        output_packet->duration = encoder->video_avs->time_base.den / encoder->video_avs->time_base.num / decoder->video_avs->avg_frame_rate.num * decoder->video_avs->avg_frame_rate.den;

        av_packet_rescale_ts(output_packet, decoder->video_avs->time_base, encoder->video_avs->time_base);
        response = av_interleaved_write_frame(encoder->avfc, output_packet);
        if (response != 0) { printf("Error  while receiving packet from decoder: "); return -1; }
    }

    av_packet_unref(output_packet);
    av_packet_free(&output_packet);
    return 0;
}

int encode_audio(StreamingContext* decoder, StreamingContext* encoder, AVFrame* input_frame) {
    AVPacket* output_packet = av_packet_alloc();
    if (!output_packet) { printf("could not allocate memory for output packet"); return -1; }

    int response = avcodec_send_frame(encoder->audio_avcc, input_frame);

    while (response >= 0) {
        response = avcodec_receive_packet(encoder->audio_avcc, output_packet);
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            break;
        }
        else if (response < 0) {
            printf("Error while receiving packet from encoder: ");
            return -1;
        }

        output_packet->stream_index = decoder->audio_index;

        av_packet_rescale_ts(output_packet, decoder->audio_avs->time_base, encoder->audio_avs->time_base);
        response = av_interleaved_write_frame(encoder->avfc, output_packet);
        if (response != 0) { printf("Error  while receiving packet from decoder: "); return -1; }
    }
    av_packet_unref(output_packet);
    av_packet_free(&output_packet);
    return 0;
}

int transcode_audio(StreamingContext* decoder, StreamingContext* encoder, AVPacket* input_packet, AVFrame* input_frame) {
    int response = avcodec_send_packet(decoder->audio_avcc, input_packet);
    if (response < 0) { printf("Error while sending packet to decoder: "); return response; }

    while (response >= 0) {
        response = avcodec_receive_frame(decoder->audio_avcc, input_frame);
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            break;
        }
        else if (response < 0) {
            printf("Error while receiving frame from decoder: ");
            return response;
        }

        if (response >= 0) {
            if (encode_audio(decoder, encoder, input_frame)) return -1;
        }
        av_frame_unref(input_frame);
    }
    return 0;
}

int prepare_copy(AVFormatContext* avfc, AVStream** avs, AVCodecParameters* decoder_par) {
    *avs = avformat_new_stream(avfc, NULL);
    avcodec_parameters_copy((*avs)->codecpar, decoder_par);
    return 0;
}

int remux(AVPacket** pkt, AVFormatContext** avfc, AVRational decoder_tb, AVRational encoder_tb) {
    av_packet_rescale_ts(*pkt, decoder_tb, encoder_tb);
    if (av_interleaved_write_frame(*avfc, *pkt) < 0) { printf("error while copying stream packet"); return -1; }
    return 0;
}

bool video_reader_open(StreamingContext* decoder, StreamingContext* encoder, const char* filename) {
    
    auto& width = decoder->width;
    auto& height = decoder->height;
    auto& time_base = decoder->time_base;
    auto& avfc = decoder->avfc;
    auto& video_avcc = decoder->video_avcc;
    auto& video_index = decoder->video_index;
    auto& av_frame = decoder->av_frame;
    auto& av_packet = decoder->av_packet;

    StreamingParams sp = { 0 };
    sp.copy_audio = 1;
    sp.copy_video = 0;
    sp.video_codec = "libx265";
    // sp.codec_priv_key = "x265-params"; 
    // sp.codec_priv_value = "keyint=60:min-keyint=60:scenecut=0";

    avdevice_register_all();

    if (open_media_file(decoder->filename, &decoder->avfc)) return -1;
    
    if (prepare_decoder(decoder)) return -1;  

    avformat_alloc_output_context2(&encoder->avfc, NULL, NULL, encoder->filename);
    if (!encoder->avfc) { printf("could not allocate memory for output format"); return -1; }

    if (!sp.copy_video) {
        AVRational input_framerate = av_guess_frame_rate(decoder->avfc, decoder->video_avs, NULL);
        prepare_video_encoder(encoder, decoder->video_avcc, input_framerate, sp);
    }
    else {
        if (prepare_copy(encoder->avfc, &encoder->video_avs, decoder->video_avs->codecpar)) { return -1; }
    }
    // prepare_audio_encoder(encoder, decoder->audio_avcc->sample_rate, sp);
   
    if (encoder->avfc->oformat->flags & AVFMT_GLOBALHEADER)
        encoder->avfc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (!(encoder->avfc->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&encoder->avfc->pb, encoder->filename, AVIO_FLAG_WRITE) < 0) {
            printf("could not open the output file");
            return -1;
        }
    }

    /* 
    AVDictionary* muxer_opts = NULL;
    if (sp.muxer_opt_key && sp.muxer_opt_value) {
        av_dict_set(&muxer_opts, sp.muxer_opt_key, sp.muxer_opt_value, 0);
    } */

    if (avformat_write_header(encoder->avfc, NULL) < 0) { printf("an error occurred when opening output file"); return -1; }

    av_frame = av_frame_alloc();
    if (!av_frame) {
        printf("Couldn't allocate AVFrame\n");
        return false;
    }
    av_packet = av_packet_alloc();
    if (!av_packet) {
        printf("Couldn't allocate AVPacket\n");
        return false;
    }

    return true;
}

bool video_reader_read_frame(StreamingContext* decoder, StreamingContext* encoder, uint8_t* frame_buffer, int64_t* pts) {

    int width = decoder->width;
    int height = decoder->height;
   
    int response;
    AVCodecParameters* av_codec_params;

    while (av_read_frame(decoder->avfc, decoder->av_packet) >= 0) {
        
        av_codec_params = decoder->avfc->streams[decoder->av_packet->stream_index]->codecpar;

        if (av_codec_params->codec_type == AVMEDIA_TYPE_VIDEO) {
            response = avcodec_send_packet(decoder->video_avcc, decoder->av_packet);
            if (response < 0) {
                printf("Failed to decode packet: %d \n", response);
                return false; 
            }
            response = avcodec_receive_frame(decoder->video_avcc, decoder->av_frame);
            if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
                av_packet_unref(decoder->av_packet);
                continue;
            }
            else if (response < 0) {
                printf("Failed to decode packet: %d \n", response);
                return false;
            }
            if (response >= 0) {

                if (decoder->av_frame) decoder->av_frame->pict_type = AV_PICTURE_TYPE_NONE;
                AVPacket* output_packet = av_packet_alloc();
                if (!output_packet) { printf("could not allocate memory for output packet"); return -1; }

                decoder->sws_scaler_ctx = sws_getContext(width, height, decoder->video_avcc->pix_fmt, width, height, AV_PIX_FMT_RGB0, SWS_BILINEAR, NULL, NULL, NULL);
                if (!decoder->sws_scaler_ctx) {
                    printf("Couldn't initialize sw scaler\n");
                    return false;
                }
                uint8_t* dest[4] = { frame_buffer, NULL, NULL, NULL };
                int dest_linesize[4] = { width * 4, 0, 0, 0 };
                sws_scale(decoder->sws_scaler_ctx, decoder->av_frame->data, decoder->av_frame->linesize, 0, decoder->av_frame->height, dest, dest_linesize);
                
                encode_video(decoder, encoder, decoder->av_frame);

                av_packet_unref(decoder->av_packet);
                av_packet_unref(output_packet);
                av_packet_free(&output_packet);

            }  else {
                printf("----------------------------");
                printf("not a video or an audio packet \n" );
                av_packet_unref(decoder->av_packet);
                continue;

            }
            /*
              else if (decoder->avfc->streams[decoder->av_packet->stream_index]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {

               if (transcode_audio(decoder, encoder, decoder->av_packet, decoder->av_frame)) return -1;
               av_packet_unref(decoder->av_packet);

               }
            */

        }
        break;
    }

    av_frame_unref(decoder->av_frame);
   
    return true;
}

bool video_reader_seek_frame(StreamingContext* state, int64_t ts) {

    auto& avfc = state->avfc;
    auto& av_codec_ctx = state->video_avcc;
    auto& video_index = state->video_index;
    auto& av_packet = state->av_packet;
    auto& av_frame = state->av_frame;

    // int err = avformat_seek_file(av_format_ctx, video_stream_index, 0, ts, ts, AVSEEK_FLAG_ANY);
    av_seek_frame(avfc, video_index, 0, AVSEEK_FLAG_BACKWARD);

    int response;
    while (av_read_frame(avfc, av_packet) >= 0) {
        if (av_packet->stream_index != video_index) {
            av_packet_unref(av_packet);
            continue;
        }

        response = avcodec_send_packet(av_codec_ctx, av_packet);
        if (response < 0) {
            printf("Failed to decode packet\n");
            return false;
        }

        response = avcodec_receive_frame(av_codec_ctx, av_frame);
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            av_packet_unref(av_packet);
            continue;
        }
        else if (response < 0) {
            printf("Failed to decode packet: \n");
            return false;
        }

        av_packet_unref(av_packet);
        break;
    }

    return true;
}

void video_reader_close(StreamingContext* decoder, StreamingContext* encoder) {
    av_write_trailer(encoder->avfc); 

    sws_freeContext(decoder->sws_scaler_ctx);

    avformat_close_input(&decoder->avfc);

    avformat_free_context(decoder->avfc); decoder->avfc = NULL;
    avformat_free_context(encoder->avfc); encoder->avfc = NULL;

    avcodec_free_context(&decoder->video_avcc); decoder->video_avcc = NULL;
    avcodec_free_context(&decoder->audio_avcc); decoder->audio_avcc = NULL;

    av_frame_free(&decoder->av_frame);
    av_packet_free(&decoder->av_packet);

    free(decoder); decoder = NULL;
    free(encoder); encoder = NULL; 
}

