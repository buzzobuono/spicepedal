#ifndef COMPONENT_H
#define COMPONENT_H

#include <string>
#include <vector>
#include <Eigen/Dense>

enum class ComponentType {
    RESISTOR, 
    CAPACITOR, 
    INDUCTOR,
    DIODE, 
    BJT, 
    VOLTAGE_SOURCE,
    POTENTIOMETER,
    WIRE,
    OPAMP,
    VCVS,
    BEHAVIORAL_VOLTAGE_SOURCE,
    PARAMETER_EVALUATOR,
    SUBCIRCUIT
};

class Component {
    
    protected:
    
    ParameterRegistry* params = nullptr;

    public:
    
    ComponentType type;
    std::string name;
    std::vector<int> nodes;
    
    virtual ~Component() = default;
    
    void setParams(ParameterRegistry* pr) {
        params = pr;
    }
    
    virtual void stamp(Eigen::MatrixXd& G, Eigen::VectorXd& I, 
                      const Eigen::VectorXd& V, double dt) = 0;
    
    virtual void updateHistory(const Eigen::VectorXd& V, double dt) {}
    
    virtual void reset() {}
    
    virtual double getCurrent() const { return 0.0; }
    
};

#endif
