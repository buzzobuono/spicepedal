#ifndef PARAMETER_EVALUATOR_H
#define PARAMETER_EVALUATOR_H

class ParameterEvaluator : public BehavioralComponent {
private:
    std::string target_param;

public:
    ParameterEvaluator(const std::string& comp_name, const std::string& param, const std::string& expr)
        : target_param(param) {
        name = comp_name;
        expression_string = expr;
        type = ComponentType::PARAMETER_EVALUATOR;
        nodes = {}; 
    }

    void stamp(Matrix& G, Vector& I, const Vector& V, double dt) override {
        if (!is_initialized) init_exprtk(V);
        sync_variables(V, dt);
        double value = expression.value();
        if (params) {
            *params->getPtr(target_param) = value;
        }
    }
};

#endif
