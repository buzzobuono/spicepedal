#ifndef TRAN_SOLVER_H
#define TRAN_SOLVER_H

#include "solvers/newton_raphson_solver.h"
#include "circuit.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <memory>
#include <map>
#include <Eigen/Dense>

class TransientSolver : public NewtonRaphsonSolver {
    
    private:
    
    std::ofstream logFile;
    bool probe_enabled = false;
    

    public:
    
    TransientSolver(Circuit& circuit, double sample_rate, int source_impedance, int max_iterations, double tolerance)
        : NewtonRaphsonSolver(circuit, sample_rate, source_impedance, max_iterations, tolerance)
    {
        
    }
    
    ~TransientSolver() override {
        if (circuit.hasProbes()) {
            closeProbeFile();
        }
    }
    
    bool initialize() override {
        NewtonRaphsonSolver::initialize(); 
        
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

    bool solve() override {
        bool success = runNewtonRaphson(dt);
        
        logProbes(); 
        
        return success;
    }
    
    void openProbeFile() {
        std::string filename = circuit.getProbeFile();
        logFile.open(filename);
        if (!logFile.is_open()) {
            throw std::runtime_error("Cannot open probe file: " + filename);
        }
        
        probe_enabled = true;
        
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
        if (!probe_enabled) return;
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
        if (probe_enabled && logFile.is_open()) {
            logFile.close();
            probe_enabled = false;
            std::cout << "Probe file closed." << std::endl;
        }
    }
    
    void printDCOperatingPoint() {
        for (int i = 0; i < circuit.num_nodes; i++) {
            std::cout << "   Node " << i << ": " << V(i) << " V" << std::endl;
        }
        std::cout << std::endl;
    }
    
};

#endif

