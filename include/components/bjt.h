#ifndef BJT_H
#define BJT_H

#include <string>
#include <stdexcept>

#include "component.h"


class BJT : public Component {
public:
    enum BJTType { NPN, PNP };
    
private:
    BJTType bjt_type;
    int nc, nb, ne;  // collector, base, emitter nodes
    
    // Model parameters
    double IS;   // Saturation current (default: 1e-14 A)
    double BF;   // Forward beta (default: 100)
    double BR;   // Reverse beta (default: 1)
    double VT;   // Thermal voltage = kT/q ≈ 26mV
    
    // Previous iteration values (for Newton-Raphson)
    double vbe_prev, vbc_prev;
    
    static constexpr double V_LIMIT = 0.5;  // Voltage limiting for convergence
    
    // For PNP, flip signs
    double sign = 1.0;
    
    double inv_VT, inv_BF_VT, inv_BR_VT, IS_inv_VT, IS_inv_BF_VT, IS_inv_BR_VT;
    
    double* g_nb_nb_addr;
    double* g_nb_nc_addr;
    double* g_nb_ne_addr;
    double* g_nc_nb_addr;
    double* g_nc_nc_addr;
    double* g_nc_ne_addr;
    double* g_ne_nb_addr;
    double* g_ne_nc_addr;
    double* g_ne_ne_addr;

    double* i_nb_addr;
    double* i_nc_addr;
    double* i_ne_addr;

public:
    BJT(const std::string& comp_name, int collector, int base, int emitter, 
        double bf, double br, double is, double Vt)
    {
        if (is <= 0) {
            throw std::runtime_error("BJT: Saturation current IS must be positive");
        }
        if (bf <= 0) {
            throw std::runtime_error("BJT: Forward beta BF must be positive");
        }
        if (br <= 0) {
            throw std::runtime_error("BJT: Reverse beta BR must be positive");
        }
        if (Vt <= 0) {
            throw std::runtime_error("BJT: Thermal voltage VT must be positive");
        }
        if (collector == base || base == emitter || collector == emitter) {
            throw std::runtime_error(std::string("BJT: All three nodes must be different"));
        }
        this->type = ComponentType::BJT;
        name = comp_name;
        bjt_type = NPN;
        
        nc = collector;
        nb = base;
        ne = emitter;
        
        IS = is;
        BF = bf;
        BR = br;  // Typically much smaller than BF
        VT = Vt;  // 26mV at 300K
        
        vbe_prev = 0.0;
        vbc_prev = 0.0;
        
        sign = (bjt_type == PNP) ? -1.0 : 1.0;
       
        inv_VT = 1.0 / VT;
        inv_BF_VT = 1.0 / (BF * VT);
        inv_BR_VT = 1.0 / (BR * VT);
        IS_inv_VT = IS * inv_VT;
        IS_inv_BF_VT = IS * inv_BF_VT;
        IS_inv_BR_VT = IS * inv_BR_VT;
    }
    
    void prepare(Matrix& G, Vector& I, Vector& V, double dt) override {
        g_nb_nb_addr = &G(nb, nb);
        g_nb_nc_addr = &G(nb, nc);
        g_nb_ne_addr = &G(nb, ne);

        g_nc_nb_addr = &G(nc, nb);
        g_nc_nc_addr = &G(nc, nc);
        g_nc_ne_addr = &G(nc, ne);

        g_ne_nb_addr = &G(ne, nb);
        g_ne_nc_addr = &G(ne, nc);
        g_ne_ne_addr = &G(ne, ne);

        i_nb_addr = &I(nb);
        i_nc_addr = &I(nc);
        i_ne_addr = &I(ne);
    
    }

    __attribute__((always_inline))
    void stamp(Matrix& G, Vector& I, const Vector& V) override {
        // Read node voltages (handle ground)
        double vc = V(nc);
        double vb = V(nb);
        double ve = V(ne);
        
        // Junction voltages
        double vbe = vb - ve;
        double vbc = vb - vc;
    
        // Voltage limiting for convergence
        vbe = limitJunction(vbe, vbe_prev);
        vbc = limitJunction(vbc, vbc_prev);
        
        vbe *= sign;
        vbc *= sign;
        
        // Clamp exponentials to prevent overflow
        double exp_vbe = std::exp(std::min(vbe / VT, 80.0));
        double exp_vbc = std::exp(std::min(vbc / VT, 80.0));
        
        // Ebers-Moll diode currents
        double if_diode = IS * (exp_vbe - 1.0);  // Forward BE junction
        double ir_diode = IS * (exp_vbc - 1.0);  // Reverse BC junction
        
        // Terminal currents (Ebers-Moll equations)
        double ib = if_diode / BF + ir_diode / BR;
        double ic = if_diode - ir_diode;
        double ie = -(ib + ic);
        
        // Conductances (derivatives for linearization - Jacobian)
        double gbe = (IS_inv_BF_VT) * exp_vbe;  // ∂ib/∂vbe
        double gbc = (IS_inv_BR_VT) * exp_vbc;  // ∂ib/∂vbc
        double gce = (IS_inv_VT) * exp_vbe;         // ∂ic/∂vbe
        double gcc = -(IS_inv_VT) * exp_vbc;        // ∂ic/∂vbc
        
        // Equivalent current sources (companion model)
        // i = g*v + ieq  =>  ieq = i - g*v
        double ieq_b = ib - (gbe * vbe + gbc * vbc);
        double ieq_c = ic - (gce * vbe + gcc * vbc);
        double ieq_e = ie - (-(gbe + gce) * vbe - (gbc + gcc) * vbc);
        
         // ========================================
        // STAMPING - Complete 3x3 submatrix
        // ========================================
        *g_nb_nb_addr += gbe + gbc + G_MIN_STABILITY;
        *g_nb_nc_addr += -gbc;
        *g_nb_ne_addr += -gbe;
        *g_nc_nb_addr += (gce + gcc - gbc);
        *g_nc_nc_addr += (gbc - gcc) + G_MIN_STABILITY;
        *g_nc_ne_addr += -gce;
        *g_ne_nb_addr += (-gbe - (gce + gcc));
        *g_ne_nc_addr += gcc;
        *g_ne_ne_addr += gbe + gce + G_MIN_STABILITY;
        
        // Vettore delle correnti
        *i_nb_addr -= sign * ieq_b;
        *i_nc_addr -= sign * ieq_c;
        *i_ne_addr -= sign * ieq_e;
    }
    
    __attribute__((always_inline))
    void updateHistory(const Vector& V) override {
        // Lettura diretta: se un nodo è 0, V(0) restituisce correttamente 0.0
        double vc = V(nc);
        double vb = V(nb);
        double ve = V(ne);
        
        // Calcoliamo le tensioni di giunzione correnti (già pesate dal segno per NPN/PNP)
        vbe_prev = sign * (vb - ve);
        vbc_prev = sign * (vb - vc);
    }
    
    double getCurrent(const Vector& V) const override {
        // Lettura tensioni senza branching
        double vc = V(nc);
        double vb = V(nb);
        double ve = V(ne);
        
        // Tensioni di giunzione (normalizzate dal segno)
        double vbe = sign * (vb - ve);
        double vbc = sign * (vb - vc);
        
        // Correnti dei diodi interni (usiamo min per sicurezza numerica contro overflow)
        double exp_vbe = std::exp(std::min(vbe / VT, 80.0));
        double exp_vbc = std::exp(std::min(vbc / VT, 80.0));
        
        double if_diode = IS * (exp_vbe - 1.0);
        double ir_diode = IS * (exp_vbc - 1.0);
        
        // Corrente di Collettore (Ic) secondo il modello Ebers-Moll
        // Ic = If - Ir - (Ir/BR)
        double ic = if_diode - ir_diode * (1.0 + 1.0 / BR);
        
        // Se PNP, la corrente fisica è uscente dal collettore, quindi invertiamo il segno
        return sign * ic;
    }
    
    void reset() override {
        vbe_prev = 0.0;
        vbc_prev = 0.0;
    }
    
private:
    // Voltage limiting to help Newton-Raphson convergence
    double limitJunction(double vnew, double vold) {
        double dv = vnew - vold;
        
        // Limit the change in junction voltage
        if (std::abs(dv) > V_LIMIT) {
            return vold + std::copysign(V_LIMIT, dv);
        }
        
        // Also limit absolute voltage for uninitialized case
        if (std::abs(vnew) > 1.0 && std::abs(vold) < 0.1) {
            return std::copysign(0.7, vnew);  // Start near typical Vbe
        }
        
        return vnew;
    }
};

#endif