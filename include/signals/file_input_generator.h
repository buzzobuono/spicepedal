#ifndef FILE_INPUT_GENERATOR_H
#define FILE_INPUT_GENERATOR_H

#include "signals/signal_generator.h"
#include <sndfile.h>
#include <algorithm>
#include <iostream>
#include <stdexcept>

class FileInputGenerator : public SignalGenerator {

    private:
    
    std::string input_file;
    double& sample_rate_ref; // Riferimento al sample_rate per aggiornarlo
    double input_amplitude;
    double mean = 0.0;
    double maxNormalized = 0.0;
    double scale = 1.0;
    
    public:
    
    FileInputGenerator(const std::string& input_file, double& sample_rate_ref, double input_amplitude)
        : input_file(input_file), sample_rate_ref(sample_rate_ref), input_amplitude(input_amplitude) {}

    std::vector<double> generate() override {
        SF_INFO sfInfo;
        sfInfo.format = 0;
        SNDFILE* file = sf_open(input_file.c_str(), SFM_READ, &sfInfo);
        
        if (!file) {
            throw std::runtime_error("Errore apertura WAV: " + std::string(sf_strerror(file)));
        }
        
        sample_rate_ref = sfInfo.samplerate;
        
        // Leggi tutti i sample
        std::vector<double> buffer(sfInfo.frames * sfInfo.channels);
        sf_count_t numFrames = sf_readf_double(file, buffer.data(), sfInfo.frames);
        sf_close(file);
        
        if (numFrames != sfInfo.frames) {
            throw std::runtime_error("Errore lettura sample: frames mismatch");
        }
        
        // Estrai canale sinistro (se multicanale)
        std::vector<double> signalIn(numFrames, 0.0);
        for (sf_count_t i = 0; i < numFrames; i++) {
            signalIn[i] = buffer[i * sfInfo.channels];
        }

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

    void printInfo() const override {
        std::cout << "Circuit input: File" << std::endl;
        
    }
};

#endif
