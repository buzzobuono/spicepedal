#ifndef FILE_INPUT_GENERATOR_H
#define FILE_INPUT_GENERATOR_H

#include <sndfile.h>
#include <samplerate.h>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "signals/signal_generator.h"
#include "utils/wav_helper.h"

class FileInputGenerator : public SignalGenerator {

    private:
    std::string input_file;
    double target_sample_rate;
    double original_sample_rate = 0.0;
    double input_amplitude;
    double mean = 0.0;
    double maxNormalized = 0.0;
    double scale = 1.0;
    
    public:
    FileInputGenerator(double sample_rate, const std::string& input_file, double input_amplitude)
        : target_sample_rate(sample_rate), input_file(input_file), input_amplitude(input_amplitude) {}

    std::vector<double> generate(double input_gain) override {
        WavHelper wav_helper;
        
        WavData wav_data = wav_helper.read(input_file);
        std::vector<double> signalIn = std::move(wav_data.samples);
        original_sample_rate = static_cast<double>(wav_data.sample_rate);

        if (std::abs(original_sample_rate - target_sample_rate) > 0.001) {
            double ratio = target_sample_rate / original_sample_rate;
            long max_output_frames = static_cast<long>(signalIn.size() * ratio) + 2;

            std::vector<float> floatIn(signalIn.begin(), signalIn.end());
            std::vector<float> floatOut(max_output_frames);

            SRC_DATA src_data;
            src_data.data_in = floatIn.data();
            src_data.data_out = floatOut.data();
            src_data.input_frames = static_cast<long>(floatIn.size());
            src_data.output_frames = max_output_frames;
            src_data.src_ratio = ratio;

            int error = src_simple(&src_data, SRC_SINC_MEDIUM_QUALITY, 1);
            if (error) {
                throw std::runtime_error(src_strerror(error));
            }

            signalIn.assign(floatOut.begin(), floatOut.begin() + src_data.output_frames_gen);
        }

        double sum = 0.0;
        for (double s : signalIn) sum += s;
        mean = sum / signalIn.size();

        for (double& s : signalIn) s -= mean;

        maxNormalized = 0.0;
        for (double s : signalIn) {
            maxNormalized = std::max(maxNormalized, std::abs(s));
        }
        
        if (maxNormalized > 1e-10) {
            scale = input_amplitude / maxNormalized;
        }

        for (double& s : signalIn) s *= scale;

        for (double& s : signalIn) s *= input_gain;

        return signalIn;
    }

    double getScaleFactor() const override { return scale; }
    double getMean() const override { return mean; }
    double getMaxNormalized() const override { return maxNormalized; }
    double getSampleRate() const override { return target_sample_rate; }

    void printInfo() const override {
        std::cout << "Circuit input: File (Resampled)" << std::endl;
        std::cout << "   File: " << input_file << std::endl;
        std::cout << "   Original SR: " << original_sample_rate << " Hz" << std::endl;
        std::cout << "   Target SR:   " << target_sample_rate << " Hz" << std::endl;
        
        SF_INFO sfInfo;
        sfInfo.format = 0;
        SNDFILE* sound_file = sf_open(input_file.c_str(), SFM_READ, &sfInfo);
        if (sound_file) {
            std::cout << "   Channels:    " << sfInfo.channels << std::endl;
            sf_close(sound_file);
        }
        std::cout << std::endl;
    }
};

#endif
