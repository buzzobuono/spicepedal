#ifndef COMPONENT_H
#define COMPONENT_H

#include <string>

#include "utils/math.h"

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
    const double R_MIN = 1e-12;
    const double R_MAX = 1e12;
    const double G_MIN_STABILITY = 1e-12;
   
    ParameterRegistry* params = nullptr;

    public:
    
    ComponentType type;
    
    bool is_static = false;
    
    std::string name;
    
    virtual ~Component() = default;
    
    void setParams(ParameterRegistry* pr) {
        params = pr;
    }
    
    virtual void prepare(Matrix& G, Vector& I, Vector& V, double dt) {};

    virtual void stampStatic(Matrix& G, Vector& I) {};
    
    virtual void stamp(Matrix& G, Vector& I, const Vector& V) {};
    
    virtual void updateHistory(const Vector& V) {};
    
    virtual double getCurrent(const Vector& V) const { return 0.0; };
    
    virtual void reset() {}
    
};

#endif
