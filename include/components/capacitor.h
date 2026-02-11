#ifndef CAPACITOR_H
#define CAPACITOR_H

#include <string>
#include <stdexcept>

#include "component.h"

class Capacitor : public Component {
    double C;
    double v_prev;   // tensione al passo precedente
    double i_prev;   // corrente equivalente al passo precedente
    double _geq;
    
public:
    Capacitor(const std::string& comp_name, int node1, int node2, double capacitance) {
        if (capacitance <= 0) {
            throw std::runtime_error(std::string("Capacitance must be positive"));
        }
        if (node1 == node2) {
            throw std::runtime_error(std::string("Capacitor nodes must be different"));
        }
        type = ComponentType::CAPACITOR;
        name = comp_name;
        nodes = { node1, node2 };
        C = capacitance;
        v_prev = 0.0;
        i_prev = 0.0;
    }

    void prepare(Matrix& G, Vector& I, Vector& V, double dt) override {
        if (dt > 0.0) {
            _geq = 2.0 * C / dt;
        } else {
            _geq = 0.0; // Caso DC
        }
    }
    
    void stampStatic(Matrix& G, Vector& I) override {
        int n1 = nodes[0], n2 = nodes[1];
        G(n1, n1) += _geq;
        G(n1, n2) -= _geq;
        G(n2, n2) += _geq;
        G(n2, n1) -= _geq;
    }
    
    __attribute__((always_inline))
    void stamp(Matrix& G, Vector& I, const Vector& V) override {
        double ieq = _geq * v_prev + i_prev;
        int n1 = nodes[0], n2 = nodes[1];
        I(n1) += ieq;
        I(n2) -= ieq;
    }
    
    __attribute__((always_inline))
    void updateHistory(const Vector& V) override {
        double v_n1 = V(nodes[0]);
        double v_n2 = V(nodes[1]);
        double v = v_n1 - v_n2;
        i_prev = _geq * (v - v_prev) - i_prev;
        v_prev = v;
    }

    void setInitialVoltage(double v0) {
        v_prev = v0;
        i_prev = 0.0; // Assumendo che parta da regime
    }

    double getCurrent(const Vector& V) const override {
        if (_geq <= 0.0) {
            return 0.0;
        }

        double v_n1 = (nodes[0] != 0) ? V(nodes[0]) : 0.0;
        double v_n2 = (nodes[1] != 0) ? V(nodes[1]) : 0.0;
        double v_now = v_n1 - v_n2;

        double current = _geq * (v_now - v_prev) - i_prev;

        return current;
    }
    
    void reset() override {
        v_prev = 0.0;
        i_prev = 0.0;
    }
};

#endif