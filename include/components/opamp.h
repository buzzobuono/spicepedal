#ifndef OPAMP_H
#define OPAMP_H

#include <string>
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <Eigen/Dense>
#include "component.h"

/**
 * OpAmp modello VCCS (Voltage-Controlled Current Source)
 * Ottimizzato per stabilità con feedback negativo
 */
class OpAmp : public Component {
private:
    int n_out;      // Output node
    int n_plus;     // Non-inverting input (+)
    int n_minus;    // Inverting input (-)
    int n_vcc;      // Positive supply node (V+)
    int n_vee;      // Negative supply node (V-)
    
    // Parametri modello
    double V_headroom;      // Headroom from rails (V)
    double R_out;           // Output impedance (Ohm)
    double I_out_max;       // Max output current (A)
    double A_open;          // Open-loop gain (adimensionale)
    
    // Slew rate
    bool enable_slew_limiting;
    double slew_rate;       // V/s
    
    // State
    double v_out_prev;
    
public:
    OpAmp(const std::string& comp_name, 
          int out, int plus, int minus, int vcc, int vee,
          double r_out,
          double i_max,
          double gain,
          double sr)
        : n_out(out), n_plus(plus), n_minus(minus), n_vcc(vcc), n_vee(vee),
          R_out(r_out), I_out_max(i_max), A_open(gain),
          enable_slew_limiting(false), slew_rate(sr),
          v_out_prev(0.0)
    {
        this->type = ComponentType::OPAMP;
        name = comp_name;
        nodes = {n_out, n_plus, n_minus, n_vcc, n_vee};
        
        if (n_out == n_plus || n_out == n_minus || n_plus == n_minus) {
            throw std::runtime_error("OpAmp " + name + ": Output and input nodes must be different");
        }
        if (R_out <= 0) {
            throw std::runtime_error("OpAmp " + name + ": Output resistance must be positive");
        }
        if (A_open <= 0) {
            throw std::runtime_error("OpAmp " + name + ": Open-loop gain must be positive");
        }
        
        V_headroom = 1.5;
    }
    
    void stamp(Eigen::MatrixXd& G, Eigen::VectorXd& I, 
               const Eigen::VectorXd& V, double dt) override {
        
        // Leggi tensioni nodali
        double v_out = (n_out != 0) ? V(n_out) : 0.0;
        double v_plus = (n_plus != 0) ? V(n_plus) : 0.0;
        double v_minus = (n_minus != 0) ? V(n_minus) : 0.0;
        double v_vcc = (n_vcc != 0) ? V(n_vcc) : 0.0;
        double v_vee = (n_vee != 0) ? V(n_vee) : 0.0;
        
        // Determina rails
        double rail_span = v_vcc - v_vee;
        if (rail_span > 18.0) {
            V_headroom = 1.5;
        } else if (rail_span < 12.0) {
            V_headroom = 0.3;
        } else {
            V_headroom = 0.5;
        }
        
        double V_sat_pos = v_vcc - V_headroom;
        double V_sat_neg = v_vee + V_headroom;
        
        // ============================================================
        // MODELLO VCCS: Transconduttanza
        // ============================================================
        // I_out = gm × (V+ - V-)
        // dove gm = A_open / R_out (transconduttanza)
        
        double g_out = 1.0 / R_out;
        double gm = A_open * g_out;
        
        // Limita gm per stabilità numerica
        // Con R_out = 75Ω e gm_max = 100 → gain_eff ≈ 7500
        double gm_max = 100.0;
        
        if (gm > gm_max) {
            gm = gm_max;
        }
        
        // Stamp VCCS nella matrice G
        if (n_out != 0) {
            // Output conductance (resistenza uscita)
            G(n_out, n_out) += g_out;
            
            // Transconduttanza: accoppia output agli input
            if (n_plus != 0) {
                G(n_out, n_plus) += gm;   // dI/dV+ = +gm
            }
            if (n_minus != 0) {
                G(n_out, n_minus) -= gm;  // dI/dV- = -gm
            }
        }
        
        // ============================================================
        // SATURAZIONE: Aggiungi come correzione non-lineare
        // ============================================================
        double v_diff = v_plus - v_minus;
        double v_out_linear = (gm / g_out) * v_diff;  // Output teorico
        
        // Applica saturazione
        double v_out_saturated = v_out_linear;
        if (v_out_linear > V_sat_pos) {
            v_out_saturated = V_sat_pos;
        } else if (v_out_linear < V_sat_neg) {
            v_out_saturated = V_sat_neg;
        }
        
        // Se siamo in saturazione, aggiungi corrente correttiva
        if (n_out != 0 && std::abs(v_out_linear - v_out_saturated) > 0.001) {
            double i_correction = g_out * (v_out_saturated - v_out_linear);
            I(n_out) += i_correction;
        }
        
        // ============================================================
        // INPUT IMPEDANCE
        // ============================================================
        double R_in = 1e6;  // 1MΩ
        double g_in = 1.0 / R_in;
        
        if (n_plus != 0 && n_minus != 0) {
            G(n_plus, n_plus) += g_in;
            G(n_plus, n_minus) -= g_in;
            G(n_minus, n_plus) -= g_in;
            G(n_minus, n_minus) += g_in;
        } else if (n_plus != 0) {
            G(n_plus, n_plus) += g_in;
        } else if (n_minus != 0) {
            G(n_minus, n_minus) += g_in;
        }
        
        // ============================================================
        // SUPPLY CURRENT
        // ============================================================
        double I_supply = 0.002;  // 2mA quiescent
        if (n_vcc != 0) I(n_vcc) -= I_supply;
        if (n_vee != 0) I(n_vee) += I_supply;
        
        v_out_prev = v_out;
    }
    
    void updateHistory(const Eigen::VectorXd& V, double dt) override {
        if (n_out != 0) {
            v_out_prev = V(n_out);
        }
    }
    
    void reset() override {
        v_out_prev = 0.0;
    }
    
    // Setters
    void setOutputImpedance(double r_out_ohm) {
        if (r_out_ohm <= 0) throw std::runtime_error("OpAmp " + name + ": Rout must be > 0");
        R_out = r_out_ohm;
    }
    
    void setMaxOutputCurrent(double i_max_amps) {
        if (i_max_amps <= 0) throw std::runtime_error("OpAmp " + name + ": Imax must be > 0");
        I_out_max = i_max_amps;
    }
    
    void setOpenLoopGain(double gain) {
        if (gain <= 0) throw std::runtime_error("OpAmp " + name + ": Gain must be > 0");
        A_open = gain;
    }
    
    void enableSlewRateLimiting(bool enable, double sr_v_per_us = 13.0) {
        enable_slew_limiting = enable;
        slew_rate = sr_v_per_us * 1e6;
    }
    
    // Getters
    double getOutputImpedance() const { return R_out; }
    double getMaxOutputCurrent() const { return I_out_max; }
    double getOpenLoopGain() const { return A_open; }
    double getSlewRate() const { return slew_rate / 1e6; }
};

#endif