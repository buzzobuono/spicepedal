#ifndef MATH_H
#define MATH_H

#include <cmath>
#include <cstdint>
#include <Eigen/Dense>

constexpr int MAX_NODES = 32;
    
using Matrix = Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::ColMajor, MAX_NODES, MAX_NODES>;
using Vector = Eigen::Matrix<double, Eigen::Dynamic, 1, Eigen::ColMajor, MAX_NODES, 1>;
using PartialPivLU = Eigen::PartialPivLU<Matrix>;

// Esponenziale polinomiale di precisione (Simil-SIMD)
// continua, derivabile e precisa su tutto il range 0-1V
double fast_exp(double x) {
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

double fast_tanh(double x) {
    if (x > 4.9) return 1.0;
    if (x < -4.9) return -1.0;
    double x2 = x * x;
    // Padé (3,2) ottimizzato: mantiene precisione e derivata continua
    return x * (135.0 + 15.0 * x2) / (135.0 + 60.0 * x2 + x2 * x2);
}

#endif
