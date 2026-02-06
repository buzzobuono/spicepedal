#ifndef VOLTAGE_H
#define VOLTAGE_H

#include <string>
#include <algorithm>
#include <stdexcept>

#include "component.h"

class VoltageSource : public Component {
    
public:

    double voltage;
    
    double rs; 

    VoltageSource(const std::string& nm, int np, int nn, double v, double Rs) {
        type = ComponentType::VOLTAGE_SOURCE;
        name = nm;
        nodes = {np, nn};
        voltage = v;
        rs = Rs;
    }
    
    void stamp(Matrix& G, Vector& I, const Vector& V, double dt) override {
        double g = 1.0 / rs;
        int n1 = nodes[0], n2 = nodes[1];
        
        if (n1 != 0) {
            G(n1, n1) += g;
            if (n2 != 0) G(n1, n2) -= g;
            I(n1) += voltage * g;
        }
        
        if (n2 != 0) {
            G(n2, n2) += g;
            if (n1 != 0) G(n2, n1) -= g;
            I(n2) -= voltage * g;
        }
    }
};

#endif