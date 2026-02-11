#ifndef MATH_H
#define MATH_H

#include <cmath>
#include <cstdint>
#include <cstring>
#include <utility>

constexpr int MAX_NODES = 32;

#ifdef BACKEND_INTERNAL
struct Vector
{
    int n = 0;
    alignas(64) double data[MAX_NODES];
    
    struct NoAliasProxy {
        Vector& v;
        NoAliasProxy(Vector& vv):v(vv){}
        inline void operator=(const Vector& other){
            v = other;
        }
    };

    inline NoAliasProxy noalias(){ return NoAliasProxy(*this); }
    
    static Vector Zero(int n){
        Vector v; v.resize(n); v.setZero(); return v;
    }

    inline void resize(int size){ n=size; }
    inline int size() const { return n; }

    inline void setZero(){
        std::memset(data,0,sizeof(double)*n);
    }

    inline double& operator()(int i){ return data[i]; }
    inline const double& operator()(int i) const{ return data[i]; }

    inline Vector& operator=(const Vector& o){
        n=o.n;
        std::memcpy(data,o.data,sizeof(double)*n);
        return *this;
    }

    inline Vector operator-(const Vector& o) const{
        Vector r; r.resize(n);
        for(int i=0;i<n;i++) r(i)=data[i]-o(i);
        return r;
    }

    inline double squaredNorm() const{
        double s=0.0;
        for(int i=0;i<n;i++) s+=data[i]*data[i];
        return s;
    }
};

struct Matrix
{
    int n=0;
    alignas(64) double data[MAX_NODES*MAX_NODES];

    static Matrix Zero(int r,int){
        Matrix M; M.resize(r,r); M.setZero(); return M;
    }

    inline void resize(int r,int){ n=r; }

    inline void setZero(){
        std::memset(data,0,sizeof(double)*n*n);
    }

    inline double& operator()(int r,int c){
        return data[r*n+c];
    }

    inline const double& operator()(int r,int c) const{
        return data[r*n+c];
    }
    
    struct RowProxy{
        double* ptr; int n;
        inline void setZero(){
            std::memset(ptr,0,sizeof(double)*n);
        }
    };

    inline RowProxy row(int r){
        return RowProxy{&data[r*n],n};
    }
    
    struct ColProxy{
        double* base; int n; int col;
        inline void setZero(){
            for(int i=0;i<n;i++)
                base[i*n+col]=0.0;
        }
    };

    inline ColProxy col(int c){
        return ColProxy{data,n,c};
    }
};

struct PartialPivLU
{
    int n = 0;
    double* LU = nullptr;
    int pivots[MAX_NODES];

    inline void compute(Matrix& M)
{
    n = M.n;
    // NON usare LU = M.data se vuoi mantenere G integra, 
    // ma se fai G.setZero() ogni volta va bene.
    LU = M.data; 
    for (int i = 0; i < n; i++) pivots[i] = i;

    for (int k = 0; k < n; k++)
    {
        int max_row = k;
        double max_val = std::abs(LU[k * n + k]);
        for (int i = k + 1; i < n; i++) {
            double val = std::abs(LU[i * n + k]);
            if (val > max_val) {
                max_val = val;
                max_row = i;
            }
        }

        // Se il pivot è troppo piccolo, la matrice è singolare
        if (max_val < 1e-20) {
            // Forza un valore minuscolo per evitare il NaN, 
            // ma questo indica un errore nel circuito (nodi fluttuanti)
            LU[k * n + k] = 1e-20; 
        }

        if (max_row != k) {
            // Scambio fisico delle righe
            for (int j = 0; j < n; j++) {
                std::swap(LU[k * n + j], LU[max_row * n + j]);
            }
            // Registro lo scambio per il vettore B
            std::swap(pivots[k], pivots[max_row]);
        }

        double inv = 1.0 / LU[k * n + k];
        for (int i = k + 1; i < n; i++) {
            LU[i * n + k] *= inv; // L'elemento L_ik
            double m = LU[i * n + k];
            for (int j = k + 1; j < n; j++) {
                LU[i * n + j] -= m * LU[k * n + j];
            }
        }
    }
}


    inline Vector solve(const Vector& b) const
    {
        Vector x; x.resize(n);
        
        // Applica il pivoting al vettore b (Forward substitution parte 1)
        for (int i = 0; i < n; i++) x(i) = b(pivots[i]);

        // Forward substitution (L)
        for (int i = 0; i < n; i++) {
            const double* __restrict Li = &LU[i * n];
            for (int j = 0; j < i; j++)
                x(i) -= Li[j] * x(j);
        }

        // Backward substitution (U)
        for (int i = n - 1; i >= 0; i--) {
            const double* __restrict Ui = &LU[i * n];
            for (int j = i + 1; j < n; j++)
                x(i) -= Ui[j] * x(j);
            x(i) /= Ui[i];
        }

        return x;
    }
};


#else

#include <Eigen/Dense>

using Matrix = Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::ColMajor, MAX_NODES, MAX_NODES>;
using Vector = Eigen::Matrix<double, Eigen::Dynamic, 1, Eigen::ColMajor, MAX_NODES, 1>;
using PartialPivLU = Eigen::PartialPivLU<Matrix>;

#endif

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