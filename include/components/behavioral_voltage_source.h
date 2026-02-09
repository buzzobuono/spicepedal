#ifndef BEHAVIORAL_VOLTAGE_SOURCE_H
#define BEHAVIORAL_VOLTAGE_SOURCE_H

#include <string>
#include <algorithm>

#include "component.h"
#include "behavioral_component.h"
#include "external/exprtk.hpp"

class BehavioralVoltageSource : public BehavioralComponent {
private:
    int n_p, n_m;
    double Rout;
    double g_out;
    
public:
    BehavioralVoltageSource(const std::string& comp_name, int p, int m, 
                           const std::string& expr_string, double rout)
        : n_p(p), n_m(m), Rout(rout) {
            name = comp_name;
            expression_string = expr_string;
            type = ComponentType::BEHAVIORAL_VOLTAGE_SOURCE;
            nodes = {n_p, n_m};
            g_out = 1.0 / Rout;
    }

    void stampStatic(Matrix& G, Vector& I) override {
        if (n_p != 0) { 
            G(n_p, n_p) += g_out;
        }
        if (n_m != 0) {
            G(n_m, n_m) += g_out;
        }
        
        if (n_p != 0 && n_m != 0) {
            G(n_p, n_m) -= g_out;
            G(n_m, n_p) -= g_out;
        }

    }
   
    void stamp(Matrix& G, Vector& I, const Vector& V) override {
        sync_variables(V, dt);
        double v_target = expression.value();
        double i_norton = v_target * g_out;

        if (n_p != 0) {
            I(n_p) += i_norton;
        }
        if (n_m != 0) {
            I(n_m) -= i_norton;
        }
    }
    
};

#endif

