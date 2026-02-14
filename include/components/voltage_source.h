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
    double g, g_neg;
    double Ieq, Ieq_neg;
    
    VoltageSource(const std::string& nm, int np, int nn, double v, double Rs) {
        type = ComponentType::VOLTAGE_SOURCE;
        category = ComponentCategory::LINEAR_STATIC;

        name = nm;
        n1 = np;
        n2 = nn;
        
        g = 1.0 / Rs;
        g_neg = -g;
        Ieq = v * g;
        Ieq_neg = -Ieq;
        
    }
    
    std::vector<StaticGStampHook> getStaticGStamps() override {
        return {
            {n1, n1, g}, 
            {n1, n2, g_neg},
            {n2, n1, g_neg},
            {n2, n2, g}
        };
    }
    
    std::vector<StaticIStampHook> getStaticIStamps() override {
        return {
            {n1, Ieq},
            {n2, Ieq_neg}
        };
    }
    
};

#endif