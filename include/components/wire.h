#ifndef WIRE_H
#define WIRE_H

#include <string>
#include <stdexcept>

#include "component.h"
#include "components/resistor.h"

class Wire : public Component {
    private:
    int n1, n2;
    
    public:
    Wire(const std::string& comp_name, int node1, int node2) {
        if (node1 == node2) {
            throw std::runtime_error("Wire nodes must be different");
        }
        type = ComponentType::WIRE;
        name = comp_name;
        n1 = node1;
        n2 = node2;
        is_static = true;
    }
    
    void stampStatic(Matrix& G, Vector& I) override {
        Resistor r(name , n1, n2, 1e-3);
        r.stampStatic(G, I);
    }
};

#endif