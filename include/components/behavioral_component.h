#ifndef BEHAVIORAL_COMPONENT_H
#define BEHAVIORAL_COMPONENT_H

#include "component.h"
#include "external/exprtk.hpp"
#include <string>
#include <vector>
#include <regex>

class BehavioralComponent : public Component {
protected:
    std::string expression_string;
    exprtk::symbol_table<double> symbol_table;
    exprtk::expression<double> expression;
    exprtk::parser<double> parser;

    mutable std::vector<double> v_buffer;
    mutable std::vector<double> v_nodes_prev;
    mutable double dt_internal = 0.0;
    mutable double time_internal = 0.0;
    mutable std::map<std::string, double> params_prev_buffer;
    
    bool is_initialized = false;

    void init_exprtk(const Eigen::VectorXd& V) {
        v_buffer.resize(V.size(), 0.0);
        v_nodes_prev.resize(V.size(), 0.0);
        
        std::string processed_expr = expression_string;
        processed_expr = std::regex_replace(processed_expr, std::regex("V\\((\\d+)\\)"), "V_$1_");
        processed_expr = std::regex_replace(processed_expr, std::regex("Vprev\\((\\d+)\\)"), "Vprev_$1_");
        processed_expr = std::regex_replace(processed_expr, std::regex("prev\\((\\w+)\\)"), "prev_$1_");
        
        for (int i = 0; i < V.size(); ++i) {
            symbol_table.add_variable("V_" + std::to_string(i) + "_", v_buffer[i]);
            symbol_table.add_variable("Vprev_" + std::to_string(i) + "_", v_nodes_prev[i]);
        }
        
        symbol_table.add_variable("dt", dt_internal);
        symbol_table.add_variable("t", time_internal);

        if (params) {
            for (auto const& [name, value] : params->getAll()) {
                symbol_table.add_variable(name, *params->getPtr(name));
                params_prev_buffer[name] = value;
                symbol_table.add_variable("prev_" + name + "_", params_prev_buffer[name]);
            }
        }
        
        expression.register_symbol_table(symbol_table);

        if (!parser.compile(processed_expr, expression)) {
            std::cout << "[EXPRTK ERROR] " << parser.error() << " in expression: " << expression_string << std::endl;
            throw std::runtime_error("BehavioralComponent: expression syntax error");
        }
        is_initialized = true;
    }

    void sync_variables(const Eigen::VectorXd& V, double dt) {
        dt_internal = dt;
        for (int i = 0; i < V.size(); ++i) {
            v_buffer[i] = V(i);
        }
    }

public:
    void updateHistory(const Eigen::VectorXd& V, double dt) override {
        time_internal += dt; 
        for(int i=0; i < V.size(); ++i) {
            v_nodes_prev[i] = V(i);
        }
        if (params) {
            for (auto const& [name, value] : params->getAll()) {
                params_prev_buffer[name] = value;
            }
        }
    }
};

#endif
