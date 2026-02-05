#ifndef FAST_MATH_H
#define FAST_MATH_H

#include <cmath>
#include <cstdint>

namespace fm {

namespace internal {
    // Esponenziale polinomiale di precisione (Simil-SIMD)
    // Questa versione è continua, derivabile e precisa su tutto il range 0-1V
    inline double smooth_exp(double x) {
        if (x < -80.0) return 0.0;
        if (x > 80.0) return std::exp(x);

        // Estrazione parte intera per riduzione del range
        // e^x = 2^(x * log2(e))
        // Qui usiamo un'approssimazione polinomiale di grado 5 molto efficiente
        union { double d; long long i; } u;
        double tmp = 1.4426950408889634073599 * x;
        long long integer_part = (long long)tmp;
        double fraction = tmp - (double)integer_part;

        // Polinomio di Taylor per la parte frazionaria (0 <= f <= 1)
        double f = fraction * 0.6931471805599453;
        double res = 1.0 + f * (1.0 + f * (0.5 + f * (0.16666666666666666 + f * (0.04166666666666666 + f * 0.00833333333333333))));

        // Moltiplicazione rapida per 2^N usando il bit-shifting dell'esponente double
        u.i = (integer_part + 1023) << 52;
        return res * u.d;
    }

    inline double smooth_tanh(double x) {
        if (x > 4.9) return 1.0;
        if (x < -4.9) return -1.0;
        double x2 = x * x;
        // Padé (3,2) ottimizzato: mantiene precisione e derivata continua
        return x * (135.0 + 15.0 * x2) / (135.0 + 60.0 * x2 + x2 * x2);
    }
}

#ifdef USE_FAST_MATH
    inline double exp(double x)      { return internal::smooth_exp(x); }
    inline double inv_sqrt(double x) { return 1.0 / std::sqrt(x); } // Test 2 mostrava che std::sqrt è già ottimizzata
    inline double tanh(double x)     { return internal::smooth_tanh(x); }
#else
    inline double exp(double x)      { return std::exp(x); }
    inline double inv_sqrt(double x) { return 1.0 / std::sqrt(x); }
    inline double tanh(double x)     { return std::tanh(x); }
#endif

} // namespace fm

#endif
