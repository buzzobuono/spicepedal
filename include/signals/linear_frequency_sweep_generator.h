#ifndef LIN_FREQUENCY_SWEEP_GENERATOR_H
#define LIN_FREQUENCY_SWEEP_GENERATOR_H

#include "signals/signal_generator.h"
#include <cmath>
#include <iostream>

class LinearFrequencySweepGenerator : public SignalGenerator {

    private:
    
    double sample_rate;
    double input_duration;
    double input_amplitude;
    
    public:
    
    LinearFrequencySweepGenerator(double sample_rate, double input_duration, double input_amplitude)
        : sample_rate(sample_rate), input_duration(input_duration), input_amplitude(input_amplitude) {}

    std::vector<double> generate(double input_gain) override {
        size_t total_samples = static_cast<size_t>(sample_rate * input_duration);
        std::vector<double> signalIn(total_samples, 0.0);
        int f_start = 1;
        int f_end = sample_rate / 2.0;
        double k = (f_end - f_start) / input_duration;
        for (size_t i = 0; i < total_samples; ++i) {
            double t = i / sample_rate;
            double phase = 2.0 * M_PI * (f_start * t + 0.5 * k * t * t);
            signalIn[i] = input_gain * input_amplitude * std::sin(phase);
        }

        return signalIn;
    }

    double getMaxNormalized() const override {
        return input_amplitude;
    }

    double getSampleRate() const override {
        return sample_rate;
    }

    void printInfo() const override {
        int f_start = 1;
        int f_end = sample_rate / 2.0;
        std::cout << "Circuit input: Linear Sweep" << std::endl;
        std::cout << "   Range: " << f_start << " Hz -> " << f_end << " Hz" << std::endl;
        std::cout << "   Amplitude: " << input_amplitude << " V" << std::endl;
        std::cout << "   Duration: " << input_duration << " s" << std::endl;
        std::cout << std::endl;
    }
    
};

#endif
