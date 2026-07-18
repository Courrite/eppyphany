#include "Generation/DSProcessor.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <memory>

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libswresample/swresample.h>
    #include <libavutil/opt.h>
    #include <kissfft/kiss_fftr.h>
}

namespace eppyphany::Generation {
    DSProcessor::DSProcessor(size_t fftSize, size_t hopSize)
        : fftSize_(fftSize), hopSize_(hopSize) {
        // kiss_fftr requires an even fft size for its complex packing trick.
        if (fftSize_ % 2 != 0) {
            std::cerr << "DSProcessor Warning: fftSize must be even for a real-input FFT; "
                          "rounding " << fftSize_ << " down to " << (fftSize_ - 1) << ".\n";
            fftSize_ -= 1;
        }
        fftPlan_ = kiss_fftr_alloc(static_cast<int>(fftSize_), /*inverse_fft=*/0, nullptr, nullptr);
        if (!fftPlan_) {
            std::cerr << "DSProcessor Error: kiss_fftr_alloc failed for fftSize=" << fftSize_ << "\n";
        }
    }

    DSProcessor::~DSProcessor() {
        if (fftPlan_) {
            kiss_fftr_free(fftPlan_);
            fftPlan_ = nullptr;
        }
    }

    bool DSProcessor::LoadAudio(const std::filesystem::path& audioPath) {
        std::cout << "Loading audio file for DSP via FFmpeg: " << audioPath.filename() << "\n";
        
        monoAudioData_.clear();

        // open the input media file
        AVFormatContext* formatCtx = nullptr;
        if (avformat_open_input(&formatCtx, audioPath.string().c_str(), nullptr, nullptr) < 0) {
            std::cerr << "FFmpeg Error: Could not open file " << audioPath << "\n";
            return false;
        }
        auto formatDeleter = [](AVFormatContext* ctx) { avformat_close_input(&ctx); };
        std::unique_ptr<AVFormatContext, decltype(formatDeleter)> formatGuard(formatCtx, formatDeleter);

        if (avformat_find_stream_info(formatCtx, nullptr) < 0) {
            std::cerr << "FFmpeg Error: Could not find stream info\n";
            return false;
        }

        // locate the primary audio stream
        const AVCodec* codec = nullptr;
        int streamIndex = av_find_best_stream(formatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
        if (streamIndex < 0) {
            std::cerr << "FFmpeg Error: Could not find an audio stream\n";
            return false;
        }
        AVStream* stream = formatCtx->streams[streamIndex];

        // allocate and open the decoder context
        AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
        if (!codecCtx) return false;
        auto codecDeleter = [](AVCodecContext* ctx) { avcodec_free_context(&ctx); };
        std::unique_ptr<AVCodecContext, decltype(codecDeleter)> codecGuard(codecCtx, codecDeleter);

        if (avcodec_parameters_to_context(codecCtx, stream->codecpar) < 0 || avcodec_open2(codecCtx, codec, nullptr) < 0) {
            std::cerr << "FFmpeg Error: Failed to open codec\n";
            return false;
        }

        // lock down internal rate target based on source asset
        sampleRate_ = codecCtx->sample_rate > 0 ? codecCtx->sample_rate : 44100;

        // set up the SwrContext resampler to downmix to Mono Float Packed
        SwrContext* swrCtx = swr_alloc();
        if (!swrCtx) return false;
        auto swrDeleter = [](SwrContext* s) { swr_free(&s); };
        std::unique_ptr<SwrContext, decltype(swrDeleter)> swrGuard(swrCtx, swrDeleter);

        // Setup channel layouts (Mono output)
        AVChannelLayout monoLayout;
        av_channel_layout_default(&monoLayout, 1);

        av_opt_set_chlayout(swrCtx, "in_chlayout", &codecCtx->ch_layout, 0);
        av_opt_set_int(swrCtx, "in_sample_rate", codecCtx->sample_rate, 0);
        av_opt_set_sample_fmt(swrCtx, "in_sample_fmt", codecCtx->sample_fmt, 0);

        av_opt_set_chlayout(swrCtx, "out_chlayout", &monoLayout, 0);
        av_opt_set_int(swrCtx, "out_sample_rate", sampleRate_, 0);
        av_opt_set_sample_fmt(swrCtx, "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0); // Packed float

        if (swr_init(swrCtx) < 0) {
            std::cerr << "FFmpeg Error: Failed to initialize audio resampler\n";
            return false;
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
        float* resampleBuffer = nullptr;
        av_samples_alloc(reinterpret_cast<uint8_t**>(&resampleBuffer), nullptr, 1, maxDstNbSamples, AV_SAMPLE_FMT_FLT, 0);
        auto bufDeleter = [](float* b) { av_freep(&b); };
        std::unique_ptr<float, decltype(bufDeleter)> bufGuard(resampleBuffer, bufDeleter);

        // packet parsing execution loop
        while (av_read_frame(formatCtx, packet) >= 0) {
            if (packet->stream_index == streamIndex) {
                if (avcodec_send_packet(codecCtx, packet) >= 0) {
                    while (avcodec_receive_frame(codecCtx, frame) >= 0) {
                        
                        // compute conversion target bounds sizing
                        int dstNbSamples = av_rescale_rnd(swr_get_delay(swrCtx, codecCtx->sample_rate) + frame->nb_samples, 
                                                          sampleRate_, codecCtx->sample_rate, AV_ROUND_UP);
                        
                        if (dstNbSamples > maxDstNbSamples) {
                            av_freep(&resampleBuffer);
                            maxDstNbSamples = dstNbSamples;
                            av_samples_alloc(reinterpret_cast<uint8_t**>(&resampleBuffer), nullptr, 1, maxDstNbSamples, AV_SAMPLE_FMT_FLT, 0);
                            bufGuard.reset(resampleBuffer);
                        }

                        // convert to Mono float array format
                        int convertedSamples = swr_convert(swrCtx, reinterpret_cast<uint8_t**>(&resampleBuffer), dstNbSamples, 
                                                           const_cast<const uint8_t**>(frame->data), frame->nb_samples);
                        
                        if (convertedSamples > 0) {
                            size_t oldSize = monoAudioData_.size();
                            monoAudioData_.resize(oldSize + convertedSamples);
                            std::copy(resampleBuffer, resampleBuffer + convertedSamples, monoAudioData_.begin() + oldSize);
                        }
                        
                        av_frame_unref(frame);
                    }
                }
            }
            av_packet_unref(packet);
        }

        std::cout << "Successfully decoded " << monoAudioData_.size() << " samples into mono processing pipeline.\n";
        return !monoAudioData_.empty();
    }

    void DSProcessor::_applyHannWindow(std::vector<float>& frame) {
        if (frame.size() < 2) return; // avoid divide-by-zero below
        for (size_t n = 0; n < frame.size(); ++n) {
            float windowValue = 0.5f * (1.0f - std::cos(2.0f * M_PI * n / (frame.size() - 1)));
            frame[n] *= windowValue;
        }
    }

    std::vector<std::complex<float>> DSProcessor::_computeFFT(const std::vector<float>& frame) {
        // real input -> fftSize_/2 + 1 unique complex bins (nyquist packing).
        size_t binCount = fftSize_ / 2 + 1;
        std::vector<std::complex<float>> output(binCount, std::complex<float>(0.0f, 0.0f));

        if (!fftPlan_ || frame.size() != fftSize_) {
            // plan wasn't allocated, or caller passed a frame of the wrong
            // size (kiss_fftr has no bounds checking of its own).
            std::cerr << "DSProcessor Error: _computeFFT called with invalid state "
                          "(plan=" << (fftPlan_ != nullptr) << ", frame.size()=" << frame.size()
                      << ", expected=" << fftSize_ << ")\n";
            return output;
        }

        std::vector<kiss_fft_cpx> fftOut(binCount);
        // kiss_fftr wants a plain float* in, kiss_fft_cpx* out.
        kiss_fftr(fftPlan_, frame.data(), fftOut.data());

        for (size_t i = 0; i < binCount; ++i) {
            output[i] = std::complex<float>(fftOut[i].r, fftOut[i].i);
        }

        return output;
    }

    float DSProcessor::_calculateSpectralFlux(const std::vector<float>& currentMag, const std::vector<float>& prevMag) {
        float flux = 0.0f;
        for (size_t i = 0; i < currentMag.size(); ++i) {
            float diff = currentMag[i] - prevMag[i];
            if (diff > 0.0f) {
                flux += diff;
            }
        }
        return flux;
    }

    std::vector<AudioFrame> DSProcessor::Analyze() {
        std::vector<AudioFrame> timeline;
        if (monoAudioData_.empty()) return timeline;

        std::vector<float> prevMagnitudes(fftSize_ / 2 + 1, 0.0f);
        std::vector<float> currentFrame(fftSize_, 0.0f);

        for (size_t offset = 0; offset + fftSize_ < monoAudioData_.size(); offset += hopSize_) {
            std::copy(monoAudioData_.begin() + offset, monoAudioData_.begin() + offset + fftSize_, currentFrame.begin());
            
            double timestampMs = (static_cast<double>(offset) / sampleRate_) * 1000.0;

            _applyHannWindow(currentFrame);
            auto fftData = _computeFFT(currentFrame);

            size_t binCount = fftSize_ / 2 + 1;
            std::vector<float> magnitudes(binCount, 0.0f);
            for (size_t i = 0; i < binCount; ++i) {
                magnitudes[i] = std::abs(fftData[i]);
            }

            AudioFrame frameData{
                .TimestampMs = timestampMs,
                .SubBassEnergy = 0.0f,
                .MidEnergy = 0.0f,
                .HighEnergy = 0.0f,
                .TotalFlux = _calculateSpectralFlux(magnitudes, prevMagnitudes)
            };

            float binWidth = static_cast<float>(sampleRate_) / fftSize_;
            for (size_t i = 0; i < binCount; ++i) {
                float freq = i * binWidth;
                if (freq >= 20.0f && freq <= 60.0f) frameData.SubBassEnergy += magnitudes[i];
                else if (freq >= 250.0f && freq <= 2000.0f) frameData.MidEnergy += magnitudes[i];
                else if (freq >= 4000.0f) frameData.HighEnergy += magnitudes[i];
            }

            timeline.push_back(frameData);
            prevMagnitudes = std::move(magnitudes);
        }

        return timeline;
    }
}