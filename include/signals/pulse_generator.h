#ifndef PULSE_GENERATOR_H
#define PULSE_GENERATOR_H

#include "signals/signal_generator.h"
#include <cmath>
#include <iostream>

class PulseGenerator : public SignalGenerator {

private:
    double sample_rate;
    double input_duration;
    double v_initial;      // Valore iniziale
    double v_pulsed;       // Valore durante il pulse
    double t_delay;        // Tempo di delay prima del pulse
    double t_rise;         // Tempo di salita
    double t_fall;         // Tempo di discesa
    double t_pulse_width;  // Larghezza del pulse (al valore alto)
    double t_period;       // Periodo (per pulse ripetuti)
    
public:
    PulseGenerator(double sample_rate, double input_duration, 
                   double v_initial = 0.0, double v_pulsed = 1.0,
                   double t_delay = 0.0, double t_rise = 0.0, double t_fall = 0.0,
                   double t_pulse_width = 1.0, double t_period = 0.0)
        : sample_rate(sample_rate), input_duration(input_duration),
          v_initial(v_initial), v_pulsed(v_pulsed),
          t_delay(t_delay), t_rise(t_rise), t_fall(t_fall),
          t_pulse_width(t_pulse_width), t_period(t_period) {}

    std::vector<double> generate() override {
        size_t total_samples = static_cast<size_t>(sample_rate * input_duration);
        std::vector<double> signalIn(total_samples, v_initial);

        for (size_t i = 0; i < total_samples; ++i) {
            double t = i / sample_rate;
            signalIn[i] = calculatePulseValue(t);
        }
        
        return signalIn;
    }

    double getMean() const override {
        if (t_period > 0.0 && t_period > t_delay + t_rise + t_pulse_width + t_fall) {
            double duty_cycle = (t_pulse_width + 0.5 * (t_rise + t_fall)) / t_period;
            return v_initial + duty_cycle * (v_pulsed - v_initial);
        } else {
            double active_time = t_pulse_width + 0.5 * (t_rise + t_fall);
            return v_initial + (active_time / input_duration) * (v_pulsed - v_initial);
        }
    }

    double getMaxNormalized() const override {
        return std::max(std::abs(v_initial), std::abs(v_pulsed));
    }

    double getSampleRate() const override {
        return sample_rate;
    }

    void printInfo() const override {
        std::cout << "Circuit input: Pulse" << std::endl;
        std::cout << "   Initial value: " << v_initial << " V" << std::endl;
        std::cout << "   Pulsed value: " << v_pulsed << " V" << std::endl;
        std::cout << "   Delay time: " << t_delay << " s" << std::endl;
        std::cout << "   Rise time: " << t_rise << " s" << std::endl;
        std::cout << "   Fall time: " << t_fall << " s" << std::endl;
        std::cout << "   Pulse width: " << t_pulse_width << " s" << std::endl;
        if (t_period > 0.0) {
            std::cout << "   Period: " << t_period << " s" << std::endl;
        }
        std::cout << "   Duration: " << input_duration << " s" << std::endl;
        std::cout << std::endl;
    }

private:
    double calculatePulseValue(double t) const {
        // Prima del delay iniziale
        if (t < t_delay) {
            return v_initial;
        }
        
        // Tempo relativo dall'inizio del primo pulse
        double t_from_start = t - t_delay;
        
        // Se il periodo è specificato e siamo oltre il primo ciclo
        if (t_period > 0.0) {
            // Calcola in quale ciclo ci troviamo e il tempo relativo nel ciclo
            t_from_start = std::fmod(t_from_start, t_period);
        }
        
        // Fase di salita
        if (t_from_start < t_rise) {
            if (t_rise == 0.0) return v_pulsed;
            return v_initial + (v_pulsed - v_initial) * (t_from_start / t_rise);
        }
        
        // Fase alta del pulse
        if (t_from_start < t_rise + t_pulse_width) {
            return v_pulsed;
        }
        
        // Fase di discesa
        if (t_from_start < t_rise + t_pulse_width + t_fall) {
            if (t_fall == 0.0) return v_initial;
            double t_fall_elapsed = t_from_start - t_rise - t_pulse_width;
            return v_pulsed - (v_pulsed - v_initial) * (t_fall_elapsed / t_fall);
        }
        
        // Dopo il pulse (resto del periodo o fine segnale)
        return v_initial;
    }
    
};

#endif