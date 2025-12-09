#ifndef DC_SOLVER_H
#define DC_SOLVER_H

#include "solvers/newton_raphson_solver.h" // Includi la classe base intermedia
#include "circuit.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <memory>
#include <map>
#include <Eigen/Dense>

class DCSolver : public NewtonRaphsonSolver {
    
    public:
    
    DCSolver(Circuit& circuit, int max_iterations, double tolerance)
        : NewtonRaphsonSolver(circuit, 1.0, 1, max_iterations, tolerance) 
    {
        
    }
    
    ~DCSolver() override = default;
    
    bool solve() override {
        return runNewtonRaphson(0.0);
    }
    
    protected:
    
    void stampComponents(double dt) override {
        NewtonRaphsonSolver::stampComponents(0.0);
    }
    
    void applySource(double dt) override {
        (void)dt;
    }
    
    void updateComponentsHistory(double dt) override {
        (void)dt;
    }
    
    public:

    void setInputVoltage(double vin) {
        this->input_voltage = vin;
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
};

#endif
