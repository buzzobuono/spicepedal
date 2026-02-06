#ifndef INTEGRATOR_H
#define INTEGRATOR_H

#include "components/component.h"

class Integrator : public Component {
private:
    int n_in, n_out;
    double Rout;
    double accumulator = 0.0;
    double current_input = 0.0;

public:
    // in: nodo frequenza (CV), out: nodo fase
    Integrator(const std::string& comp_name, int in, int out, double rout = 1.0)
        : n_in(in), n_out(out), Rout(rout) 
    {
        name = comp_name;
        nodes = {n_in, n_out};
        type = ComponentType::SUBCIRCUIT;
    }

    void stamp(Matrix& G, Vector& I, const Vector& V, double dt) override {
        // Catturiamo l'input attuale per lo step
        current_input = V(n_in);

        // Ci comportiamo come una sorgente di tensione ideale sul nodo di uscita
        // che impone il valore dell'accumulatore
        double g_out = 1.0 / Rout;
        double i_norton = accumulator * g_out;

        if (n_out != 0) {
            G(n_out, n_out) += g_out;
            I(n_out) += i_norton;
        }
    }

    void updateHistory(const Vector& V, double dt) override {
        // L'integrazione avviene solo qui, una volta per step temporale
        accumulator += V(n_in) * dt;

        // Opzionale: Wrap-around per oscillatori (mantiene la fase tra 0 e 1)
        // Utile per evitare che l'accumulatore diventi un numero enorme
        // if (accumulator > 1.0) accumulator -= 1.0;
    }
};

#endif