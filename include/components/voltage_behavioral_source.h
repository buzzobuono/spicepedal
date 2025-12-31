#ifndef VOLTAGE_BEHAVIORAL_SOURCE_H
#define VOLTAGE_BEHAVIORAL_SOURCE_H

#include <string>
#include <algorithm>
#include <Eigen/Dense>
#include "component.h"
#include "external/exprtk.hpp"

class VoltageBehavioralSource : public Component {
private:
    int n_p, n_m;
    double Rout;
    std::string expression_string;
    mutable double v_target;
    
    exprtk::symbol_table<double> symbol_table;
    exprtk::expression<double> expression;
    exprtk::parser<double> parser;

    mutable std::vector<double> v_buffer;
    mutable double v_prev = 0.0;
    mutable double dt_internal = 0.0;
    mutable double time_internal = 0.0;
    
    bool is_initialized = false;
    
public:
    VoltageBehavioralSource(const std::string& comp_name, int p, int m, 
                     const std::string& expr_string, double rout)
        : n_p(p), n_m(m), expression_string(expr_string), Rout(rout)
    {
        name = comp_name;
        type = ComponentType::VOLTAGE_BEHAVIORAL_SOURCE;
        nodes = {n_p, n_m};
        
    }

    void stamp(Eigen::MatrixXd& G, Eigen::VectorXd& I,
               const Eigen::VectorXd& V, double dt) override
    {
        
        if (!is_initialized) {
            v_buffer.resize(V.size(), 0.0);
            
            for (int i = 0; i < V.size(); ++i) {
                symbol_table.add_variable("V" + std::to_string(i), v_buffer[i]);
            }
            symbol_table.add_variable("dt", dt_internal);
            symbol_table.add_variable("t", time_internal);
            symbol_table.add_variable("Vprev", v_prev);
            
            expression.register_symbol_table(symbol_table);
            if (!parser.compile(expression_string, expression)) {
                throw std::runtime_error("VoltageBehavioralSource: expression syntax error");
            }
            is_initialized = true;
        }
        dt_internal = dt;
        for (int i = 0; i < V.size(); ++i) {
            v_buffer[i] = V(i);
        }
        v_target = expression.value();
        
        double g_out = 1.0 / Rout;
        double i_norton = v_target * g_out;

        if (n_p != 0) { 
            G(n_p, n_p) += g_out;
            I(n_p) += i_norton;
        }
        if (n_m != 0) {
            G(n_m, n_m) += g_out; 
            I(n_m) -= i_norton;
        }
        
        if (n_p != 0 && n_m != 0) {
            G(n_p, n_m) -= g_out;
            G(n_m, n_p) -= g_out;
        }
    }

    void updateHistory(const Eigen::VectorXd& V, double dt) override {
        time_internal += dt; 
        v_prev = v_target;
    }
};

#endif

