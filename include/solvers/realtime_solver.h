#ifndef RT_SOLVER_H
#define RT_SOLVER_H

#include "solvers/newton_raphson_solver.h"
#include "circuit.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <memory>
#include <map>

class RealTimeSolver : public NewtonRaphsonSolver {    

    public:
    
    RealTimeSolver(Circuit& circuit, double dt, int max_iterations, double tolerance)
        : NewtonRaphsonSolver(circuit, dt, max_iterations, tolerance)
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

    bool solve() {
        return solveImpl();
    }

    bool solveImpl() override {
        if (runNewtonRaphson()) {
            updateComponentsHistory();
            return true;
        }
        return false;
    }
    
    void printResult() override {
        
    }
    
};

#endif

