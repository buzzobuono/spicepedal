#ifndef WIRE_H
#define WIRE_H

#include <string>
#include <stdexcept>

#include "component.h"
#include "components/resistor.h"

class Wire : public Resistor {
    public:
    Wire(const std::string& comp_name, int node1, int node2) :
         Resistor(comp_name, node1, node2, 1e-3)
    {
        type = ComponentType::WIRE;
        category = ComponentCategory::LINEAR_STATIC;
    }
};

#endif