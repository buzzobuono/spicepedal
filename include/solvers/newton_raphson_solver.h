#ifndef NR_SOLVER_H
#define NR_SOLVER_H

#include "solvers/solver.h"
#include "circuit.h"
#include <Eigen/Dense>

class NewtonRaphsonSolver : public Solver {

    protected:
    
    Circuit& circuit;
    Eigen::MatrixXd G;
    Eigen::VectorXd I;
    Eigen::VectorXd V, V_new;
    Eigen::PartialPivLU<Eigen::MatrixXd> lu_solver;
        
    double input_voltage;
    double source_g;
    int max_iterations;
    double tolerance_sq;
    double dt;
        
    virtual void stampComponents(double dt) {
        for (auto& comp : circuit.components) {
            comp->stamp(G, I, V, dt);
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
            
            lu_solver.compute(G);
            V_new = lu_solver.solve(I);
            
            double error_sq = (V_new - V).squaredNorm();
            V = V_new;
            
            if (error_sq < tolerance_sq) {
                this->updateComponentsHistory(dt);
                this->iteration_count += iter + 1;
                return true;
            }
        }
        
        this->failed_count++;
        this->iteration_count += max_iterations;
        return false;
    }
    
    public:
    
    NewtonRaphsonSolver(Circuit& circuit, double sample_rate, int source_impedance, int max_iterations, double tolerance) 
        : circuit(circuit), 
          dt(1.0 / sample_rate),
          source_g(1.0 / source_impedance),
          max_iterations(max_iterations),
          tolerance_sq(tolerance*tolerance) {
                  
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
    
    bool reset() override {
        V.setZero();
        G.setZero();
        I.setZero();
        
        circuit.reset();
        this->initCounters();
        return true;
    }
    
};

#endif
