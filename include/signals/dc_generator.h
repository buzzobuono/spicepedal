#ifndef DC_GENERATOR_H
#define DC_GENERATOR_H

#include "signals/signal_generator.h"
#include <cmath>
#include <iostream>

class DCGenerator : public SignalGenerator {

    private:
    
    double sample_rate;
    double input_duration;
    double input_amplitude;
    
    public:
    
    DCGenerator(double sample_rate, double input_duration, double input_amplitude)
        : sample_rate(sample_rate), input_duration(input_duration), input_amplitude(input_amplitude) {}

    std::vector<double> generate() override {
        size_t total_samples = static_cast<size_t>(sample_rate * input_duration);
        std::vector<double> signalIn(total_samples, 0.0);
        
        for (size_t i = 0; i < total_samples; ++i) {
            signalIn[i] = input_amplitude;
        }
        
        return signalIn;
    }

    double getMean() const override {
        return input_amplitude;
    }

    double getMaxNormalized() const override {
        return input_amplitude;
    }

    double getSampleRate() const override {
        return sample_rate;
    }

    void printInfo() const override {
        std::cout << "Circuit input: DC" << std::endl;
        std::cout << "   Amplitude: " << input_amplitude << " V" << std::endl;
        std::cout << "   Duration: " << input_duration << " s" << std::endl;
        std::cout << std::endl;
    }
    
};

#endif
