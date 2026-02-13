#ifndef POTENTIOMETER_H
#define POTENTIOMETER_H

#include <string>
#include <stdexcept>
#include <cmath>
#include <algorithm>

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
    
    const double R_MIN_SAFE = 0.1;
    
    double _r_total;
    TaperType _taper;
    std::string _param;
    int n1, n2, nw;
    
    void stampInternalResistor(Matrix& G, int nA, int nB, double res) {
        if (nA == nB || res > R_MAX) return;
        double g = 1.0 / std::max(res, R_MIN_SAFE);
        G(nA, nA) += g;
        G(nA, nB) -= g;
        G(nB, nB) += g;
        G(nB, nA) -= g;
    }
    
    double getTaperedPosition() const {
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
                  int node1, int node2, int nodew,
                  double r_total,
                  TaperType taper,
                  const std::string &param_name) // Passaggio per reference
        : _r_total(r_total), _taper(taper), _param(param_name)
    {
        if (r_total <= 0)
            throw std::runtime_error("Potentiometer total resistance must be positive");
        
        this->type = ComponentType::POTENTIOMETER;
        this->name = comp_name;
        n1 = node1;
        n2 = node2;
        nw = nodew;
    }

    void stamp(Matrix& G, Vector& I, const Vector& V) override {
        if (_r_total > R_MAX) return;
        
        double taperedPos = getTaperedPosition();
        double r1_val = std::max(_r_total * (1.0 - taperedPos), R_MIN_SAFE);
        double r2_val = std::max(_r_total * taperedPos, R_MIN_SAFE);

        stampInternalResistor(G, n1, nw, r1_val);
        stampInternalResistor(G, n2, nw, r2_val);
        
        G(n1, n1) += G_MIN_STABILITY;
        G(n2, n2) += G_MIN_STABILITY;
        G(nw, nw) += G_MIN_STABILITY;
        
    }
};

#endif
