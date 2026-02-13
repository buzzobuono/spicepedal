#ifndef CAPACITOR_H
#define CAPACITOR_H

#include <string>
#include <stdexcept>

#include "component.h"

class Capacitor : public Component {
    double C;
    double v_prev;   // tensione al passo precedente
    double i_prev;   // corrente equivalente al passo precedente
    double geq;
    double ieq;
    int n1, n2;
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
        n1 = node1;
        n2 = node2;
        C = capacitance;
        v_prev = 0.0;
        i_prev = 0.0;
    }

    void prepare(Matrix& G, Vector& I, Vector& V, double dt) override {
        if (dt > 0.0) {
            geq = 2.0 * C / dt;
        } else {
            geq = 0.0; // Caso DC
        }
    }
    
    void stampStatic(Matrix& G, Vector& I) override {
        G(n1, n1) += geq;
        G(n1, n2) -= geq;
        G(n2, n2) += geq;
        G(n2, n1) -= geq;
    }
    
    void prepareTimeStep() {
        ieq = geq * v_prev + i_prev;
    }

    __attribute__((always_inline))
    void stamp(Matrix& G, Vector& I, const Vector& V) override {
        I(n1) += ieq;
        I(n2) -= ieq;
    }
    
    __attribute__((always_inline))
    void updateHistory(const Vector& V) override {
        double v_n1 = V(n1);
        double v_n2 = V(n2);
        double v = v_n1 - v_n2;
        i_prev = geq * (v - v_prev) - i_prev;
        v_prev = v;
    }

    void setInitialVoltage(double v0) {
        v_prev = v0;
        i_prev = 0.0; // Assumendo che parta da regime
    }

    double getCurrent(const Vector& V) const override {
        double v_n1 = V(n1);
        double v_n2 = V(n2);
        double v_now = v_n1 - v_n2;
        double current = geq * (v_now - v_prev) - i_prev;
        return current;
    }
    
    void reset() override {
        v_prev = 0.0;
        i_prev = 0.0;
    }
};

#endif