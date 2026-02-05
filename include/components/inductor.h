#ifndef INDUCTOR_H
#define INDUCTOR_H

#include <string>
#include <stdexcept>
#include <Eigen/Dense>
#include "component.h"

class Inductor : public Component {
private:
    int n1, n2;  // nodi positivo e negativo
    
    // Parametri del modello
    double L;     // Induttanza [H]
    double R_dc;  // Resistenza DC dell'avvolgimento [Ohm] (tipicamente 10-500 Ohm per pickup chitarra)
    
    // Variabili di stato per integrazione trapezoidale
    double i_prev;     // Corrente al passo precedente
    double v_prev;     // Tensione ai capi al passo precedente
    
public:
    Inductor(const std::string& comp_name, int node_pos, int node_neg, 
             double inductance, double dc_resistance = 0.0) {
        if (inductance <= 0) {
            throw std::runtime_error("Inductor: Inductance L must be positive");
        }
        if (dc_resistance < 0) {
            throw std::runtime_error("Inductor: DC resistance R_dc cannot be negative");
        }
        
        this->type = ComponentType::INDUCTOR;
        name = comp_name;
        n1 = node_pos;
        n2 = node_neg;
        nodes = {n1, n2};
        
        L = inductance;
        R_dc = dc_resistance;
        
        i_prev = 0.0;
        v_prev = 0.0;
        
        // Validazione
        if (n1 == n2) {
            throw std::runtime_error("Inductor: Nodes must be different");
        }
    }
    
    // void stamp(Eigen::MatrixXd& G, Eigen::VectorXd& I, const Eigen::VectorXd& V, double dt) override {
    void stamp(Eigen::Ref<Eigen::MatrixXd> G, Eigen::Ref<Eigen::VectorXd> I, const Eigen::Ref<const Eigen::VectorXd>& V, double dt) override {     

        if (dt <= 0) {
            // Caso DC: induttore cortocircuitato trattato come resistenza con valore molto piccolo
            double g = 1e6; // Conduttanza molto alta (R ≈ 0)
            if (n1 >= 0) G(n1, n1) += g;
            if (n2 >= 0) G(n2, n2) += g;
            if (n1 >= 0 && n2 >= 0) {
                G(n1, n2) -= g;
                G(n2, n1) -= g;
            }
            return;
        }
        
        // Leggi tensioni nodali
        double v1 = (n1 != 0) ? V(n1) : 0.0;
        double v2 = (n2 != 0) ? V(n2) : 0.0;
        double v_L = v1 - v2;
        
        // Integrazione trapezoidale: i(t) = i(t-dt) + (dt/2L) * [v(t) + v(t-dt)]
        // Riorganizzando: v(t) = (2L/dt) * i(t) - (2L/dt) * i(t-dt) - v(t-dt)
        // In forma companion: v = Req * i + Veq
        
        double Req = (2.0 * L / dt) + R_dc;  // Resistenza equivalente
        double Geq = 1.0 / Req;              // Conduttanza equivalente
        
        // Sorgente di tensione equivalente (storia)
        double Veq = (2.0 * L / dt) * i_prev + v_prev + R_dc * i_prev;
        
        // Sorgente di corrente equivalente: Ieq = Veq / Req = Veq * Geq
        double Ieq = Veq * Geq;
        
        // ========================================
        // STAMPING - Modello companion (conduttanza + sorgente di corrente)
        // ========================================
        
        // Stampa conduttanza (come un resistore)
        if (n1 != 0 && n2 != 0) {
            G(n1, n1) += Geq;
            G(n1, n2) -= Geq;
            G(n2, n1) -= Geq;
            G(n2, n2) += Geq;
        } else if (n1 != 0) {
            G(n1, n1) += Geq;
        } else if (n2 != 0) {
            G(n2, n2) += Geq;
        }
        
        // Stampa sorgente di corrente equivalente
        // La corrente scorre da n1 a n2
        if (n1 != 0) I(n1) -= Ieq;
        if (n2 != 0) I(n2) += Ieq;
    }
    
    // void updateHistory(const Eigen::VectorXd& V, double dt) override {
    void updateHistory(const Eigen::Ref<const Eigen::VectorXd>& V, double dt) override {
        // Aggiorna le variabili di stato per il prossimo time step
        double v1 = (n1 != 0) ? V(n1) : 0.0;
        double v2 = (n2 != 0) ? V(n2) : 0.0;
        double v_L = v1 - v2;
        
        // Calcola corrente attuale dall'equazione trapezoidale
        // i(t) = i(t-dt) + (dt/2L) * [v(t) + v(t-dt)]
        i_prev = i_prev + (dt / (2.0 * L)) * (v_L + v_prev);
        v_prev = v_L;
    }
    
    void reset() override {
        i_prev = 0.0;
        v_prev = 0.0;
    }
    
    // double getCurrent(const Eigen::VectorXd& V, double dt) const override {
    double getCurrent(const Eigen::Ref<const Eigen::VectorXd>& V, double dt) const override {
        if (dt <= 0.0) {
            // In DC l'induttore è un corto circuito. 
            // La corrente dipende dal resto del circuito, ma se c'è R_dc:
            if (R_dc > 1e-9) {
                double v1 = (n1 != 0) ? V(n1) : 0.0;
                double v2 = (n2 != 0) ? V(n2) : 0.0;
                return (v1 - v2) / R_dc;
            }
            return 0.0; // Valore indefinito per R_dc = 0, restituiamo 0 per sicurezza
        }

        double v1 = (n1 != 0) ? V(n1) : 0.0;
        double v2 = (n2 != 0) ? V(n2) : 0.0;
        double v_now = v1 - v2;

        // Integrazione trapezoidale:
        // i(n) = i(n-1) + (dt / 2L) * [v(n) + v(n-1)]
        // Nota: Qui ignoriamo R_dc nel calcolo della "i" istantanea 
        // perché è già implicitamente gestita dal solver NR nel calcolo di v_now
        double i_now = i_prev + (dt / (2.0 * L)) * (v_now + v_prev);

        return i_now;
    }
    
};

#endif