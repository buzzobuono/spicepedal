#ifndef DC_CIRCUIT_SOLVER_H
#define DC_CIRCUIT_SOLVER_H

#include "circuit.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <memory>
#include <map>
#include <Eigen/Dense>

class DCCircuitSolver {

private:
    Circuit& circuit;
    Eigen::MatrixXd G;
    Eigen::VectorXd I, V, V_new;
    
    double input_voltage;

    Eigen::PartialPivLU<Eigen::MatrixXd> lu_solver;
    
    uint64_t sample_count = 0;
    uint64_t failed_count = 0;
    uint64_t iteration_count = 0;
    
    int max_iterations;
    double tolerance_sq;
    
public:
    DCCircuitSolver(Circuit& ckt, int max_iterations, double tolerance) 
        : circuit(ckt), 
          max_iterations(max_iterations),
          tolerance_sq(tolerance*tolerance) {
        
        G.resize(circuit.num_nodes, circuit.num_nodes);
        I.resize(circuit.num_nodes);
        V.resize(circuit.num_nodes);
        V_new.resize(circuit.num_nodes);
        V.setZero();
    }
    
    ~DCCircuitSolver() {
    }

    bool initialize() {
        return true;
    }
    
    bool solve() {
        sample_count++;
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
                iteration_count += iter + 1;
                return true;
            }
        }
        failed_count++;
        iteration_count += max_iterations;
        return false;
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
    
    void printDCOperatingPoint() {
        for (int i = 0; i < circuit.num_nodes; i++) {
            std::cout << "   Node " << i << ": " << V(i) << " V" << std::endl;
        }
        std::cout << std::endl;
    }

    void setInputVoltage(double vin) {
        input_voltage = vin;
    }

    void reset() {
        V.setZero();
        circuit.reset();
    }
    
};

#endif