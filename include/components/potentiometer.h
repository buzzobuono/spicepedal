#ifndef POTENTIOMETER_H
#define POTENTIOMETER_H

#include <string>
#include <stdexcept>
#include <cmath>
#include <Eigen/Dense>
#include "component.h"
#include "components/resistor.h"

class Potentiometer : public Component
{
public:
    enum class TaperType
    {
        LINEAR,
        LOGARITHMIC
    };

private:
    double _r_total;
    double _position;
    TaperType _taper;
    std::string _param;

    double _r1_val;
    double _r2_val;

    const double R_MIN = 1e-9;
    const double R_MAX = 1e12;

    double applyTaper()
    {
        _position = params->get(_param);
        switch (_taper)
        {
        case TaperType::LINEAR:
            return _position;
        case TaperType::LOGARITHMIC:
        {
            constexpr double k = 5.0;
            return std::pow(_position, k);
        }
        default:
            return _position;
        }
    }

    void updateResistances()
    {
        if (_r_total > R_MAX)
        {
            _r1_val = R_MAX;
            _r2_val = R_MAX;
            return;
        }
        double taperedPos = applyTaper();
        _r1_val = std::max(_r_total * (1.0 - taperedPos), R_MIN);
        _r2_val = std::max(_r_total * taperedPos, R_MIN);
    }

public:
    Potentiometer(const std::string &comp_name,
                  int n1, int n2, int nw,
                  double r_total,
                  TaperType taper,
                  const std::string param)
        : _r_total(r_total), _param(param), _taper(taper)
    {

        if (r_total <= 0)
            throw std::runtime_error("Potentiometer total resistance must be positive");

        _position = params->get(_param);
        if (_position < 0.0 || _position > 1.0)
            throw std::runtime_error("Potentiometer position must be in [0, 1]");
        if (n1 == n2 || n1 == nw || n2 == nw)
            throw std::runtime_error("Potentiometer nodes must be distinct");

        type = ComponentType::POTENTIOMETER;
        name = comp_name;
        nodes = {n1, n2, nw};

        updateResistances();
    }

    void stamp(Eigen::MatrixXd &G, Eigen::VectorXd &I, const Eigen::VectorXd &V, double dt) override
    {
        if (_r_total > R_MAX)
            return;

        int n1 = nodes[0], n2 = nodes[1], nw = nodes[2];

        Resistor r1(name + "_r1", n1, nw, _r1_val);
        Resistor r2(name + "_r2", n2, nw, _r2_val);

        r1.stamp(G, I, V, dt);
        r2.stamp(G, I, V, dt);
    }
};

#endif