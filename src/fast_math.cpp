#include <iostream>
#include <cmath>
#include <chrono>
#include <iomanip>
#include "utils/fast_math.h"

// Macro per il benchmark
#define BENCHMARK(name, iterations, func_call, range_start, step) { \
    auto start = std::chrono::high_resolution_clock::now(); \
    volatile double s = 0; \
    for(int i = 0; i < iterations; ++i) { \
        double x = (double)((range_start) + (i % 1000) * (step)); \
        s += (func_call); \
    } \
    auto end = std::chrono::high_resolution_clock::now(); \
    std::chrono::duration<double> diff = end - start; \
    std::cout << "Tempo " << name << ": " << diff.count() << "s" << std::endl; \
    last_diff = diff.count(); \
}

int main() {
    const int iterations = 100000000;
    double last_diff = 0;
    double std_time = 0;

    std::cout << std::fixed << std::setprecision(6);
    std::cout << "=============================================================" << std::endl;
    std::cout << "      SIMULATION OPTIMIZATION SUITE: FAST_MATH               " << std::endl;
    
#ifdef USE_FAST_MATH
    std::cout << "      MODE: [ FAST_MATH ACTIVE ]                             " << std::endl;
#else
    std::cout << "      MODE: [ STANDARD ACCURACY ]                            " << std::endl;
#endif
    std::cout << "=============================================================" << std::endl;

    // --- TEST 1: EXP ---
    std::cout << "\n--- TEST 1: EXP (fm::exp vs std::exp) ---" << std::endl;
    double x_exp_val = 0.7 / 0.026; 
    double real_e = std::exp(x_exp_val);
    double approx_e = fm::exp(x_exp_val); 
    double err_exp = std::abs(real_e - approx_e) / (real_e + 1e-20) * 100.0;
    
    std::cout << "Accuratezza a 0.7V: " << err_exp << "%" << std::endl;

    auto start_exp = std::chrono::high_resolution_clock::now();
    volatile double sum_exp = 0;
    for(int i = 0; i < iterations; ++i) {
        sum_exp += std::exp((double)((i % 1000) * 0.08));
    }
    auto end_exp = std::chrono::high_resolution_clock::now();
    std_time = std::chrono::duration<double>(end_exp - start_exp).count();
    
    std::cout << "Tempo std::exp:  " << std_time << "s" << std::endl;
    BENCHMARK("fm::exp      ", iterations, fm::exp(x), 0.0, 0.08);
    std::cout << "Guadagno EXP:    " << std_time / last_diff << "x" << std::endl;


    // --- TEST 2: INV_SQRT ---
    std::cout << "\n--- TEST 2: INV_SQRT (fm::inv_sqrt vs 1/std::sqrt) ---" << std::endl;
    double x_isqrt_val = 4.0;
    double real_is = 1.0 / std::sqrt(x_isqrt_val);
    double approx_is = fm::inv_sqrt(x_isqrt_val);
    double err_isqrt = std::abs(real_is - approx_is) / (real_is + 1e-20) * 100.0;
    
    std::cout << "Accuratezza 1/sqrt(4.0): " << err_isqrt << "%" << std::endl;

    auto start_isqrt = std::chrono::high_resolution_clock::now();
    volatile double sum_isqrt = 0;
    for(int i = 0; i < iterations; ++i) {
        sum_isqrt += 1.0 / std::sqrt((double)(0.1 + (i % 1000) * 0.01));
    }
    auto end_isqrt = std::chrono::high_resolution_clock::now();
    std_time = std::chrono::duration<double>(end_isqrt - start_isqrt).count();
    
    std::cout << "Tempo 1/std::sqrt: " << std_time << "s" << std::endl;
    BENCHMARK("fm::inv_sqrt    ", iterations, fm::inv_sqrt(x), 0.1, 0.01);
    std::cout << "Guadagno ISQRT:    " << std_time / last_diff << "x" << std::endl;


    // --- TEST 3: TANH ---
    std::cout << "\n--- TEST 3: TANH (fm::tanh vs std::tanh) ---" << std::endl;
    double x_tanh_val = 1.2;
    double real_t = std::tanh(x_tanh_val);
    double approx_t = fm::tanh(x_tanh_val);
    double err_tanh = std::abs(real_t - approx_t) / (std::abs(real_t) + 1e-20) * 100.0;
    
    std::cout << "Accuratezza tanh(1.2): " << err_tanh << "%" << std::endl;

    auto start_tanh = std::chrono::high_resolution_clock::now();
    volatile double sum_tanh = 0;
    for(int i = 0; i < iterations; ++i) {
        sum_tanh += std::tanh((double)(-4.0 + (i % 1000) * 0.008));
    }
    auto end_tanh = std::chrono::high_resolution_clock::now();
    std_time = std::chrono::duration<double>(end_tanh - start_tanh).count();
    
    std::cout << "Tempo std::tanh:   " << std_time << "s" << std::endl;
    BENCHMARK("fm::tanh        ", iterations, fm::tanh(x), -4.0, 0.008);
    std::cout << "Guadagno TANH:     " << std_time / last_diff << "x" << std::endl;

    std::cout << "\n=============================================================" << std::endl;
    return 0;
}
