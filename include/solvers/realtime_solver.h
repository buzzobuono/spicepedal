#ifndef RT_SOLVER_H
#define RT_SOLVER_H

#include "solvers/newton_raphson_solver.h"
#include "circuit.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <memory>
#include <map>
#include <Eigen/Dense>

class RealTimeSolver : public NewtonRaphsonSolver {    

    public:
    
    RealTimeSolver(Circuit& circuit, double sample_rate, int source_impedance, int max_iterations, double tolerance)
        : NewtonRaphsonSolver(circuit, sample_rate, source_impedance, max_iterations, tolerance)
    {
        
    }
    
    ~RealTimeSolver() override = default;
    
    bool initialize() override {
        NewtonRaphsonSolver::initialize(); 
        
        if (circuit.hasInitialConditions()) {
            circuit.applyInitialConditions();
        }
        
        if (circuit.hasWarmUp()) {
            warmUp(circuit.warmup_duration);
        }

        return true;
    }

    bool solve() override {
        return runNewtonRaphson(dt);
    }
    
    void printDCOperatingPoint() {
        for (int i = 0; i < circuit.num_nodes; i++) {
            std::cout << "   Node " << i << ": " << V(i) << " V" << std::endl;
        }
        std::cout << std::endl;
    }
    
};

#endif

