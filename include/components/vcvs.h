#ifndef VCVS_H
#define VCVS_H

#include <string>
#include <algorithm>
#include <Eigen/Dense>
#include "component.h"

class VCVS : public Component {
private:
    int n_out_p;   // Nodo uscita positivo
    int n_out_m;   // Nodo uscita negativo (solitamente 0)
    int n_ctrl_p;  // Nodo controllo positivo (In+)
    int n_ctrl_m;  // Nodo controllo negativo (In-)

    double Gain;
    double Vmax;
    double Vmin;
    double Rout;

public:
    VCVS(const std::string& comp_name,
         int out_p, int out_m, int ctrl_p, int ctrl_m,
         double gain, double v_max, double v_min, double rout)
        : n_out_p(out_p), n_out_m(out_m), n_ctrl_p(ctrl_p), n_ctrl_m(ctrl_m),
          Gain(gain), Vmax(v_max), Vmin(v_min), Rout(rout)
    {
        name = comp_name;
        type = ComponentType::VCVS; // Assicurati che VCVS sia nell'enum ComponentType
        nodes = {n_out_p, n_out_m, n_ctrl_p, n_ctrl_m};
    }

    void stamp(Eigen::MatrixXd& G, Eigen::VectorXd& I,
               const Eigen::VectorXd& V, double dt) override
    {
        // 1. Recupero tensioni ai nodi di controllo
        double v_cp = (n_ctrl_p != 0) ? V(n_ctrl_p) : 0.0;
        double v_cm = (n_ctrl_m != 0) ? V(n_ctrl_m) : 0.0;

        // 2. Calcolo tensione target con Hard Clamp
        //double v_target = Gain * (v_cp - v_cm);
        //v_target = std::clamp(v_target, Vmin, Vmax);
        double x = Gain * (v_cp - v_cm);
        double v_target = Vmax * std::tanh(x / Vmax);
        
        // 3. Conversione in modello Norton (I = Vtarget / Rout, G = 1 / Rout)
        double g_out = 1.0 / Rout;
        double i_norton = v_target * g_out;

        // 4. Stamp sulla matrice di Conduttanza G e vettore Correnti I
        if (n_out_p != 0) {
            G(n_out_p, n_out_p) += g_out;
            I(n_out_p) += i_norton;
        }

        if (n_out_m != 0) {
            G(n_out_m, n_out_m) += g_out;
            I(n_out_m) -= i_norton;
        }

        // Se l'uscita è differenziale, aggiungiamo i termini incrociati
        if (n_out_p != 0 && n_out_m != 0) {
            G(n_out_p, n_out_m) -= g_out;
            G(n_out_m, n_out_p) -= g_out;
        }
    }

    void updateHistory(const Eigen::VectorXd& V, double dt) override {
        // Componente puramente algebrico, non serve memoria
    }
};

#endif
