#ifndef NR_SOLVER_H
#define NR_SOLVER_H

#include "solvers/solver.h"
#include "circuit.h"
#include "utils/math.h"

#include <Eigen/Dense>

#ifdef DEBUG_MODE
#include <chrono>
#include <map>
#include <typeindex>

struct ProfileData {
    double total_time_ns = 0;
    long calls = 0;
};
#endif

class NewtonRaphsonSolver : public Solver {

    protected:
    
    #ifdef DEBUG_MODE
    std::map<ComponentType, ProfileData> stamp_stats;
    double lu_total_time_ns = 0;
    #endif
    
    Circuit& circuit;
    
    Matrix G;
    Vector I;
    Vector V, V_new;
    Eigen::PartialPivLU<Matrix> lu_solver;
    
    double sample_rate;
    double input_voltage;
    double source_g;
    int max_iterations;
    double tolerance_sq;
    double dt;
    
    void warmUp(double warmup_duration) {
        std::cout << "Circuit WarmUp" << std::endl;
        
        int warmup_samples = static_cast<int>(warmup_duration / dt);
        
        this->input_voltage = 0.0; 
        
        for (int i = 0; i < warmup_samples; i++) {
            if (runNewtonRaphson(dt)) {
                this->updateComponentsHistory(dt);
            } 
        }
        
        std::cout << "   Circuit stabilized after " << (warmup_samples * dt * 1000) << " ms" << std::endl;
        std::cout << std::endl;
        
        this->initCounters();
    }
    
    virtual void stampComponents(double dt) {
        for (auto& comp : circuit.components) {
            #ifdef DEBUG_MODE
            auto start = std::chrono::high_resolution_clock::now();
            #endif
            
            comp->stamp(G, I, V, dt);
            
            #ifdef DEBUG_MODE
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            stamp_stats[comp->type].total_time_ns += duration;
            stamp_stats[comp->type].calls++;
            #endif
        }
    }
    
    virtual void applySource(double dt) {
        if (circuit.input_node > 0) {
            G(circuit.input_node, circuit.input_node) += source_g;
            I(circuit.input_node) += input_voltage * source_g;
        }
    }
    
    virtual void updateComponentsHistory(double dt) {
        for (auto& comp : circuit.components) {
            comp->updateHistory(V, dt);
        }
    }
        
    bool runNewtonRaphson(double dt) {
        this->sample_count++;
        
        for (int iter = 0; iter < max_iterations; iter++) {
            G.setZero();
            I.setZero();
            
            this->stampComponents(dt);
            
            this->applySource(dt);
            
            G.row(0).setZero();
            G.col(0).setZero();
            G(0, 0) = 1.0;
            I(0) = 0.0;
            
            #ifdef DEBUG_MODE
            auto start_lu = std::chrono::high_resolution_clock::now();
            #endif
            
            lu_solver.compute(G);
            V_new.noalias() = lu_solver.solve(I);
            
            #ifdef DEBUG_MODE
            auto end_lu = std::chrono::high_resolution_clock::now();
            lu_total_time_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(end_lu - start_lu).count();
            #endif
            
            double error_sq = (V_new - V).squaredNorm();
            V = V_new;
            
            if (error_sq < tolerance_sq) {
                this->iteration_count += iter + 1;
                return true;
            }
        }
        
        this->failed_count++;
        this->iteration_count += max_iterations;
        return false;
    }
    
    public:
    
    NewtonRaphsonSolver(Circuit& circuit, double sample_rate, int max_iterations, double tolerance) 
        : circuit(circuit), 
          sample_rate(sample_rate),
          dt(1.0 / sample_rate),
          max_iterations(max_iterations),
          tolerance_sq(tolerance*tolerance) {
              
              source_g = 1.0 / circuit.source_impedance;
    }
    
    virtual ~NewtonRaphsonSolver() = default;
    
    bool initialize() override {
        G.resize(circuit.num_nodes, circuit.num_nodes);
        I.resize(circuit.num_nodes);
        V.resize(circuit.num_nodes);
        V_new.resize(circuit.num_nodes);
        V.setZero();
        
        this->initCounters();
        circuit.reset();
        
        return true;
    }
    
    double getOutputVoltage() const {
        return V(circuit.output_node);
    }
    
    void setInputVoltage(double vin) {
        input_voltage = vin;
    }
    
    void printDCOperatingPoints() {
        for (int i = 0; i < V.size(); i++) {
            std::cout << "   Node " << i << ": " << V(i) << " V" << std::endl;
        }
        std::cout << std::endl;
    }

    virtual void printResult() = 0;
    
    bool reset() override {
        V.setZero();
        G.setZero();
        I.setZero();
        
        circuit.reset();
        this->initCounters();
        return true;
    }
    
    #ifdef DEBUG_MODE
    void printProcessStatistics() override {
        // Chiama la base per i dati generali
        Solver::printProcessStatistics();
        
        std::cout << "[DEBUG] Component Stamping Profiling" << std::endl;

        double total_stamp_time_ms = 0;
        
        // Iteriamo direttamente sulla mappa dei dati raccolti
        for (auto const& [type, data] : stamp_stats) {
            double total_ms = data.total_time_ns / 1e6;
            double avg_ns = (data.calls > 0) ? (static_cast<double>(data.total_time_ns) / data.calls) : 0;
            total_stamp_time_ms += total_ms;

            // Stampiamo l'ID intero dell'enum per massima semplicità
            std::cout << "  [Type ID: " << std::setw(2) << static_cast<int>(type) << "]" 
                      << " Total: " << std::fixed << std::setprecision(2) << std::setw(8) << total_ms << " ms"
                      << " | Avg: " << std::setw(8) << std::setprecision(1) << avg_ns << " ns/call" 
                      << " (" << data.calls << " calls)" << std::endl;
        }

        std::cout << "  ----------------------------------------" << std::endl;
        std::cout << "  Total Stamping Time: " << total_stamp_time_ms << " ms" << std::endl;
        std::cout << "  Total LU Solve Time: " << lu_total_time_ns / 1e6 << " ms" << std::endl;
        std::cout << std::endl;
    }
    #endif

    
};

#endif
