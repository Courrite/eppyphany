#include "DSP/AudioLoader.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libswresample/swresample.h>
    #include <libavutil/opt.h>
    #include <libavutil/samplefmt.h>
}

namespace eppyphany::DSP {
    Audio AudioLoader::LoadAudio(const std::filesystem::path &audioPath) {
        std::cout << "Loading audio file: " << audioPath.filename() << "\n";

        std::vector<double> monoAudioData;

        // open the input media file
        AVFormatContext* formatCtx = nullptr;
        if (avformat_open_input(&formatCtx, audioPath.string().c_str(), nullptr, nullptr) < 0) {
            throw std::runtime_error("Could not open file. " + audioPath.string());
        }
        auto formatDeleter = [](AVFormatContext* ctx) { avformat_close_input(&ctx); };
        std::unique_ptr<AVFormatContext, decltype(formatDeleter)> formatGuard(formatCtx, formatDeleter);

        if (avformat_find_stream_info(formatCtx, nullptr) < 0) {
            throw std::runtime_error("Could not find stream info.");
        }

        // locate the primary audio stream
        const AVCodec* codec = nullptr;
        int streamIndex = av_find_best_stream(formatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
        if (streamIndex < 0) {
            throw std::runtime_error("Could not find an audio stream.");
        }
        AVStream* stream = formatCtx->streams[streamIndex];

        // allocate and open the decoder context
        AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
        if (!codecCtx) {
            throw std::runtime_error("Failed to create codec.");
        }
        auto codecDeleter = [](AVCodecContext* ctx) { avcodec_free_context(&ctx); };
        std::unique_ptr<AVCodecContext, decltype(codecDeleter)> codecGuard(codecCtx, codecDeleter);

        if (avcodec_parameters_to_context(codecCtx, stream->codecpar) < 0 || avcodec_open2(codecCtx, codec, nullptr) < 0) {
            throw std::runtime_error("Failed to open codec.");
        }

        // lock down internal sample rate based on source asset
        auto sampleRate = codecCtx->sample_rate > 0 ? codecCtx->sample_rate : 44100.0;

        // set up the SwrContext resampler to downmix to Mono double Packed
        SwrContext* swrCtx = swr_alloc();
        if (!swrCtx) {
            throw std::runtime_error("Failed to create resampler.");
        }
        auto swrDeleter = [](SwrContext* s) { swr_free(&s); };
        std::unique_ptr<SwrContext, decltype(swrDeleter)> swrGuard(swrCtx, swrDeleter);

        // setup channel layouts
        AVChannelLayout monoLayout;
        av_channel_layout_default(&monoLayout, 1);

        av_opt_set_chlayout(swrCtx, "in_chlayout", &codecCtx->ch_layout, 0);
        av_opt_set_int(swrCtx, "in_sample_rate", codecCtx->sample_rate, 0);
        av_opt_set_sample_fmt(swrCtx, "in_sample_fmt", codecCtx->sample_fmt, 0);

        av_opt_set_chlayout(swrCtx, "out_chlayout", &monoLayout, 0);
        av_opt_set_int(swrCtx, "out_sample_rate", sampleRate, 0);
        av_opt_set_sample_fmt(swrCtx, "out_sample_fmt", AV_SAMPLE_FMT_DBL, 0); // Packed double

        if (swr_init(swrCtx) < 0) {
            throw std::runtime_error("Failed to initialize resampler");
        }

        // allocation of runtime working packets & frames
        AVPacket* packet = av_packet_alloc();
        AVFrame* frame = av_frame_alloc();
        auto pktDeleter = [](AVPacket* p) { av_packet_free(&p); };
        auto frmDeleter = [](AVFrame* f) { av_frame_free(&f); };
        std::unique_ptr<AVPacket, decltype(pktDeleter)> pktGuard(packet, pktDeleter);
        std::unique_ptr<AVFrame, decltype(frmDeleter)> frmGuard(frame, frmDeleter);

        // max allocations guess for resample block
        int maxDstNbSamples = 4096;
        double* resampleBuffer = nullptr;
        av_samples_alloc(reinterpret_cast<uint8_t**>(&resampleBuffer), nullptr, 1, maxDstNbSamples, AV_SAMPLE_FMT_DBL, 0);
        auto bufDeleter = [](double* b) { av_freep(&b); };
        std::unique_ptr<double, decltype(bufDeleter)> bufGuard(resampleBuffer, bufDeleter);

        // packet parsing execution loop
        while (av_read_frame(formatCtx, packet) >= 0) {
            if (packet->stream_index == streamIndex) {
                if (avcodec_send_packet(codecCtx, packet) >= 0) {
                    while (avcodec_receive_frame(codecCtx, frame) >= 0) {
                        
                        // compute conversion target bounds sizing
                        int dstNbSamples = av_rescale_rnd(swr_get_delay(swrCtx, codecCtx->sample_rate) + frame->nb_samples, 
                                                          sampleRate, codecCtx->sample_rate, AV_ROUND_UP);
                        
                        if (dstNbSamples > maxDstNbSamples) {
                            av_freep(&resampleBuffer);
                            maxDstNbSamples = dstNbSamples;
                            av_samples_alloc(reinterpret_cast<uint8_t**>(&resampleBuffer), nullptr, 1, maxDstNbSamples, AV_SAMPLE_FMT_DBL, 0);
                            bufGuard.reset(resampleBuffer);
                        }

                        // convert to Mono double array format
                        int convertedSamples = swr_convert(swrCtx, reinterpret_cast<uint8_t**>(&resampleBuffer), dstNbSamples, 
                                                           const_cast<const uint8_t**>(frame->data), frame->nb_samples);
                        
                        if (convertedSamples > 0) {
                            int oldSize = monoAudioData.size();
                            monoAudioData.resize(oldSize + convertedSamples);
                            std::copy(resampleBuffer, resampleBuffer + convertedSamples, monoAudioData.begin() + oldSize);
                        }
                        
                        av_frame_unref(frame);
                    }
                }
            }

            av_packet_unref(packet);
        }

        std::cout << "Successfully decoded " << monoAudioData.size() << " samples into mono processing pipeline.\n";
        
        return {
            .Samples = monoAudioData,
            .SampleRate = sampleRate
        };
    };
}