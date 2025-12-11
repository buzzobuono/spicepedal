#ifndef SINUSOID_GENERATOR_H
#define SINUSOID_GENERATOR_H

#include "signals/signal_generator.h"
#include <cmath>
#include <iostream>

class SinusoidGenerator : public SignalGenerator {

    private:
    
    double sample_rate;
    int input_frequency;
    double input_duration;
    double input_amplitude;
    
    public:
    
    SinusoidGenerator(double sample_rate, int input_frequency, double input_duration, double input_amplitude)
        : sample_rate(sample_rate), input_frequency(input_frequency), input_duration(input_duration), input_amplitude(input_amplitude) {}

    std::vector<double> generate() override {
        size_t total_samples = static_cast<size_t>(sample_rate * input_duration);
        std::vector<double> signalIn(total_samples, 0.0);

        for (size_t i = 0; i < total_samples; ++i) {
            double t = i / sample_rate;
            signalIn[i] = input_amplitude * std::sin(2.0 * M_PI * input_frequency * t);
        }
        
        return signalIn;
    }

    double getMean() const override {
        return 0.0;
    }

    double getMaxNormalized() const override {
        return input_amplitude;
    }

    void printInfo() const override {
        std::cout << "Circuit input: Sinusoid" << std::endl;
        std::cout << "   Frequency: " << input_frequency << " Hz" << std::endl;
        std::cout << "   Amplitude: " << input_amplitude << " V" << std::endl;
        std::cout << "   Duration: " << input_duration << " s" << std::endl;
    }
    
};

#endif
