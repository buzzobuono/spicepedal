#ifndef RESISTOR_H
#define RESISTOR_H

#include <string>
#include <algorithm>
#include <stdexcept>

#include "component.h"

class Resistor : public Component {
private:
    double _r;
    int n1, n2;
    
public:
    Resistor(const std::string& comp_name, int node1, int node2, double r) {
        if (r <= 0) throw std::runtime_error("Resistance must be positive");
        if (node1 == node2) throw std::runtime_error("Resistor nodes must be different");

        type = ComponentType::RESISTOR;
        name = comp_name;
        n1 = node1;
        n2 = node2;
        _r = r;
        is_static = true;
    }
    
    void stampStatic(Matrix& G, Vector& I) override {
        if (_r > R_MAX) return;
        double g = 1.0 / std::max(_r, R_MIN);
        G(n1, n1) += g;
        G(n1, n2) -= g;
        G(n2, n2) += g;
        G(n2, n1) -= g;
    }
    
    double getCurrent(const Vector& V) const override {
        double v1 = V(n1);
        double v2 = V(n2);
        // Legge di Ohm: I = V/R
        return (v1 - v2) / std::max(_r, R_MIN);
    }
    
};

#endif