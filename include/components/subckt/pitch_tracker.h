#ifndef PITCH_TRACKER_H
#define PITCH_TRACKER_H

#include "components/component.h"
#include <Eigen/Dense>
#include <vector>

class PitchTracker : public Component {
private:
    int n_in, n_out;       // Nodi di ingresso (segnale) e uscita (tensione CV)
    double threshold;      // Soglia di scatto (es. 0.05V)
    double last_cross_t;   // Tempo dell'ultimo zero-crossing
    double current_freq;   // Frequenza istantanea calcolata
    int last_state;        // 1 se sopra soglia, -1 se sotto
    
    // Filtro per stabilizzare l'uscita
    double smoothed_freq;
    double alpha;          // Fattore di smoothing (0.0 a 1.0)

public:
    PitchTracker(const std::string& name, int in, int out, double thr = 0.02, double smooth = 0.2)
        : n_in(in), n_out(out), threshold(thr), alpha(smooth) 
    {
        this->name = name;
        nodes = {n_in, n_out};
        type = ComponentType::SUBCIRCUIT;
        last_cross_t = 0.0;
        current_freq = 0.0;
        smoothed_freq = 0.0;
        last_state = 0;
    }

    void stamp(Eigen::MatrixXd& G, Eigen::VectorXd& I, const Eigen::VectorXd& V, double dt) override {
        // Il tracker si comporta come una sorgente di tensione ideale (V-Source) sull'uscita
        // che impone la tensione pari alla frequenza calcolata.
        
        double g_out = 1000.0; // Impedenza d'uscita virtuale
        if (n_out != 0) {
            G(n_out, n_out) += g_out;
            I(n_out) += smoothed_freq * g_out; 
        }
    }

    void updateHistory(const Eigen::VectorXd& V, double dt) override {
        double v_in = V(n_in);
        static double internal_time = 0;
        internal_time += dt;

        // Logica di Zero-Crossing con Isteresi
        if (v_in > threshold && last_state <= 0) {
            // Fronte di salita rilevato
            double period = internal_time - last_cross_t;
            if (period > 0.001) { // Limite superiore ~1000Hz per stabilità
                current_freq = 1.0 / period;
                // Applica filtro passa-basso digitale al valore di frequenza
                smoothed_freq = (alpha * current_freq) + (1.0 - alpha) * smoothed_freq;
            }
            last_cross_t = internal_time;
            last_state = 1;
        } else if (v_in < -threshold) {
            last_state = -1;
        }
    }
    
};

#endif