#ifndef PARAMETER_EVALUATOR_H
#define PARAMETER_EVALUATOR_H

class ParameterEvaluator : public BehavioralComponent {
private:
    std::string target_param;

public:
    ParameterEvaluator(const std::string& comp_name, const std::string& target, const std::string& expr)
        : target_param(target) {
        name = comp_name;
        expression_string = expr;
        type = ComponentType::PARAM_EVALUATOR;
        nodes = {}; 
    }

    void stamp(Eigen::MatrixXd& G, Eigen::VectorXd& I, const Eigen::VectorXd& V, double dt) override {
        if (!is_initialized) init_exprtk(V);
        
        sync_variables(V, dt);
        last_value = expression.value();

        if (params) {
            *params->getPtr(target_param) = last_value;
        }
    }
};

#endif
