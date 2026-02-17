#ifndef DIODE_H
#define DIODE_H

#include <string>
#include <stdexcept>
#include <algorithm>
#include <cmath>

#include "component.h"

/*
double Is = 5.3e-9,      // 1N4148
double n = 1.68,
double Vt = 0.02585,
double Cj0 = 1.15e-12,   // Capacità (0 = disabilita)
double Vj = 0.74,
double Mj = 0.02)
*/
class Diode : public Component {
    
private:
    double _Is;
    double _n;
    double _Vt;
    double _Cj0;  // Junction capacitance
    double _Vj;   // Built-in potential
    double _Mj;   // Grading coefficient

    double vd_prev = 0.0;
    double dt = 0.0;
    double ieq_cap = 0.0;
    double g_cap = 0.0;
    
    int n1, n2;
    
public:
    Diode(const std::string& comp_name, int nn, int np,
                  double Is,
                  double n,
                  double Vt,
                  double Cj0,   // Capacità (0 = disabilita)
                  double Vj,
                  double Mj)
    {
        type = ComponentType::DIODE;
        if (Is <= 0 || n <= 0 || Vt <= 0) {
            throw std::runtime_error("Diode: parametri non validi");
        }
        name = comp_name;
        n1 = nn;
        n2 = np;
        _Is = Is;
        _n = n;
        _Vt = Vt;
        _Cj0 = Cj0;
        _Vj = Vj;
        _Mj = Mj;
    }

    void prepare(Matrix& G, Vector& I, Vector& V, double dt) override {
        this->dt = dt;
    }
    
    void prepareTimeStep() override {
        if (dt > 0 && _Cj0 > 0) {
            double vd_cap_prev = std::clamp(vd_prev, -5.0, 0.5);
            double Cj;
            if (vd_cap_prev < 0) {
                Cj = _Cj0 * std::pow(1.0 - vd_cap_prev / _Vj, -_Mj);
            } else {
                Cj = _Cj0 * 2.0;
            }
            g_cap = Cj / dt;
            ieq_cap = g_cap * vd_prev;
        }
    }
    
    void stamp(Matrix& G, Vector& I, const Vector& V) override {
        double vd = V(n1) - V(n2);
        vd = std::clamp(vd, -5.0, 1.0);
        
        double Vt_total = _n * _Vt;
        double exp_term = std::exp(vd / Vt_total);
        
        double id = _Is * (exp_term - 1.0);
        double gd = (_Is / Vt_total) * exp_term;
        double ieq = id - gd * vd;
        
        G(n1, n1) += gd;
        G(n1, n2) -= gd;
        G(n2, n1) -= gd;
        G(n2, n2) += gd;
        I(n1) -= ieq;
        I(n2) += ieq;
        
        if (dt > 0 && _Cj0 > 0) {
            G(n1, n1) += g_cap;
            G(n1, n2) -= g_cap;
            G(n2, n1) -= g_cap;
            G(n2, n2) += g_cap;
            I(n1) += ieq_cap;
            I(n2) -= ieq_cap;
        }
    }
    
   void updateHistory(const Vector& V) override {
        double v1 = V(n1);
        double v2 = V(n2);
        vd_prev = v1 - v2;
    }
    
    double getCurrent(const Vector& V) const override {
        double v1 = V(n1);
        double v2 = V(n2);
        double vd = v1 - v2;

        // 1. Parte Resistiva (Shockley Equation)
        double Vt_total = _n * _Vt;
        double id = _Is * (std::exp(vd / Vt_total) - 1.0);

        // 2. Parte Capacitiva (se abilitata e siamo in transiente)
        double ic = 0.0;
        if (dt > 0 && _Cj0 > 0) {
            double vd_cap = std::clamp(vd, -5.0, 0.5);
            double Cj = (vd_cap < 0) ? 
                        _Cj0 * std::pow(1.0 - vd_cap / _Vj, -_Mj) : 
                        _Cj0 * 2.0;
            
            // i = C * dv/dt -> Approssimazione Eulero Forward/Backward coerente con lo stamp
            ic = Cj * (vd - vd_prev) / dt;
        }

        return id + ic;
    }
    
    void reset() override {
        vd_prev = 0.0;
    }
};

#endif
