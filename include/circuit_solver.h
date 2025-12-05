#ifndef CIRCUIT_SOLVER_H
#define CIRCUIT_SOLVER_H

#include "circuit.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <memory>
#include <map>
#include <Eigen/Dense>

struct ImpedanceMeasurement {
    std::vector<double> frequencies;
    std::vector<double> magnitude_kohm;
    std::vector<std::complex<double>> impedance;
    
    void printSummary() const {
        if (frequencies.empty()) return;
        
        std::cout << "Input Impedance Summary:" << std::endl;
        
        // Calcola media geometrica (più significativa per scale logaritmiche)
        double log_sum = 0.0;
        for (double z : magnitude_kohm) {
            log_sum += std::log10(z);
        }
        double geometric_mean = std::pow(10.0, log_sum / magnitude_kohm.size());
        
        // Trova min e max
        auto min_it = std::min_element(magnitude_kohm.begin(), magnitude_kohm.end());
        auto max_it = std::max_element(magnitude_kohm.begin(), magnitude_kohm.end());
        size_t min_idx = std::distance(magnitude_kohm.begin(), min_it);
        size_t max_idx = std::distance(magnitude_kohm.begin(), max_it);
        
        std::cout << "   Average: " << std::fixed << std::setprecision(2) 
                  << geometric_mean << " kΩ (geometric mean)" << std::endl;
        std::cout << "   Minimum: " << *min_it << " kΩ @ " 
                  << std::setprecision(1) << frequencies[min_idx] << " Hz" << std::endl;
        std::cout << "   Maximum: " << std::setprecision(2) << *max_it << " kΩ @ " 
                  << std::setprecision(1) << frequencies[max_idx] << " Hz" << std::endl;
        std::cout << std::endl;
    }
    
    void printByBands() const {
        if (frequencies.empty()) return;
        
        std::cout << "Input Impedance by Frequency Bands:" << std::endl;
        
        // Definisci bande audio comuni
        struct Band {
            std::string name;
            double f_min;
            double f_max;
        };
        
        std::vector<Band> bands = {
            {"Sub-bass",     20,    60},
            {"Bass",         60,   250},
            {"Low-mid",     250,   500},
            {"Mid",         500,  2000},
            {"High-mid",   2000,  4000},
            {"Presence",   4000,  6000},
            {"Brilliance", 6000, 20000}
        };
        
        for (const auto& band : bands) {
            double sum = 0.0;
            int count = 0;
            double min_z = 1e12;
            double max_z = 0.0;
            
            for (size_t i = 0; i < frequencies.size(); i++) {
                if (frequencies[i] >= band.f_min && frequencies[i] <= band.f_max) {
                    sum += magnitude_kohm[i];
                    min_z = std::min(min_z, magnitude_kohm[i]);
                    max_z = std::max(max_z, magnitude_kohm[i]);
                    count++;
                }
            }
            
            if (count > 0) {
                double avg = sum / count;
                double variation = ((max_z - min_z) / avg) * 100.0;
                
                std::cout << "   " << std::setw(12) << std::left << band.name 
                          << std::right << std::fixed << std::setprecision(2)
                          << " (" << std::setw(5) << band.f_min << "-" 
                          << std::setw(6) << band.f_max << " Hz): "
                          << std::setw(6) << avg << " kΩ";
                
                // Indica se varia molto
                if (variation > 20.0) {
                    std::cout << "  [varies " << std::setprecision(0) << variation << "%]";
                }
                std::cout << std::endl;
            }
        }
        std::cout << std::endl;
    }
};

class CircuitSolver {

private:
    Circuit& circuit;
    Eigen::MatrixXd G;
    Eigen::VectorXd I, V, V_new;
    
    double input_voltage;

    Eigen::PartialPivLU<Eigen::MatrixXd> lu_solver;
    
    uint64_t sample_count = 0;
    uint64_t failed_count = 0;
    uint64_t iteration_count = 0;
    
    double dt;
    int max_iterations;
    double tolerance_sq;
    double source_g;
    int max_non_convergence_warning;
    std::ofstream logFile;
    bool logging_enabled = false;
    
    void warmUp(double warmup_duration) {
        std::cout << "Circuit WarmUp" << std::endl;
        int warmup_samples = static_cast<int>(warmup_duration / dt);
        input_voltage = 0.0;        
        for (int i = 0; i < warmup_samples; i++) {
            solve();
        }
        std::cout << "   Circuit stabilized after " << (warmup_samples * dt * 1000) << " ms" << std::endl;
        std::cout << std::endl;
        sample_count = 0;
        failed_count = 0;
        iteration_count = 0;
    }
    
public:
    CircuitSolver(Circuit& ckt, double sample_rate, int source_impedance, int max_iterations, double tolerance, int max_non_convergence_warning = 50) 
        : circuit(ckt), 
          dt(1.0 / sample_rate),
          source_g(1.0 / source_impedance),
          max_iterations(max_iterations),
          tolerance_sq(tolerance*tolerance),
          max_non_convergence_warning(max_non_convergence_warning) {
        
        G.resize(circuit.num_nodes, circuit.num_nodes);
        I.resize(circuit.num_nodes);
        V.resize(circuit.num_nodes);
              V_new.resize(circuit.num_nodes);
        V.setZero();
    }
    
    ~CircuitSolver() {
        if (circuit.hasProbes()) {
            closeProbeFile();
        }
    }

    bool initialize() {
        if (circuit.hasInitialConditions()) {
            circuit.applyInitialConditions();
        }
        if (circuit.hasWarmUp()) {
            warmUp(circuit.warmup_duration);
        }
        if (circuit.hasProbes()) {
            openProbeFile();
        }
        return true;
    }
    
    bool solveDC() {
        for (int iter = 0; iter < max_iterations; iter++) {
            G.setZero();
            I.setZero();
            
            for (auto& comp : circuit.components) {
                // Passo dt=0 per indicare analisi DC
                comp->stamp(G, I, V, 0.0);
            }

            // Nodo di massa forzato
            G.row(0).setZero();
            G.col(0).setZero();
            G(0, 0) = 1.0;
            I(0) = 0.0;

            V_new = G.lu().solve(I);
            
            double error_sq = (V_new - V).squaredNorm();
            V = V_new;
            if (error_sq < tolerance_sq) {
                return true;
            }
        }
        return false;
    }

    void openProbeFile() {
        std::string filename = circuit.getProbeFile();
        logFile.open(filename);
        if (!logFile.is_open()) {
            throw std::runtime_error("Cannot open probe file: " + filename);
        }
        
        logging_enabled = true;
        
        logFile << "time";
        for (auto& p : circuit.probes) {
            if (p.type == ProbeTarget::Type::VOLTAGE) {
                logFile << ";V(" << p.name << ")";
            } else if (p.type == ProbeTarget::Type::CURRENT) {
                logFile << ";I(" << p.name << ")";
            }
        }
        logFile << "\n";
        
        std::cout << "Probe file opened: " << filename << std::endl;
    }
    
    void logProbes() {
        if (!logging_enabled) return;
        double time = sample_count * dt;
        logFile << std::fixed << std::setprecision(9) << time;
        
        for (auto& p : circuit.probes) {
            if (p.type == ProbeTarget::Type::VOLTAGE) {
                if (p.name == "input") {
                    logFile << ";" << input_voltage;    
                } else {
                    int node = std::stoi(p.name);
                    if (node < circuit.num_nodes) {
                        logFile << ";" << V(node);
                    } else {
                        logFile << ";NaN";
                    }
                }
            } else if (p.type == ProbeTarget::Type::CURRENT) {
                double current = 0.0;
                bool found = false;
                for (auto& comp : circuit.components) {
                    if (comp->name == p.name) {
                        current = comp->getCurrent();
                        found = true;
                        break;
                    }
                }
                if (found) {
                    logFile << ";" << current;
                } else {
                    logFile << ";NaN";
                }
            }
        }
        
        logFile << "\n";
    }
    
    void closeProbeFile() {
        if (logging_enabled && logFile.is_open()) {
            logFile.close();
            logging_enabled = false;
            std::cout << "Probe file closed." << std::endl;
        }
    }
    
    bool solve() {
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
                logProbes();
                iteration_count += iter + 1;
                return true;
            }
        }
        logProbes();
        failed_count++;
        iteration_count += max_iterations;
        return false;
    }
    
    ImpedanceMeasurement measureInputImpedance(double f_start = 20.0, double f_end = 20000.0, double duration = 2.0) {
        ImpedanceMeasurement result;
    
    // Salva stato originale
    Eigen::VectorXd V_backup = V;
    uint64_t sample_count_backup = sample_count;
    uint64_t failed_count_backup = failed_count;
    uint64_t iteration_count_backup = iteration_count;
    
    std::cout << "AC Analysis - Input Impedance Measurement" << std::endl;
    std::cout << "   Frequency range: " << f_start << " Hz - " << f_end << " Hz" << std::endl;
    std::cout << "   Duration per frequency: " << (duration * 1000) << " ms" << std::endl;
    std::cout << std::endl;
    
    // Calcola frequenze in scala logaritmica
    int num_decades = static_cast<int>(std::ceil(std::log10(f_end / f_start)));
    int total_points = num_decades * 50;  // 50 punti per decade come SPICE
    
    int num_samples = static_cast<int>(duration / dt);
    int skip_samples = num_samples / 2;  // Salta transitori
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < total_points; i++) {
        // Frequenza logaritmica
        double log_f = std::log10(f_start) + (std::log10(f_end) - std::log10(f_start)) * i / (total_points - 1);
        double freq = std::pow(10.0, log_f);
        double omega = 2.0 * M_PI * freq;
        
        // Reset circuito per ogni frequenza
        V.setZero();
        circuit.reset();
        sample_count = 0;
        failed_count = 0;
        iteration_count = 0;
        
        // Variabili per misura corrente
        double current_real_sum = 0.0;
        double current_imag_sum = 0.0;
        int valid_samples = 0;
        
        // Simula per la durata specificata
        for (int s = 0; s < num_samples; s++) {
            double t = s * dt;
            double v_ac = std::sin(omega * t);  // 1V amplitude AC
            setInputVoltage(v_ac);
            
            solve();
            
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
        std::complex<double> Z_in = (I_magnitude > 1e-12) ? 
            std::complex<double>(1.0, 0.0) / I_avg : 
            std::complex<double>(1e12, 0.0);  // Impedenza molto alta se corrente trascurabile
        
        double Z_magnitude = std::abs(Z_in);
        double Z_phase = std::arg(Z_in) * 180.0 / M_PI;  // Fase in gradi
        
        result.frequencies.push_back(freq);
        result.magnitude_kohm.push_back(Z_magnitude / 1000.0);  // Ω -> kΩ
        result.impedance.push_back(Z_in);
        
        if (i % 10 == 0 || i == total_points - 1) {
            std::cout << "   " << std::fixed << std::setprecision(1) 
                      << freq << " Hz: " 
                      << std::setprecision(2) << (Z_magnitude / 1000.0) << " kΩ, "
                      << std::setprecision(1) << Z_phase << "°" << std::endl;
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    
    std::cout << std::endl;
    std::cout << "AC Analysis Statistics:" << std::endl;
    std::cout << "   Total Points: " << total_points << std::endl;
    std::cout << "   Execution Time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms" << std::endl;
    std::cout << "   Samples per frequency: " << num_samples << std::endl;
    std::cout << std::endl;
    
    // Ripristina stato originale
    V = V_backup;
    sample_count = sample_count_backup;
    failed_count = failed_count_backup;
    iteration_count = iteration_count_backup;
    
    return result;
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
    
    double getOutputVoltage() const {
        return V(circuit.output_node);
    }
    
    void setInputVoltage(double vin) {
        input_voltage = vin;
    }

    void printDCOperatingPoint() {
        for (int i = 0; i < circuit.num_nodes; i++) {
            std::cout << "   Node " << i << ": " << V(i) << " V" << std::endl;
        }
        std::cout << std::endl;
    }

    void reset() {
        V.setZero();
        circuit.reset();
    }
    
};

#endif