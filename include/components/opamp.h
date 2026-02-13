#ifndef OPAMP_H
#define OPAMP_H

#include <string>
#include <stdexcept>
#include <cmath>

#include "component.h"

class OpAmp : public Component {
private:
    int n_out;
    int n_plus;
    int n_minus;
    int n_vcc;
    int n_vee;

    // Parametri utente
    double Rout;      // Rout
    double Imax;      // Imax (A)
    double Gain;      // Open-loop gain
    double Sr;        // Slew rate in V/us

    // Interni
    double V_headroom;
    double v_out_prev;
    bool enable_slew;
    
    double dt = 0.0;
    
public:
    OpAmp(const std::string& comp_name,
          int out, int plus, int minus, int vcc, int vee,
          double rout,
          double imax,
          double gain,
          double sr)
        : n_out(out), n_plus(plus), n_minus(minus), n_vcc(vcc), n_vee(vee),
          Rout(rout), Imax(imax), Gain(gain), Sr(sr * 1e6),
          v_out_prev(0.0), enable_slew(true)
    {
        type = ComponentType::OPAMP;
        name = comp_name;
        
        if (Rout <= 0) throw std::runtime_error("OpAmp " + name + ": Rout must be > 0");
        if (Imax <= 0) throw std::runtime_error("OpAmp " + name + ": Imax must be > 0");
        if (Gain <= 0) throw std::runtime_error("OpAmp " + name + ": Gain must be > 0");
        if (Sr <= 0) throw std::runtime_error("OpAmp " + name + ": SR must be > 0");

        V_headroom = 1.0; // regolato dinamicamente
    }

    void prepare(Matrix& G, Vector& I, Vector& V, double dt) override {
        this->dt = dt;
    }
    
    void stamp(Matrix& G, Vector& I, const Vector& V) override {
        double v_o = (n_out  != 0) ? V(n_out)  : 0.0;
        double v_p = (n_plus != 0) ? V(n_plus) : 0.0;
        double v_m = (n_minus!= 0) ? V(n_minus): 0.0;
        double v_vcc = (n_vcc != 0) ? V(n_vcc) : 0.0;
        double v_vee = (n_vee != 0) ? V(n_vee) : 0.0;

        // Headroom dinamico
        double rail_span = v_vcc - v_vee;
        if (rail_span > 18.0)      V_headroom = 1.5;
        else if (rail_span < 12.0) V_headroom = 0.3;
        else                       V_headroom = 0.8;

        double Vsat_hi = v_vcc - V_headroom;
        double Vsat_lo = v_vee + V_headroom;

        double g_out = 1.0 / Rout;

        // --- Transconductance nominale
        double gm0 = Gain * g_out;
        double v_diff = v_p - v_m;
        double v_lin = Gain * v_diff;

        // --- Saturazione morbida: target
        double span = 0.5 * (Vsat_hi - Vsat_lo);
        double mid  = 0.5 * (Vsat_hi + Vsat_lo);
        double v_sat = mid + span * std::tanh((v_lin - mid) / (0.2 * span));

        // --- GM hard clamp aggressivo in saturazione
        double Vsat_mag = std::max(span, 1e-6);
        double sat_threshold = 1.0; // 100% del range
        double abs_vlin = std::abs(v_lin - mid);
        double gm = gm0;

        if (abs_vlin > sat_threshold * Vsat_mag) {
            double over = (abs_vlin / (sat_threshold * Vsat_mag)) - 1.0;
            double denom = 1.0 + 200.0 * std::pow(over + 1e-12, 1.5);
            double gm_min = 1e-6;
            gm = std::max(gm_min, gm0 / denom);
        }

        // --- Stamp G
        if (n_out != 0) {
            G(n_out, n_out) += g_out;
            if (n_plus != 0)  G(n_out, n_plus)  += gm;
            if (n_minus != 0) G(n_out, n_minus) -= gm;
        }

        // --- Slew rate
        double v_target = v_sat;
        double maxdv = Sr * dt;
        double dv = v_target - v_out_prev;
        if (enable_slew) {
            if (dv > maxdv)       v_target = v_out_prev + maxdv;
            else if (dv < -maxdv) v_target = v_out_prev - maxdv;
        }

        // --- Corrente correttiva e limitazione Imax
        double i_corr = g_out * (v_target - v_o);
        if (i_corr >  Imax) i_corr =  Imax;
        if (i_corr < -Imax) i_corr = -Imax;
        if (n_out != 0) I(n_out) += i_corr;

        // --- Input impedance 1M verso massa
        double g_in = 1e-6;
        if (n_plus != 0)  G(n_plus,  n_plus)  += g_in;
        if (n_minus!= 0)  G(n_minus, n_minus) += g_in;

        // --- Corrente quiescente
        double Iq = 0.002;
        if (n_vcc != 0) I(n_vcc) -= Iq;
        if (n_vee != 0) I(n_vee) += Iq;

        v_out_prev = v_o;
    }

    void updateHistory(const Vector& V) override {
        if (n_out != 0) v_out_prev = V(n_out);
    }

    void reset() override {
        v_out_prev = 0.0;
    }
};

#endif