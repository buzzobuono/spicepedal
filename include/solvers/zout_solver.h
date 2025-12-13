#ifndef ZOUT_SOLVER_H
#define ZOUT_SOLVER_H

#include <iostream>
#include <fstream>
#include <iomanip>
#include <memory>
#include <map>
#include <complex>
#include <cmath>

#include "circuit.h"
#include "solvers/newton_raphson_solver.h"
#include "signals/signal_generator.h"
#include "signals/sinusoid_generator.h"

class ZOutSolver : public NewtonRaphsonSolver {

    private:
    
    double load_g;
    double current_load_g;
    
    double Z_magnitude = 0.0;
    double Z_phase = 0.0;

    std::vector<double> signalIn;
    std::unique_ptr<SinusoidGenerator> signal_generator;

    protected:
    
    void applySource(double dt) override {
        NewtonRaphsonSolver::applySource(dt);
        
        if (circuit.output_node > 0) {
            G(circuit.output_node, circuit.output_node) += current_load_g; 
        }
    }


    public:
    
    ZOutSolver(Circuit& circuit, double sample_rate, int source_impedance, double input_amplitude, int input_frequency, double input_duration, int max_iterations, double tolerance, double test_load_impedance = 1e6)
        : NewtonRaphsonSolver(circuit, sample_rate, source_impedance, max_iterations, tolerance),
          signal_generator(std::make_unique<SinusoidGenerator>(sample_rate, input_frequency, input_duration, input_amplitude)),
            load_g(1.0 / test_load_impedance),
            current_load_g(1.0 / test_load_impedance)
    {
        signal_generator->printInfo();
        signalIn = signal_generator->generate();
        this->input_voltage = 0.0;
    }

    ~ZOutSolver() override = default;

    bool initialize() override {
        NewtonRaphsonSolver::initialize(); 
        warmUp(5.0);
        return true;
    }
    
    bool solveImpl() override {

        int num_samples = signalIn.size();
        

        double omega = 2.0 * M_PI * signal_generator->getFrequency();

        double load_g_backup = load_g;
        
        current_load_g = 1e-12;

        std::complex<double> V_open_ph(0.0, 0.0);
        

        for (int s = 0; s < num_samples; ++s) {
            double t = s * dt;
            
            double v_src = signalIn[s];

            this->input_voltage = v_src;

            bool ok = runNewtonRaphson(dt);
            (void)ok;

            
            double v_out = 0.0;
            if (circuit.output_node >= 0 && static_cast<size_t>(circuit.output_node) < static_cast<size_t>(circuit.num_nodes)) {
                v_out = V(circuit.output_node); 
            }

            double cos_wt = std::cos(omega * t);
            double sin_wt = std::sin(omega * t);
            std::complex<double> weight(cos_wt, -sin_wt);

            V_open_ph += std::complex<double>(v_out, 0.0) * weight;
           
        }

        V_open_ph /= static_cast<double>(num_samples);

        current_load_g = load_g_backup;

        std::complex<double> V_loaded_ph(0.0, 0.0);
        std::complex<double> I_loaded_ph(0.0, 0.0);

        initialize();

        for (int s = 0; s < num_samples; ++s) {
            double t = s * dt;
            
            double v_src = signalIn[s];

            this->input_voltage = v_src;
            
            runNewtonRaphson(dt);

            
            double v_out = 0.0;
            if (circuit.output_node >= 0 && static_cast<size_t>(circuit.output_node) < static_cast<size_t>(circuit.num_nodes)) {
                v_out = V(circuit.output_node);
            }
            
            double i_load = v_out * current_load_g;

            double cos_wt = std::cos(omega * t);
            double sin_wt = std::sin(omega * t);
            std::complex<double> weight(cos_wt, -sin_wt);

            V_loaded_ph += std::complex<double>(v_out, 0.0) * weight;
            I_loaded_ph += std::complex<double>(i_load, 0.0) * weight;
        
        }

        V_loaded_ph /= static_cast<double>(num_samples);
        I_loaded_ph /= static_cast<double>(num_samples);

        const double min_current = 1e-12;
        std::complex<double> Z_out;
        
        if (std::abs(I_loaded_ph) < min_current) {
            Z_out = std::complex<double>(1e12, 0.0);
        } else {
            Z_out = (V_open_ph - V_loaded_ph) / I_loaded_ph;
        }
        
        Z_magnitude = std::abs(Z_out);
        Z_phase = std::arg(Z_out) * 180.0 / M_PI;

        return true;
    }

    void printResult() override {
        std::cout << "Output Impedence Analysis" << std::endl;
        std::cout << "   " << std::fixed << std::setprecision(1) << signal_generator->getFrequency() << " Hz: "
                  << std::setprecision(2) << (Z_magnitude / 1000.0) << " kΩ, "
                  << std::setprecision(1) << Z_phase << "°" << std::endl;
        std::cout << std::endl;
    }
};

#endif
