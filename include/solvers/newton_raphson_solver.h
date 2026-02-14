#ifndef NR_SOLVER_H
#define NR_SOLVER_H

#include "solvers/solver.h"
#include "circuit.h"
#include "utils/math.h"

#ifdef DEBUG_MODE
#include <chrono>
#include <map>
#include <typeindex>

struct ProfileData {
    double total_time_ns = 0;
    long calls = 0;
};
#endif

#include <variant>

class NewtonRaphsonSolver : public Solver {

    protected:
    
    struct StaticFastEntry {
        double* dest;
        double value;
    };
    
    struct FastEntry {
        double* dest;   // Indirizzo nella matrice G
        double* source; // Indirizzo nel componente
    };
    
    std::vector<StaticFastEntry> static_fast_G_entries;
    std::vector<StaticFastEntry> static_fast_I_entries;
    
    std::vector<FastEntry> fast_G_entries;
    std::vector<FastEntry> fast_I_entries;
    
    std::vector<Capacitor*> linear_time_variant_components;
    std::vector<BJT*> non_linear_components;
    std::vector<Component*> legacy_components; 
    std::vector<BJT*> bjt_components;
    
    #ifdef DEBUG_MODE
    std::map<ComponentType, ProfileData> stamp_stats;
    double lu_total_time_ns = 0;
    #endif
    
    Circuit& circuit;
    
    Matrix G;
    Vector I;
    Vector V, V_new;
    PartialPivLU lu_solver;
    
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
            if (runNewtonRaphson()) {
                this->updateComponentsHistory();
            } 
        }
        
        std::cout << "   Circuit stabilized after " << (warmup_samples * dt * 1000) << " ms" << std::endl;
        std::cout << std::endl;
        
        this->initCounters();
    }
    
    virtual void stampComponents() {
        for (const auto& entry : static_fast_G_entries) {
            *(entry.dest) += entry.value;
        }
        for (const auto& entry : static_fast_I_entries) {
            *(entry.dest) += entry.value;
        }
        for (const auto& entry : fast_G_entries) {
            *(entry.dest) += *(entry.source);
        }
        for (const auto& entry : fast_I_entries) {
            *(entry.dest) += *(entry.source);
        }
        
        for (auto* comp : legacy_components) {
            #ifdef DEBUG_MODE
            auto start = std::chrono::high_resolution_clock::now();
            #endif
            comp->stamp(G, I, V);
            #ifdef DEBUG_MODE
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            stamp_stats[comp->type].total_time_ns += duration;
            stamp_stats[comp->type].calls++;
            #endif
        }
    }

    virtual void applySource() {
        if (circuit.input_node > 0) {
            G(circuit.input_node, circuit.input_node) += source_g;
            I(circuit.input_node) += input_voltage * source_g;
        }
    }
    
    virtual void updateComponentsHistory() {
        for (auto& comp : circuit.components) {
            comp->updateHistory(V);
        }
    }
    
    bool runNewtonRaphson() {
        this->sample_count++;
        
        for (auto& comp : linear_time_variant_components) {
            #ifdef DEBUG_MODE
            auto start = std::chrono::high_resolution_clock::now();
            #endif
            comp->Capacitor::onTimeStep();
            #ifdef DEBUG_MODE
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            stamp_stats[comp->type].total_time_ns += duration;
            stamp_stats[comp->type].calls++;
            #endif
        }
        
        for (int iter = 0; iter < max_iterations; iter++) {
            G.setZero();
            I.setZero();
            
            for (auto& comp : non_linear_components) {
                #ifdef DEBUG_MODE
                auto start = std::chrono::high_resolution_clock::now();
                #endif
                comp->BJT::onIterationStep();
                #ifdef DEBUG_MODE
                auto end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
                stamp_stats[comp->type].total_time_ns += duration;
                stamp_stats[comp->type].calls++;
                #endif
            }
            
            this->stampComponents();
            
            this->applySource();
            
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
    
    NewtonRaphsonSolver(Circuit& circuit, double dt, int max_iterations, double tolerance) 
        : circuit(circuit),
          dt(dt),
          max_iterations(max_iterations),
          tolerance_sq(tolerance*tolerance)
    {
        source_g = 1.0 / circuit.source_impedance;
    }
    
    virtual ~NewtonRaphsonSolver() = default;
    
    bool initialize() override {
        G.resize(circuit.num_nodes, circuit.num_nodes);
        I.resize(circuit.num_nodes);
        V.resize(circuit.num_nodes);
        V_new.resize(circuit.num_nodes);
        V.setZero();
        
        for (auto& comp : circuit.components) {
            comp->prepare(G,I,V,dt);
        }
        
        fast_G_entries.clear();
        fast_I_entries.clear();
        linear_time_variant_components.clear();
        non_linear_components.clear();
        legacy_components.clear();
        for (auto& comp : circuit.components) {
            auto g_hooks = comp->getStaticGStamps();
            for (auto& g_hook : g_hooks) {
               static_fast_G_entries.push_back({
                    &G(g_hook.row, g_hook.col),
                    g_hook.value
               });
            }
            auto i_hooks = comp->getStaticIStamps();
            for (auto& i_hook : i_hooks) {
               static_fast_I_entries.push_back({
                    &I(i_hook.idx),
                    i_hook.value
               });
            }
            auto static_g_hooks = comp->getGStamps();
            for (auto& g_hook : static_g_hooks) {
               fast_G_entries.push_back({
                    &G(g_hook.row, g_hook.col),
                    g_hook.source
               });
            }
            auto static_i_hooks = comp->getIStamps();
            for (auto& i_hook : static_i_hooks) {
               fast_I_entries.push_back({
                    &I(i_hook.idx),
                    i_hook.source
               });
            }
            switch (comp->category) {
                case ComponentCategory::LINEAR_STATIC:
                break;
                
                case ComponentCategory::LINEAR_TIME_VARIANT:
                if (comp->type == ComponentType::CAPACITOR) {
                    linear_time_variant_components.push_back(static_cast<Capacitor*>(comp.get()));
                } else {
                    legacy_components.push_back(comp.get());
                }
                break;
                
                case ComponentCategory::NON_LINEAR:
                if (comp->type == ComponentType::BJT) {
                    non_linear_components.push_back(static_cast<BJT*>(comp.get()));
                } else {
                    legacy_components.push_back(comp.get());
                }
                break;
            }
        }
        
        auto sortByAddr = [](const FastEntry& a, const FastEntry& b) { return a.dest < b.dest; };
        std::sort(fast_G_entries.begin(), fast_G_entries.end(), sortByAddr);
        std::sort(fast_I_entries.begin(), fast_I_entries.end(), sortByAddr);
        
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
