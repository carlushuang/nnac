#include "utils.h"

#include <math.h>
#include <float.h>
#include <fstream>

namespace nac{
namespace utils{


// blas like gemm from darknet
static inline void gemm_nn(int m, int n, int k, float alpha, 
        float *a, int lda, 
        float *b, int ldb,
        float *c, int ldc)
{
    int _i,_j,_k;
    //#pragma omp parallel for
    for(_i = 0; _i < m; ++_i){
        for(_k = 0; _k < k; ++_k){
            float a_part = alpha*a[_i*lda+_k];
            for(_j = 0; _j < n; ++_j){
                c[_i*ldc+_j] += a_part*b[_k*ldb+_j];
            }
        }
    }
}

static inline void gemm_nt(int m, int n, int k, float alpha, 
        float *a, int lda, 
        float *b, int ldb,
        float *c, int ldc)
{
    int _i,_j,_k;
    //#pragma omp parallel for
    for(_i = 0; _i < m; ++_i){
        for(_j = 0; _j < n; ++_j){
            float sum = 0;
            for(_k = 0; _k < k; ++_k){
                sum += alpha*a[_i*lda+_k]*b[_j*ldb + _k];
            }
            c[_i*ldc+_j] += sum;
        }
    }
}

static inline void gemm_tn(int m, int n, int k, float alpha, 
        float *a, int lda, 
        float *b, int ldb,
        float *c, int ldc)
{
    int _i,_j,_k;
    //#pragma omp parallel for
    for(_i = 0; _i < m; ++_i){
        for(_k = 0; _k < k; ++_k){
            float a_part = alpha*a[_k*lda+_i];
            for(_j = 0; _j < n; ++_j){
                c[_i*ldc+_j] += a_part*b[_k*ldb+_j];
            }
        }
    }
}

static inline void gemm_tt(int m, int n, int k, float alpha, 
        float *a, int lda, 
        float *b, int ldb,
        float *c, int ldc)
{
    int _i,_j,_k;
    //#pragma omp parallel for
    for(_i = 0; _i < m; ++_i){
        for(_j = 0; _j < n; ++_j){
            float sum = 0;
            for(_k = 0; _k < k; ++_k){
                sum += alpha*a[_i+_k*lda]*b[_k+_j*ldb];
            }
            c[_i*ldc+_j] += sum;
        }
    }
}

void gemm_cpu(int ta, int tb,
        int m, int n, int k, float alpha, 
        float *a, int lda, 
        float *b, int ldb,
        float *c, int ldc){
    // c must be init/clear before.

    if(!ta && !tb)
        gemm_nn(m, n, k, alpha, a, lda, b, ldb, c, ldc);
    else if(ta && !tb)
        gemm_tn(m, n, k, alpha, a, lda, b, ldb, c, ldc);
    else if(!ta && tb)
        gemm_nt(m, n, k, alpha, a, lda, b, ldb, c, ldc);
    else
        gemm_tt(m, n, k, alpha, a, lda, b, ldb, c, ldc);

}

static inline float stair_activate(float x)
{
    int n = floor(x);
    if (n%2 == 0) return floor(x/2.);
    else return (x - n) + floor(x/2.);
}
static inline float hardtan_activate(float x)
{
    if (x < -1) return -1;
    if (x > 1) return 1;
    return x;
}
static inline float linear_activate(float x){return x;}
static inline float logistic_activate(float x){return 1./(1. + exp(-x));}
static inline float loggy_activate(float x){return 2./(1. + exp(-x)) - 1;}
static inline float relu_activate(float x){return x*(x>0);}
static inline float elu_activate(float x){return (x >= 0)*x + (x < 0)*(exp(x)-1);}
static inline float relie_activate(float x){return (x>0) ? x : .01*x;}
static inline float ramp_activate(float x){return x*(x>0)+.1*x;}
static inline float leaky_activate(float x){return (x>0) ? x : .1*x;}
static inline float tanh_activate(float x){return (exp(2*x)-1)/(exp(2*x)+1);}
static inline float plse_activate(float x)
{
    if(x < -4) return .01 * (x + 4);
    if(x > 4)  return .01 * (x - 4) + 1;
    return .125*x + .5;
}

static inline float lhtan_activate(float x)
{
    if(x < 0) return .001*x;
    if(x > 1) return .001*(x-1) + 1;
    return x;
}

static inline float do_activate(float x, activation_type act_type)
{
    switch(act_type){
        case activation_type::LINEAR:
            return linear_activate(x);
        case activation_type::LOGISTIC:
            return logistic_activate(x);
        case activation_type::LOGGY:
            return loggy_activate(x);
        case activation_type::RELU:
            return relu_activate(x);
        case activation_type::ELU:
            return elu_activate(x);
        case activation_type::RELIE:
            return relie_activate(x);
        case activation_type::RAMP:
            return ramp_activate(x);
        case activation_type::LEAKY:
            return leaky_activate(x);
        case activation_type::TANH:
            return tanh_activate(x);
        case activation_type::PLSE:
            return plse_activate(x);
        case activation_type::STAIR:
            return stair_activate(x);
        case activation_type::HARDTAN:
            return hardtan_activate(x);
        case activation_type::LHTAN:
            return lhtan_activate(x);
    }
    return 0;
}
void activate_cpu(float * d, int num, activation_type act_type){
    int i;
    for(i = 0; i < num; ++i){
        d[i] = do_activate(d[i], act_type);
    }
}
void activate_cpu(float * din, float * dout, int num, activation_type act_type){
    int i;
    for(i = 0; i < num; ++i){
        dout[i] = do_activate(din[i], act_type);
    }
}

void softmax(float *input, int n, float temp, int stride, float *output)
{
    int i;
    float sum = 0;
    float largest = -FLT_MAX;
    for(i = 0; i < n; ++i){
        if(input[i*stride] > largest) largest = input[i*stride];
    }
    for(i = 0; i < n; ++i){
        float e = exp(input[i*stride]/temp - largest/temp);
        sum += e;
        output[i*stride] = e;
    }
    for(i = 0; i < n; ++i){
        output[i*stride] /= sum;
    }
}

/*
 * region node using softmax in darknet is very cache un-friendly
 * we may consider ignore stride/group in the future.
 */

void softmax_cpu(float *input, int n, int batch, int batch_offset, int groups, int group_offset, int stride, float temp, float *output)
{
    int g, b;
    for(b = 0; b < batch; ++b){
        for(g = 0; g < groups; ++g){
            softmax(input + b*batch_offset + g*group_offset, n, temp, stride, output + b*batch_offset + g*group_offset);
        }
    }
}

void bin_to_file(const char * file_name, const void * ptr, int bytes){
    if(!file_name || !ptr || bytes==0)
        return;
    std::ofstream out_file;
    out_file.open(file_name, std::ofstream::binary);
    if(!out_file){
        NAC_WARNING("can not open ", file_name, " to write bin");
        return ;
    }
    out_file.write((const char *)ptr, bytes);
    if(!out_file){
        NAC_WARNING("fail to write to file:", file_name);
    }
    out_file.close();
    return;
}


}
}