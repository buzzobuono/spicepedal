#ifndef POTENTIOMETER_H
#define POTENTIOMETER_H

#include <string>
#include <stdexcept>
#include <cmath>
#include <algorithm> // per std::max e std::clamp
#include <Eigen/Dense>
#include "component.h"
#include "components/resistor.h"

class Potentiometer : public Component
{
public:
    
    enum class TaperType { 
        LINEAR, 
        LOGARITHMIC 
    };

private:
    double _r_total;
    TaperType _taper;
    std::string _param;

    double getTaperedPosition() const
    {
        double pos = params->get(_param);
        
        pos = std::clamp(pos, 0.0, 1.0);

        switch (_taper)
        {
            case TaperType::LOGARITHMIC:
            {
                constexpr double k = 5.0;
                return std::pow(pos, k);
            }
            case TaperType::LINEAR:
            default:
                return pos;
        }
    }

public:
    Potentiometer(const std::string &comp_name,
                  int n1, int n2, int nw,
                  double r_total,
                  TaperType taper,
                  const std::string &param_name) // Passaggio per reference
        : _r_total(r_total), _taper(taper), _param(param_name)
    {
        if (r_total <= 0)
            throw std::runtime_error("Potentiometer total resistance must be positive");
        
        this->type = ComponentType::POTENTIOMETER;
        this->name = comp_name;
        this->nodes = {n1, n2, nw};
    }

    void stamp(Eigen::MatrixXd &G, Eigen::VectorXd &I, const Eigen::VectorXd &V, double dt) override
    {
        if (_r_total > R_MAX) return;
        
        double taperedPos = getTaperedPosition();
        double r1_val = std::max(_r_total * (1.0 - taperedPos), R_MIN);
        double r2_val = std::max(_r_total * taperedPos, R_MIN);

        int n1 = nodes[0], n2 = nodes[1], nw = nodes[2];

        Resistor r1(name + "_r1", n1, nw, r1_val);
        Resistor r2(name + "_r2", n2, nw, r2_val);

        r1.stamp(G, I, V, dt);
        r2.stamp(G, I, V, dt);
    }
};

#endif
