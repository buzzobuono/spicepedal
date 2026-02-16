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
    double total_history_time_ns = 0;
    long calls_history = 0;
    double total_timestep_time_ns = 0;
    long calls_timestep = 0;
};
#endif

class NewtonRaphsonSolver : public Solver {

    protected:
    
    struct FastEntry {
        double* address;
        double value;
    };
    
    std::vector<FastEntry> fast_G_entries;
    std::vector<FastEntry> fast_I_entries;
    std::tuple<
        std::vector<BJT*>,
        std::vector<Capacitor*>,
        std::vector<Potentiometer*>,
        std::vector<Component*>
    > dynamic_lists;
    
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
        for (const auto& entry : fast_G_entries) {
            *(entry.address) += entry.value;
        }
        for (const auto& entry : fast_I_entries) {
            *(entry.address) += entry.value;
        }
        std::apply([&](auto&... args) {
            (..., [&](auto& vec) {
                #ifdef DEBUG_MODE
                if (vec.empty()) return;
                auto start = std::chrono::high_resolution_clock::now();
                #endif
                for (auto* comp : vec) {
                    comp->stamp(G, I, V);
                }
                #ifdef DEBUG_MODE
                auto end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
                using ElemType = std::remove_pointer_t<typename std::decay_t<decltype(vec)>::value_type>;
                ComponentType key;
                if constexpr (std::is_same_v<ElemType, Component>)
                    key = ComponentType::GENERIC;
                else
                    key = vec.front()->type;
                auto& stats = stamp_stats[key];
                stats.total_time_ns += duration;
                stats.calls += vec.size();
                #endif
            }(args));
        }, dynamic_lists);
    }

    virtual void applySource() {
        if (circuit.input_node > 0) {
            G(circuit.input_node, circuit.input_node) += source_g;
            I(circuit.input_node) += input_voltage * source_g;
        }
    }
    
    virtual void updateComponentsHistory() {
        std::apply([&](auto&... args) {
            (..., [&](auto& vec) {
                #ifdef DEBUG_MODE
                if (vec.empty()) return;
                auto start = std::chrono::high_resolution_clock::now();
                #endif
                for (auto* comp : vec) {
                    comp->updateHistory(V);
                }
                #ifdef DEBUG_MODE
                auto end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
                using ElemType = std::remove_pointer_t<typename std::decay_t<decltype(vec)>::value_type>;
                ComponentType key;
                if constexpr (std::is_same_v<ElemType, Component>)
                    key = ComponentType::GENERIC;
                else
                    key = vec.front()->type;
                auto& stats = stamp_stats[key];
                stats.total_history_time_ns += duration;
                stats.calls_history += vec.size();
                #endif
            }(args));
        }, dynamic_lists);
    }
    
    void prepareTimeStep() {
        std::apply([&](auto&... args) {
            (..., [&](auto& vec) {
                #ifdef DEBUG_MODE
                if (vec.empty()) return;
                auto start = std::chrono::high_resolution_clock::now();
                #endif
                for (auto* comp : vec) {
                    comp->prepareTimeStep();
                }
                #ifdef DEBUG_MODE
                auto end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
                using ElemType = std::remove_pointer_t<typename std::decay_t<decltype(vec)>::value_type>;
                ComponentType key;
                if constexpr (std::is_same_v<ElemType, Component>)
                    key = ComponentType::GENERIC;
                else
                    key = vec.front()->type;
                auto& stats = stamp_stats[key];
                stats.total_timestep_time_ns += duration;
                stats.calls_timestep += vec.size();
                #endif
            }(args));
        }, dynamic_lists);
    }
    
    bool runNewtonRaphson() {
        this->sample_count++;
        
        this->prepareTimeStep();
        
        for (int iter = 0; iter < max_iterations; iter++) {
            G.setZero();
            I.setZero();
            
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
        
        fast_G_entries.clear();
        fast_I_entries.clear();
        
        std::apply([](auto&... vec) { (vec.clear(), ...); }, dynamic_lists);
        
        for (auto& comp : circuit.components) {
            G.setZero();
            I.setZero();
            comp->prepare(G, I, V, dt);
            comp->stampStatic(G, I);
            
            for (int r = 0; r < circuit.num_nodes; ++r) {
                for (int c = 0; c < circuit.num_nodes; ++c) {
                    if (G(r, c) != 0.0) 
                        fast_G_entries.push_back({&G(r, c), G(r, c)});
                }
                if (I(r) != 0.0) 
                    fast_I_entries.push_back({&I(r), I(r)});
            }
            
            if (!comp->is_static) {
                if (comp->type == ComponentType::BJT) {
                    std::get<std::vector<BJT*>>(dynamic_lists).push_back(static_cast<BJT*>(comp.get()));
                } else if (comp->type == ComponentType::CAPACITOR) {
                    std::get<std::vector<Capacitor*>>(dynamic_lists).push_back(static_cast<Capacitor*>(comp.get()));
                } else if (comp->type == ComponentType::POTENTIOMETER) {
                    std::get<std::vector<Potentiometer*>>(dynamic_lists).push_back(static_cast<Potentiometer*>(comp.get()));
                } else {
                    std::get<std::vector<Component*>>(dynamic_lists).push_back(comp.get());
                }
            }
        }
        
        auto sortByAddr = [](const FastEntry& a, const FastEntry& b) { return a.address < b.address; };
        std::sort(fast_G_entries.begin(), fast_G_entries.end(), sortByAddr);
        std::sort(fast_I_entries.begin(), fast_I_entries.end(), sortByAddr);
        
        #ifdef DEBUG_MODE
        auto& generic = std::get<std::vector<Component*>>(dynamic_lists);
        for (auto* comp : generic) {
            std::cout << "GENERIC: " << comp->name 
            << " type=" << static_cast<int>(comp->type) << std::endl;
        }
        #endif
        
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
        Solver::printProcessStatistics();
        std::cout << "[DEBUG] Component Profiling" << std::endl;
        double total_stamp_time_ms = 0;
        for (auto const& [type, data] : stamp_stats) {
            double total_timestep_ms = data.total_timestep_time_ns / 1e6;
            double avg_timestep_ns = (data.calls_timestep > 0) ? (static_cast<double>(data.total_timestep_time_ns) / data.calls_timestep) : 0;
            
            double total_ms = data.total_time_ns / 1e6;
            double avg_ns = (data.calls > 0) ? (static_cast<double>(data.total_time_ns) / data.calls) : 0;
            
            double total_history_ms = data.total_history_time_ns / 1e6;
            double avg_history_ns = (data.calls_history > 0) ? (static_cast<double>(data.total_history_time_ns) / data.calls_history) : 0;
            
            total_stamp_time_ms += total_ms;
            std::cout << "Prepare Time Step [Type ID: " << std::setw(2) << static_cast<int>(type) << "]" 
                      << " Total: " << std::fixed << std::setprecision(2) << std::setw(8) << total_timestep_ms << " ms"
                      << " | Avg: " << std::setw(8) << std::setprecision(1) << avg_timestep_ns << " ns/call" 
                      << " (" << data.calls_timestep << " calls)" << std::endl;
            
            std::cout << "Stamping [Type ID: " << std::setw(2) << static_cast<int>(type) << "]" 
                      << " Total: " << std::fixed << std::setprecision(2) << std::setw(8) << total_ms << " ms"
                      << " | Avg: " << std::setw(8) << std::setprecision(1) << avg_ns << " ns/call" 
                      << " (" << data.calls << " calls)" << std::endl;
            
            std::cout << " Update History [Type ID: " << std::setw(2) << static_cast<int>(type) << "]" 
                      << " Total: " << std::fixed << std::setprecision(2) << std::setw(8) << total_history_ms << " ms"
                      << " | Avg: " << std::setw(8) << std::setprecision(1) << avg_history_ns << " ns/call" 
                      << " (" << data.calls_history << " calls)" << std::endl;
        }
        std::cout << "  ----------------------------------------" << std::endl;
        std::cout << "  Total Stamping Time: " << total_stamp_time_ms << " ms" << std::endl;
        std::cout << "  Total LU Solve Time: " << lu_total_time_ns / 1e6 << " ms" << std::endl;
        std::cout << std::endl;
    }
    #endif
    
};

#endif
