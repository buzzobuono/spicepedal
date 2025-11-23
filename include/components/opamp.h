
#ifndef OPAMP_H
#define OPAMP_H

#include <string>
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <Eigen/Dense>
#include "component.h"

/**
 * OpAmp modello semplificato per audio pedals
 * - VCCS (I_out = gm * (V+ - V-))
 * - Output impedance modellata come conduttanza verso massa (semplificazione)
 * - Soft saturation con tanh per stabilità numerica
 * - Slew-rate applicato sul target di uscita
 * - Limitazione corrente di uscita
 * - Input impedenza verso massa (alta)
 *
 * Note: è un modello pratico per simulazioni tempo-discrete e pedali audio,
 * non un modello transistor-level.
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
    double R_out;           // Output impedance (Ohm) -> modellato come conduttanza to ground
    double I_out_max;       // Max output current (A)
    double A_open;          // Open-loop gain (dimensionless)
    
    // Slew rate
    bool enable_slew_limiting;
    double slew_rate;       // V/s (internal)
    
    // State
    double v_out_prev;
    
    // Softclip parameter (how "soft" the clipping is). Higher => sharper clip.
    double softclip_k;
    
public:
    OpAmp(const std::string& comp_name, 
          int out, int plus, int minus, int vcc, int vee,
          double r_out = 75.0,
          double i_max = 0.05,
          double gain = 1e5,
          double sr_v_per_us = 5.0)
        : n_out(out), n_plus(plus), n_minus(minus), n_vcc(vcc), n_vee(vee),
          R_out(r_out), I_out_max(i_max), A_open(gain),
          enable_slew_limiting(true),
          slew_rate(sr_v_per_us * 1e6), // convert V/µs -> V/s
          v_out_prev(0.0),
          softclip_k(1.0)
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
        
        // Leggi tensioni nodali (0 -> ground)
        double v_out = (n_out != 0) ? V(n_out) : 0.0;
        double v_plus = (n_plus != 0) ? V(n_plus) : 0.0;
        double v_minus = (n_minus != 0) ? V(n_minus) : 0.0;
        double v_vcc = (n_vcc != 0) ? V(n_vcc) : 0.0;
        double v_vee = (n_vee != 0) ? V(n_vee) : 0.0;
        
        // Determina rails (headroom adattivo)
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
        // I_out = gm * (V+ - V-)
        // gm = A_open / R_out  (semplice mapping di scala)
        // ============================================================
        double g_out = 1.0 / R_out; // output conductance verso massa
        double gm = A_open * g_out;
        
        // Stamp della conduttanza di uscita (semplificazione)
        if (n_out != 0) {
            G(n_out, n_out) += g_out;
        }
        
        // Stamp transconduttanza: corrente in output dipendente da V+ - V-
        // Se nodo di ingresso è GND (0) non tocchiamo quelle colonne
        if (n_out != 0) {
            if (n_plus != 0) {
                // dI_out / dV_plus = +gm
                G(n_out, n_plus) += gm;
            }
            if (n_minus != 0) {
                // dI_out / dV_minus = -gm
                G(n_out, n_minus) -= gm;
            }
        }
        
        // ============================================================
        // SOFT SATURATION (continua) — usa tanh per stabilità
        // v_out_linear_ideal = A_open * (v_plus - v_minus)
        // v_out_soft = V_sat_pos * tanh( v_out_linear_ideal / V_sat_pos )
        // ============================================================
        double v_diff = v_plus - v_minus;
        double v_out_linear = A_open * v_diff; // teoria ideale
        // Scale the softclip threshold by rail (use smaller of pos/neg)
        double Vsat_mag = std::min(std::abs(V_sat_pos), std::abs(V_sat_neg));
        if (Vsat_mag < 0.1) Vsat_mag = 0.1; // safe lower bound
        // soften with a tunable factor
        double soft_k = softclip_k; // 1.0 => standard tanh
        double v_out_soft = Vsat_mag * std::tanh((v_out_linear / Vsat_mag) * soft_k);
        
        // ============================================================
        // SLEW RATE: limita la variazione rispetto al passo dt
        // ============================================================
        double v_target = v_out_soft;
        if (enable_slew_limiting && dt > 0.0) {
            double dv = v_target - v_out_prev;
            double dv_max = slew_rate * dt;
            if (std::abs(dv) > dv_max) {
                v_target = v_out_prev + (dv > 0 ? dv_max : -dv_max);
            }
        }
        
        // ============================================================
        // Applicare la correzione di corrente necessaria per portare
        // l'uscita verso v_target. Per stabilità Newton-Raphson, applichiamo
        // una correzione proporzionale lineare (Jacobian approximated by g_out)
        // ============================================================
        if (n_out != 0) {
            // corrente che il resto della rete "vedrà" verso massa dal nodo out:
            // I_corr = g_out * (v_target - v_out)  -> injection into I vector
            double i_correction = g_out * (v_target - v_out);
            
            // LIMITAZIONE CORRENTE: assicurati che la corrente immessa non superi I_out_max
            // Stimiamo la corrente assoluta richiesta e clamp se necessario.
            double i_abs = std::abs(i_correction);
            if (I_out_max > 0 && i_abs > I_out_max) {
                i_correction = (i_correction / i_abs) * I_out_max;
            }
            
            // Applichiamo la corrente correttiva (nota: segno coerente con I vettore)
            I(n_out) += i_correction;
        }
        
        // Aggiorna v_out_prev localmente (updateHistory farà il vero update alla fine del passo)
        // ma teniamo valore per successivi calcoli durante la stessa iterazione
        // (non sovrascriviamo con V perchè la soluzione Newton non è ancora convergente)
        // v_out_prev rimane l'ultima soluzione accettata — updateHistory sovrascrive quando serve.
        
        // ============================================================
        // INPUT IMPEDANCE: alta impedenza verso massa (1MΩ) per pedali
        // ============================================================
        double R_in = 1e6;  // 1 MΩ
        double g_in = 1.0 / R_in;
        if (n_plus != 0) {
            G(n_plus, n_plus) += g_in;
        }
        if (n_minus != 0) {
            G(n_minus, n_minus) += g_in;
        }
        
        // ============================================================
        // SUPPLY QUIESCENT CURRENT (small bias), modellato come sorgente costante
        // ============================================================
        double I_supply = 0.002;  // 2 mA quiescent
        if (n_vcc != 0) I(n_vcc) -= I_supply;
        if (n_vee != 0) I(n_vee) += I_supply;
        
        // Nota: non modifichiamo v_out_prev qui; updateHistory farà il commit
    }
    
    void updateHistory(const Eigen::VectorXd& V, double dt) override {
        // Dopo che il solver ha converguto per il passo, salviamo il valore per il prossimo dt
        if (n_out != 0) {
            v_out_prev = V(n_out);
        } else {
            v_out_prev = 0.0;
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
    
    void enableSlewRateLimiting(bool enable, double sr_v_per_us = 5.0) {
        enable_slew_limiting = enable;
        slew_rate = sr_v_per_us * 1e6;
    }
    
    void setSoftclipK(double k) {
        // k in [0.1 .. 5],  lower = softer, higher = harder clip
        softclip_k = std::max(0.1, std::min(k, 10.0));
    }
    
    // Getters
    double getOutputImpedance() const { return R_out; }
    double getMaxOutputCurrent() const { return I_out_max; }
    double getOpenLoopGain() const { return A_open; }
    double getSlewRate() const { return slew_rate / 1e6; }
    double getSoftclipK() const { return softclip_k; }
};

#endif