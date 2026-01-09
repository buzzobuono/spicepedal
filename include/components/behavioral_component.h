#ifndef BEHAVIORAL_COMPONENT_H
#define BEHAVIORAL_COMPONENT_H

#include "component.h"
#include "external/exprtk.hpp"
#include <string>
#include <vector>

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
    
    bool is_initialized = false;

    void init_exprtk(const Eigen::VectorXd& V) {
        v_buffer.resize(V.size(), 0.0);
        v_nodes_prev.resize(V.size(), 0.0);
        
        for (int i = 0; i < V.size(); ++i) {
            symbol_table.add_variable("V" + std::to_string(i), v_buffer[i]);
            symbol_table.add_variable("V" + std::to_string(i) + "prev", v_nodes_prev[i]);
        }
        
        symbol_table.add_variable("dt", dt_internal);
        symbol_table.add_variable("t", time_internal);

        // Registra parametri dal registro globale
        if (params) {
            for (auto const& [name, value] : params->getAll()) {
                symbol_table.add_variable(name, *params->getPtr(name));
            }
        }
        
        expression.register_symbol_table(symbol_table);

        if (!parser.compile(expression_string, expression)) {
            std::cout << "[EXPRTK ERROR] " << parser.error() << " in expression: " << expression_string << std::endl;
            throw std::runtime_error("BehavioralComponent: expression syntax error");
        }
        is_initialized = true;
    }

    // Metodo per aggiornare i buffer dei dati prima della valutazione
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
    }
};

#endif
