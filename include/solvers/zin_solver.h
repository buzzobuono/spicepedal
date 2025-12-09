#ifndef ZIN_SOLVER_H
#define ZIN_SOLVER_H

#include "solvers/newton_raphson_solver.h" // Includi la classe base intermedia
#include "circuit.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <memory>
#include <map>
#include <complex>
#include <Eigen/Dense>
#include <cmath>

class ZInSolver : public NewtonRaphsonSolver {

    private:
    
    double input_amplitude;
    double input_frequency;
    double input_duration;
    
    double Z_magnitude = 0.0;
    double Z_phase = 0.0;
    
    public:
    
    ZInSolver(Circuit& circuit, double sample_rate, int source_impedance, 
            double input_amplitude,
            double input_frequency,
            double input_duration,
            int max_iterations,
            double tolerance)
        : NewtonRaphsonSolver(circuit, sample_rate, source_impedance, max_iterations, tolerance),
            input_amplitude(input_amplitude),
            input_frequency(input_frequency),
            input_duration(input_duration)
    {
        this->input_voltage = 0.0;
    }

    ~ZInSolver() override = default;
    
    bool solve() override {
        double sample_rate = 1 / dt;
        int num_samples = static_cast<int>(std::ceil(input_duration * sample_rate));
        
        if (num_samples <= 4) num_samples = std::max(8, num_samples);

        int skip_samples = num_samples / 2;
        
        std::complex<double> V_ph(0.0, 0.0);
        std::complex<double> I_ph(0.0, 0.0);

        double omega = 2.0 * M_PI * input_frequency;

        for (int s = 0; s < num_samples; ++s) {
            double t = s * dt;

            double v_src = input_amplitude * std::sin(omega * t);
            
            this->input_voltage = v_src; 

            bool ok = runNewtonRaphson(dt);
            (void)ok;

            if (s >= skip_samples) {
                double v_node = 0.0;
                if (circuit.input_node >= 0 && static_cast<size_t>(circuit.input_node) < static_cast<size_t>(circuit.num_nodes)) {
                    v_node = V(circuit.input_node);
                }
                double i_inst = (v_src - v_node) * source_g; 

                double cos_wt = std::cos(omega * t);
                double sin_wt = std::sin(omega * t);
                std::complex<double> weight(cos_wt, -sin_wt);

                V_ph += std::complex<double>(v_src, 0.0) * weight;
                I_ph += std::complex<double>(i_inst, 0.0) * weight;
            }
        }
        
        int valid_samples = num_samples - skip_samples;
        if (valid_samples <= 0) return false;

        V_ph /= static_cast<double>(valid_samples);
        I_ph /= static_cast<double>(valid_samples);
        
        const double min_current = 1e-12;
        std::complex<double> Z_in;
        if (std::abs(I_ph) < min_current) {
            Z_in = std::complex<double>(1e12, 0.0);
        } else {
            Z_in = V_ph / I_ph;
        }
        
        Z_magnitude = std::abs(Z_in);
        Z_phase = std::arg(Z_in) * 180.0 / M_PI; // in gradi

        return true;
    }
    
    void printInputImpedance() const {
        std::cout << "   " << std::fixed << std::setprecision(1) << input_frequency << " Hz: "
                  << std::setprecision(2) << (Z_magnitude / 1000.0) << " k\u03A9, "
                  << std::setprecision(1) << Z_phase << "°" << std::endl;
        std::cout << std::endl;
    }
    
    double getZMagnitude() const {
        return Z_magnitude;
    }
    
    double getZPhaseDegrees() const {
        return Z_phase;
    }
    
};

#endif

