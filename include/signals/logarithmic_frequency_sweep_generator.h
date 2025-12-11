#ifndef LOG_FREQUENCY_SWEEP_GENERATOR_H
#define LOG_FREQUENCY_SWEEP_GENERATOR_H

#include "signals/signal_generator.h"
#include <cmath>
#include <iostream>

class LogarithmicFrequencySweepGenerator : public SignalGenerator {

    private:
    
    double sample_rate;
    double input_duration;
    double input_amplitude;
    
    public:
    
    LogarithmicFrequencySweepGenerator(double sample_rate, double input_duration, double input_amplitude)
        : sample_rate(sample_rate), input_duration(input_duration), input_amplitude(input_amplitude) {}

    std::vector<double> generate() override {
        size_t total_samples = static_cast<size_t>(sample_rate * input_duration);
        std::vector<double> signalIn(total_samples, 0.0);
        int f_start = 1;
        int f_end = sample_rate / 2.0;
        double log_f_start = std::log(f_start);
        double log_f_end = std::log(f_end);
        double k = (log_f_end - log_f_start) / input_duration;
            
        for (size_t i = 0; i < total_samples; ++i) {
            double t = i / sample_rate;
            double phase = 2.0 * M_PI * f_start * (std::exp(k * t) - 1.0) / k;
            signalIn[i] = input_amplitude * std::sin(phase);
        }
        
        return signalIn;
    }

    double getMaxNormalized() const override {
        return input_amplitude;
    }

    void printInfo() const override {
        int f_start = 1;
        int f_end = sample_rate / 2.0;
        std::cout << "Circuit input: Logarithmic Sweep" << std::endl;
        std::cout << "   Range: " << f_start << " Hz -> " << f_end << " Hz" << std::endl;
        std::cout << "   Amplitude: " << input_amplitude << " V" << std::endl;
        std::cout << "   Duration: " << input_duration << " s" << std::endl;
    }
    
};

#endif
