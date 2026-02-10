#ifndef MATH_H
#define MATH_H

#include <cmath>
#include <cstdint>
#include <Eigen/Dense>

constexpr int MAX_NODES = 32;

#ifndef BACKEND_INTERNAL
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
    int n=0;
    double* LU=nullptr;
    
    inline void compute(Matrix& M)
    {
        LU=M.data;
        n=M.n;

        for(int k=0;k<n;k++)
        {
            double Akk=LU[k*n+k];
            double inv=1.0/Akk;

            double* __restrict Ak=&LU[k*n];

            for(int i=k+1;i<n;i++)
            {
                double* __restrict Ai=&LU[i*n];

                double f=Ai[k]*inv;
                Ai[k]=f;

                for(int j=k+1;j<n;j++)
                    Ai[j]-=f*Ak[j];
            }
        }
    }
    
    inline Vector solve(const Vector& b) const
    {
        Vector x; x.resize(n);
        
        for(int i=0;i<n;i++) x(i)=b(i);
        
        for(int i=0;i<n;i++)
        {
            double sum=x(i);
            const double* __restrict Li=&LU[i*n];

            for(int j=0;j<i;j++)
                sum-=Li[j]*x(j);

            x(i)=sum;
        }
        
        for(int i=n-1;i>=0;i--)
        {
            double sum=x(i);
            const double* __restrict Ui=&LU[i*n];

            for(int j=i+1;j<n;j++)
                sum-=Ui[j]*x(j);

            x(i)=sum/Ui[i];
        }

        return x;
    }
};

#else

using Matrix = Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::ColMajor, MAX_NODES, MAX_NODES>;
using Vector = Eigen::Matrix<double, Eigen::Dynamic, 1, Eigen::ColMajor, MAX_NODES, 1>;
using PartialPivLU = Eigen::PartialPivLU<Matrix>;

#endif

#endif
