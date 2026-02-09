#ifndef DC_SOLVER_H
#define DC_SOLVER_H

#include "solvers/newton_raphson_solver.h" // Includi la classe base intermedia
#include "circuit.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <memory>
#include <map>

class DCSolver : public NewtonRaphsonSolver {
    
    public:
    
    DCSolver(Circuit& circuit, int max_iterations, double tolerance)
        : NewtonRaphsonSolver(circuit, 0.0, max_iterations, tolerance) 
    {
        
    }
    
    ~DCSolver() override = default;
    
    bool solveImpl() override {
        return runNewtonRaphson();
    }
    
    protected:
    
    void stampComponents() override {
        NewtonRaphsonSolver::stampComponents();
    }
    
    void applySource() override {
    }
    
    void updateComponentsHistory() override {
    }
    
    public:
    
    void printResult() override {
        std::cout << "DC Analysis" << std::endl;
        printDCOperatingPoints();
    }
};

#endif
