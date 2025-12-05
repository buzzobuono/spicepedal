#ifndef ZIN_CIRCUIT_SOLVER_H
#define ZIN_CIRCUIT_SOLVER_H

#include "circuit.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <memory>
#include <map>
#include <Eigen/Dense>

class ZInCircuitSolver {

private:
    Circuit& circuit;
    Eigen::MatrixXd G;
    Eigen::VectorXd I, V, V_new;
    
    double input_voltage;

    double input_frequency;
    double input_duration;

    Eigen::PartialPivLU<Eigen::MatrixXd> lu_solver;
    
    uint64_t sample_count = 0;
    uint64_t failed_count = 0;
    uint64_t iteration_count = 0;
    
    double sample_rate;
    int max_iterations;
    double tolerance_sq;
    double source_g;

    double Z_magnitude;
    double Z_phase;

private:

    bool internal_solve(double dt) {
        sample_count++;
        
        // Newton-Raphson iteration
        double final_error_sq = 0.0;
        
        for (int iter = 0; iter < max_iterations; iter++) {
            G.setZero();
            I.setZero();
            
            for (auto& component : circuit.components) {   
                component->stamp(G, I, V, dt);
            }
            
            if (circuit.input_node > 0) {
                G(circuit.input_node, circuit.input_node) += source_g;
                I(circuit.input_node) += input_voltage * source_g;
            }
            
            // Ground node constraint
            G.row(0).setZero();
            G.col(0).setZero();
            G(0, 0) = 1.0;
            I(0) = 0.0;
            
            // Solve linear system (usa LU per robustezza)
            lu_solver.compute(G);
            V_new = lu_solver.solve(I);
            
            // Check convergence
            double error_sq = (V_new - V).squaredNorm();
            V = V_new;
            // Convergence check
            if (error_sq < tolerance_sq) {
                // Update component history
                for (auto& comp : circuit.components) {
                    comp->updateHistory(V, dt);
                }
                iteration_count += iter + 1;
                return true;
            }
        }
        failed_count++;
        iteration_count += max_iterations;
        return false;
    }

public:
    ZInCircuitSolver(Circuit& circuit, double sample_rate, int source_impedance, double input_frequency, double input_duration, int max_iterations, double tolerance) 
        : circuit(circuit),
          sample_rate(sample_rate),
          input_frequency(input_frequency),
          input_duration(input_duration),
          source_g(1.0 / source_impedance),
          max_iterations(max_iterations),
          tolerance_sq(tolerance*tolerance) {
        
        G.resize(circuit.num_nodes, circuit.num_nodes);
        I.resize(circuit.num_nodes);
        V.resize(circuit.num_nodes);
        V_new.resize(circuit.num_nodes);
        V.setZero();
    }
    
    ~ZInCircuitSolver() {
    }

    bool initialize() {
        return true;
    }

    
    bool solve() {
        double dt = 1.0 / sample_rate;
        int num_samples = static_cast<int>(input_duration / dt);
        int skip_samples = num_samples / 2;  // Salta transitori

        double current_real_sum = 0.0;
        double current_imag_sum = 0.0;
        int valid_samples = 0;
        double omega = 2.0 * M_PI * input_frequency;
        
        size_t total_samples = static_cast<size_t>(sample_rate * input_duration);
        
        // Simula per la durata specificata
        for (int s = 0; s < total_samples; s++) {
            double t = s * dt;
            double v_ac = std::sin(omega * t);  // 1V amplitude AC
            input_voltage = v_ac;

            internal_solve(dt);

            // Misura corrente solo dopo stabilizzazione (seconda metà)
            if (s >= skip_samples) {
                // Componente reale: in fase con tensione
                double i_real = (v_ac - V(circuit.input_node)) * source_g;

                // Componente immaginaria: in quadratura con tensione (sfasata 90°)
                double v_quad = std::cos(omega * t);
                double i_imag = (v_quad - V(circuit.input_node)) * source_g;

                current_real_sum += i_real;
                current_imag_sum += i_imag;
                valid_samples++;
            }
        }

        // Corrente complessa media
        std::complex<double> I_avg(
            current_real_sum / valid_samples,
            current_imag_sum / valid_samples
        );

        double I_magnitude = std::abs(I_avg);

        // Zin = V / I = (1+j0) / I_avg
        std::complex<double> Z_in = (I_magnitude > 1e-12) ? std::complex<double>(1.0, 0.0) / I_avg : std::complex<double>(1e12, 0.0);  // Impedenza molto alta se corrente trascurabile

        Z_magnitude = std::abs(Z_in);
        Z_phase = std::arg(Z_in) * 180.0 / M_PI;  // Fase in gradi

        return true;
    }

    void printInputImpedance() {
        std::cout << "   " << std::fixed << std::setprecision(1) << input_frequency << " Hz: " 
                    << std::setprecision(2) << (Z_magnitude / 1000.0) << " kΩ, "
                    << std::setprecision(1) << Z_phase << "°" << std::endl;
        std::cout << std::endl;
    }

    double getFailurePercentage() {
        return (sample_count > 0) ? (100.0 * failed_count / sample_count) : 0.0;
    }
    
    double getTotalIterations() {
        return iteration_count;
    }
    
    double getTotalSamples() {
        return sample_count;
    }
    
    double getMeanIterations() {
        return (sample_count > 0) ? (1.0 * iteration_count / sample_count) : 0.0;
    }

    void reset() {
        V.setZero();
        circuit.reset();
    }
    
};

#endif