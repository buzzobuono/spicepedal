#ifndef FILE_INPUT_GENERATOR_H
#define FILE_INPUT_GENERATOR_H

#include <sndfile.h>
#include <algorithm>
#include <iostream>
#include <stdexcept>

#include "signals/signal_generator.h"
#include "utils/wav_helper.h"

class FileInputGenerator : public SignalGenerator {

    private:
    
    std::string input_file;
    double sample_rate;
    double input_amplitude;
    double mean = 0.0;
    double maxNormalized = 0.0;
    double scale = 1.0;
    
    public:
    
    FileInputGenerator(const std::string& input_file, double input_amplitude)
        : input_file(input_file), input_amplitude(input_amplitude) {}

    std::vector<double> generate() override {
        WavHelper wav_helper;
        
        WavData wav_data = wav_helper.read(input_file);
        std::vector<double> signalIn = wav_data.samples;
        sample_rate = wav_data.sample_rate;

        // Calcola e rimuovi DC offset 
        double sum = 0.0;
        for (double s : signalIn) sum += s;
        mean = sum / signalIn.size();

        for (double& s : signalIn) s -= mean;

        // Calcola maxNormalized e fattore di scala
        for (double s : signalIn) {
            maxNormalized = std::max(maxNormalized, std::abs(s));
        }
        
        if (maxNormalized > 1e-10) {
            scale = input_amplitude / maxNormalized;
        }

        // Normalizza in Volt
        for (double& s : signalIn) s *= scale;

        return signalIn;
    
    }

    double getScaleFactor() const override {
        return scale;
    }

    double getMean() const override {
        return mean;
    }

    double getMaxNormalized() const override {
        return maxNormalized;
    }

    double getSampleRate() const override {
        return sample_rate;
    }

    void printInfo() const override {
        std::cout << "Circuit input: File" << std::endl;
        SF_INFO sfInfo;
        sfInfo.format = 0;
        SNDFILE* sound_file = sf_open(input_file.c_str(), SFM_READ, &sfInfo);
        sf_close(sound_file);
        std::cout << "   Channels: " << sfInfo.channels << std::endl;
        std::cout << "   Sample Rate: " << sfInfo.samplerate << "Hz" << std::endl;
        std::cout << "   Frames: " << sfInfo.frames << std::endl;
        std::cout << "   Numeric Format (bitmask): 0x" << std::hex << sfInfo.format << std::dec << std::endl;

        // Decodifica del formato audio (opzionale)
        int major_format = sfInfo.format & SF_FORMAT_TYPEMASK;
        int subtype = sfInfo.format & SF_FORMAT_SUBMASK;

        std::cout << "   Container Format: ";
        switch (major_format) {
            case SF_FORMAT_WAV:      std::cout << "WAV"; break;
            case SF_FORMAT_AIFF:     std::cout << "AIFF"; break;
            case SF_FORMAT_FLAC:     std::cout << "FLAC"; break;
            default:                 std::cout << "Other (" << std::hex << major_format << std::dec << ")"; break;
        }
        std::cout << std::endl;

        std::cout << "   Subtype (PCM, float, etc.): ";
        switch (subtype) {
            case SF_FORMAT_PCM_16:   std::cout << "PCM 16-bit"; break;
            case SF_FORMAT_PCM_24:   std::cout << "PCM 24-bit"; break;
            case SF_FORMAT_PCM_32:   std::cout << "PCM 32-bit"; break;
            case SF_FORMAT_FLOAT:    std::cout << "Float"; break;
            case SF_FORMAT_DOUBLE:   std::cout << "Double"; break;
            default:                 std::cout << "Other (" << std::hex << subtype << std::dec << ")"; break;
        }
        std::cout << std::endl;

        std::cout << std::endl;
        
    }
};

#endif
