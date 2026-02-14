#ifndef CAPACITOR_H
#define CAPACITOR_H

#include <string>
#include <stdexcept>

#include "component.h"

class Capacitor : public Component {
    
    private:
    double C;
    double v_prev = 0.0;
    double i_prev = 0.0;
    int n1, n2;
    double geq, geq_neg;
    
    public:
    double ieq, ieq_neg;
    
    Capacitor(const std::string& comp_name, int node1, int node2, double capacitance) {
        if (capacitance <= 0) {
            throw std::runtime_error(std::string("Capacitance must be positive"));
        }
        if (node1 == node2) {
            throw std::runtime_error(std::string("Capacitor nodes must be different"));
        }
        type = ComponentType::CAPACITOR;
        category = ComponentCategory::LINEAR_TIME_VARIANT;

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
        geq_neg = -geq;
    }
    
    void onTimeStep() override {
        ieq = geq * v_prev + i_prev;
        ieq_neg = -ieq;
    }

    std::vector<StaticGStampHook> getStaticGStamps() override {
        return {
            {n1, n1, geq}, 
            {n1, n2, geq_neg},
            {n2, n1, geq_neg},
            {n2, n2, geq}
        };
    }
    
    std::vector<IStampHook> getIStamps() override {
        return {
            {n1, &ieq},
            {n2, &ieq_neg}
        };
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