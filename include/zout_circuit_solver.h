#ifndef ZOUT_CIRCUIT_SOLVER_H
#define ZOUT_CIRCUIT_SOLVER_H

#include "circuit.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <memory>
#include <map>
#include <complex>
#include <Eigen/Dense>
#include <cmath>

class ZOutCircuitSolver {
private:
    Circuit& circuit;

    Eigen::MatrixXd G;
    Eigen::VectorXd I;
    Eigen::VectorXd V, V_new;

    // Parametri di ingresso
    double input_voltage;            // valore istantaneo della sorgente (V)
    double max_input_voltage;        // ampiezza
    double input_frequency;          // Hz
    double input_duration;           // secondi
    double sample_rate;              // campioni/sec
    int max_iterations;
    double tolerance_sq;             // tolleranza quadrata per NR

    // sorgente interna ingresso (resistenza serie)
    double source_g;                 // conduttanza 1/R_source

    // carico di test per misura Zout
    double load_g;                   // conduttanza del carico di test

    // LU solver (riutilizzato)
    Eigen::PartialPivLU<Eigen::MatrixXd> lu_solver;

    // statistiche
    uint64_t sample_count = 0;       
    uint64_t failed_count = 0;       
    uint64_t iteration_count = 0;    

    // risultati
    double Z_magnitude = 0.0;        // Ohm
    double Z_phase = 0.0;            // gradi

private:

    bool internal_solve(double dt) {
        sample_count++;
        
        // Newton-Raphson iteration
        for (int iter = 0; iter < max_iterations; iter++) {
            G.setZero();
            I.setZero();
            
            for (auto& component : circuit.components) {   
                component->stamp(G, I, V, dt);
            }
            
            // Sorgente di ingresso
            if (circuit.input_node > 0) {
                G(circuit.input_node, circuit.input_node) += source_g;
                I(circuit.input_node) += input_voltage * source_g;
            }
            
            // Carico di test sull'uscita
            if (circuit.output_node > 0) {
                G(circuit.output_node, circuit.output_node) += load_g;
            }
            
            // Ground node constraint
            G.row(0).setZero();
            G.col(0).setZero();
            G(0, 0) = 1.0;
            I(0) = 0.0;
            
            // Solve linear system
            lu_solver.compute(G);
            V_new = lu_solver.solve(I);
            
            // Check convergence
            double error_sq = (V_new - V).squaredNorm();
            V = V_new;
            
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
    ZOutCircuitSolver(Circuit& circuit, double sample_rate, int source_impedance, 
                      double max_input_voltage, double input_frequency, double input_duration, 
                      int max_iterations, double tolerance, double test_load_impedance = 1e6)
        : circuit(circuit),
            sample_rate(sample_rate),
            max_input_voltage(max_input_voltage),
            input_frequency(input_frequency),
            input_duration(input_duration),
            source_g(1.0 / source_impedance),
            load_g(1.0 / test_load_impedance),
            max_iterations(max_iterations),
            tolerance_sq(tolerance * tolerance) {

        // prepara vettori/matrici
        G = Eigen::MatrixXd::Zero(circuit.num_nodes, circuit.num_nodes);
        I = Eigen::VectorXd::Zero(circuit.num_nodes);
        V = Eigen::VectorXd::Zero(circuit.num_nodes);
        V_new = Eigen::VectorXd::Zero(circuit.num_nodes);

        input_voltage = 0.0;
    }

    ~ZOutCircuitSolver() = default;

    bool initialize() {
        V.setZero();
        G.setZero();
        I.setZero();
        circuit.reset();
        sample_count = failed_count = iteration_count = 0;
        return true;
    }

    bool solve() {
        // Metodo: misura della tensione a vuoto e con carico
        // Z_out = (V_open - V_loaded) / I_loaded
        // Alternativamente: due misure con carichi diversi
        
        double dt = 1.0 / sample_rate;
        int num_samples = static_cast<int>(std::ceil(input_duration * sample_rate));
        
        if (num_samples <= 4) num_samples = std::max(8, num_samples);
        int skip_samples = num_samples / 2;

        double omega = 2.0 * M_PI * input_frequency;

        // ===== PRIMA MISURA: A VUOTO (carico molto alto) =====
        double load_g_backup = load_g;
        load_g = 1e-12; // praticamente a vuoto

        std::complex<double> V_open_ph(0.0, 0.0);
        
        initialize();
        
        for (int s = 0; s < num_samples; ++s) {
            double t = s * dt;
            double v_src = max_input_voltage * std::sin(omega * t);
            input_voltage = v_src;

            internal_solve(dt);

            if (s >= skip_samples) {
                double v_out = 0.0;
                if (circuit.output_node >= 0 && 
                    static_cast<size_t>(circuit.output_node) < static_cast<size_t>(circuit.num_nodes)) {
                    v_out = V(circuit.output_node);
                }

                double cos_wt = std::cos(omega * t);
                double sin_wt = std::sin(omega * t);
                std::complex<double> weight(cos_wt, -sin_wt);

                V_open_ph += std::complex<double>(v_out, 0.0) * weight;
            }
        }

        int valid_samples = num_samples - skip_samples;
        if (valid_samples <= 0) return false;
        V_open_ph /= static_cast<double>(valid_samples);

        // ===== SECONDA MISURA: CON CARICO =====
        load_g = load_g_backup; // ripristina il carico di test

        std::complex<double> V_loaded_ph(0.0, 0.0);
        std::complex<double> I_loaded_ph(0.0, 0.0);

        initialize();

        for (int s = 0; s < num_samples; ++s) {
            double t = s * dt;
            double v_src = std::sin(omega * t);
            input_voltage = v_src;

            internal_solve(dt);

            if (s >= skip_samples) {
                double v_out = 0.0;
                if (circuit.output_node >= 0 && 
                    static_cast<size_t>(circuit.output_node) < static_cast<size_t>(circuit.num_nodes)) {
                    v_out = V(circuit.output_node);
                }

                // Corrente nel carico: I = V * G
                double i_load = v_out * load_g;

                double cos_wt = std::cos(omega * t);
                double sin_wt = std::sin(omega * t);
                std::complex<double> weight(cos_wt, -sin_wt);

                V_loaded_ph += std::complex<double>(v_out, 0.0) * weight;
                I_loaded_ph += std::complex<double>(i_load, 0.0) * weight;
            }
        }

        V_loaded_ph /= static_cast<double>(valid_samples);
        I_loaded_ph /= static_cast<double>(valid_samples);

        // ===== CALCOLO IMPEDENZA DI USCITA =====
        // Z_out = (V_open - V_loaded) / I_loaded
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

    void printOutputImpedance() const {
        std::cout << "   " << std::fixed << std::setprecision(1) << input_frequency << " Hz: "
                  << std::setprecision(2) << (Z_magnitude / 1000.0) << " kΩ, "
                  << std::setprecision(1) << Z_phase << "°" << std::endl;
        std::cout << std::endl;
    }

    // statistiche di convergenza
    double getFailurePercentage() const {
        return (sample_count > 0) ? (100.0 * static_cast<double>(failed_count) / static_cast<double>(sample_count)) : 0.0;
    }

    double getTotalIterations() const {
        return static_cast<double>(iteration_count);
    }

    double getTotalSamples() const {
        return static_cast<double>(sample_count);
    }

    double getMeanIterations() const {
        return (sample_count > 0) ? (static_cast<double>(iteration_count) / static_cast<double>(sample_count)) : 0.0;
    }

    void reset() {
        V.setZero();
        G.setZero();
        I.setZero();
        circuit.reset();
        sample_count = failed_count = iteration_count = 0;
    }

    // accessori
    double getZMagnitude() const { return Z_magnitude; }
    double getZPhaseDegrees() const { return Z_phase; }
};

#endif // ZOUT_CIRCUIT_SOLVER_H
