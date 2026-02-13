#ifndef VOLTAGE_H
#define VOLTAGE_H

#include <string>
#include <algorithm>
#include <stdexcept>

#include "component.h"

class VoltageSource : public Component {
    
    private:
    int n1, n2;
    
    public:
    double g;
    double Ieq;
    
    VoltageSource(const std::string& nm, int np, int nn, double v, double Rs) {
        type = ComponentType::VOLTAGE_SOURCE;
        name = nm;
        n1 = np;
        n2 = nn;
        
        g = 1.0 / Rs;
        Ieq = v * g;
        
        is_static = true;
    }
    
    void stampStatic(Matrix& G, Vector& I) override {
        G(n1, n1) += g;
        G(n1, n2) -= g;
        I(n1) += Ieq;
        G(n2, n2) += g;
        G(n2, n1) -= g;
        I(n2) -= Ieq;
    }
    
};

#endif