#ifndef ZIN_SOLVER_H
#define ZIN_SOLVER_H

#include <iostream>
#include <fstream>
#include <iomanip>
#include <memory>
#include <map>
#include <complex>
#include <Eigen/Dense>
#include <cmath>

#include "circuit.h"
#include "solvers/newton_raphson_solver.h"
#include "signals/signal_generator.h"
#include "signals/sinusoid_generator.h"

class ZInSolver : public NewtonRaphsonSolver {

    private:
    
    
    double Z_magnitude = 0.0;
    double Z_phase = 0.0;
    
    std::vector<double> signalIn;
    std::unique_ptr<SinusoidGenerator> signal_generator;

    public:
    
    ZInSolver(Circuit& circuit, double sample_rate, int source_impedance, double input_amplitude, double input_frequency, double input_duration, int max_iterations, double tolerance)
        : NewtonRaphsonSolver(circuit, sample_rate, source_impedance, max_iterations, tolerance),
          signal_generator(std::make_unique<SinusoidGenerator>(sample_rate, input_frequency, input_duration, input_amplitude))
    {
        signal_generator->printInfo();
        signalIn = signal_generator->generate();
        this->input_voltage = 0.0;
    }

    ~ZInSolver() override = default;

    bool initialize() override {
        NewtonRaphsonSolver::initialize(); 
        warmUp(5.0);
        return true;
    }

    bool solveImpl() override {
        
        int num_samples = signalIn.size();
        
        std::complex<double> V_ph(0.0, 0.0);
        std::complex<double> I_ph(0.0, 0.0);

        double omega = 2.0 * M_PI * signal_generator->getFrequency();

        for (int s = 0; s < num_samples; ++s) {
            double t = s * dt;
            
            double v_src = signalIn[s];
            
            this->input_voltage = v_src;

            bool ok = runNewtonRaphson(dt);
            (void)ok;

            
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
        
        V_ph /= static_cast<double>(num_samples);
        I_ph /= static_cast<double>(num_samples);
        
        const double min_current = 1e-12;
        std::complex<double> Z_in;
        if (std::abs(I_ph) < min_current) {
            Z_in = std::complex<double>(1e12, 0.0);
        } else {
            Z_in = V_ph / I_ph;
        }
        
        Z_magnitude = std::abs(Z_in);
        Z_phase = std::arg(Z_in) * 180.0 / M_PI;
        
        return true;
    }
    
    void printResult() override {
        std::cout << "Input Impedence Analysis" << std::endl;
        std::cout << "   " << std::fixed << std::setprecision(1) << signal_generator->getFrequency() << " Hz: "
                  << std::setprecision(2) << (Z_magnitude / 1000.0) << " kΩ, "
                  << std::setprecision(1) << Z_phase << "°" << std::endl;
        std::cout << std::endl;
    }
};

#endif
