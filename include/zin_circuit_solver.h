#ifndef ZIN_CIRCUIT_SOLVER_H
#define ZIN_CIRCUIT_SOLVER_H

#include "circuit.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <memory>
#include <map>
#include <complex>
#include <Eigen/Dense>
#include <cmath>

class ZInCircuitSolver {
private:
    Circuit& circuit;

    Eigen::MatrixXd G;
    Eigen::VectorXd I;
    Eigen::VectorXd V, V_new;

    // Parametri di ingresso
    double input_voltage;            // valore istantaneo della sorgente (V)
    double input_frequency;          // Hz
    double input_duration;           // secondi
    double sample_rate;              // campioni/sec
    int max_iterations;
    double tolerance_sq;             // tolleranza quadrata per NR

    // sorgente interna (resistenza serie)
    double source_g;                 // conduttanza 1/R

    // LU solver (riutilizzato)
    Eigen::PartialPivLU<Eigen::MatrixXd> lu_solver;

    // statistiche
    uint64_t sample_count = 0;       // numero di passi temporali simulati
    uint64_t failed_count = 0;       // numero di passi che non hanno convergito
    uint64_t iteration_count = 0;    // somma iterazioni NR effettuate

    // risultati
    double Z_magnitude = 0.0;        // Ohm
    double Z_phase = 0.0;            // gradi

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

                
        tolerance_sq = tolerance * tolerance;

        // prepara vettori/matrici alla dimensione corretta
        G = Eigen::MatrixXd::Zero(circuit.num_nodes, circuit.num_nodes);
        I = Eigen::VectorXd::Zero(circuit.num_nodes);
        V = Eigen::VectorXd::Zero(circuit.num_nodes);
        V_new = Eigen::VectorXd::Zero(circuit.num_nodes);

        input_voltage = 0.0;
    }

    ~ZInCircuitSolver() = default;

    bool initialize() {
        V.setZero();
        G.setZero();
        I.setZero();
        circuit.reset();
        sample_count = failed_count = iteration_count = 0;
        return true;
    }

    bool solve() {
        // dt e numero di campioni
        double dt = 1.0 / sample_rate;
        int num_samples = static_cast<int>(std::ceil(input_duration * sample_rate));
        
        if (num_samples <= 4) num_samples = std::max(8, num_samples);

        int skip_samples = num_samples / 2; // scarta transitori nella prima metà

        
        // accumulatore per i fasori (metodo: calcolo del rapporto tra il fasore della tensione di ingresso e del la corrente misurata)
        std::complex<double> V_ph(0.0, 0.0);
        std::complex<double> I_ph(0.0, 0.0);

        double omega = 2.0 * M_PI * input_frequency;

        // loop temporale
        for (int s = 0; s < num_samples; ++s) {
            double t = s * dt;

            // segnale di ingresso: sin(omega t) amplitude 1 V
            double v_src = std::sin(omega * t);
            input_voltage = v_src;

            // risolvo lo stato per questo passo
            bool ok = internal_solve(dt);
            (void)ok; // non interrompo la misura se qualche passo non converge: lo conto nelle statistiche

            // se siamo nella seconda metà, accumuliamo i fasori
            if (s >= skip_samples) {
                // corrente istantanea dalla sorgente (Ohm's law sulla resistenza serie): i(t) = (v_src - v_node)/R = (v_src - V(node))*g
                double v_node = 0.0;
                if (circuit.input_node >= 0 && static_cast<size_t>(circuit.input_node) < static_cast<size_t>(circuit.num_nodes)) {
                    v_node = V(circuit.input_node);
                }
                double i_inst = (v_src - v_node) * source_g;

                // peso complesso = e^{-j omega t} = cos(omega t) - j sin(omega t)
                double cos_wt = std::cos(omega * t);
                double sin_wt = std::sin(omega * t);
                std::complex<double> weight(cos_wt, -sin_wt);

                // accumula fasori per tensione e corrente (usiamo il valore istantaneo della sorgente come riferimento)
                V_ph += std::complex<double>(v_src, 0.0) * weight;
                I_ph += std::complex<double>(i_inst, 0.0) * weight;
            }
        }

        // normalizziamo per il numero di campioni utilizzati
        int valid_samples = num_samples - skip_samples;
        if (valid_samples <= 0) return false;

        V_ph /= static_cast<double>(valid_samples);
        I_ph /= static_cast<double>(valid_samples);

        // Se la corrente è trascurabile, assegna impedenza molto alta
        const double min_current = 1e-12;
        std::complex<double> Z_in;
        if (std::abs(I_ph) < min_current) {
            Z_in = std::complex<double>(1e12, 0.0);
        } else {
            // Attenzione convenzione di fase: abbiamo usato v_src come sin(omega t) e peso e^{-j omega t}; risultato è correttamente il fasore di V e I
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

#endif // ZIN_CIRCUIT_SOLVER_H
