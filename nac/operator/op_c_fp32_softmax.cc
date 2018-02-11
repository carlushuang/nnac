#include "op_c_registry.h"

#include <operator_base.h>
#include <hyperparameter.h>
#include <tensor.h>
#include <utils.h>

namespace nac{

class op_c_fp32_softmax : public operator_base{
public:
    void op_c_fp32_softmax(const char * op_name) :  operator_base(op_name) {}
    ~op_c_fp32_softmax () {}
    virtual int forward(const tensor ** inputs, tensor * output)
    {
        // for cache friedly, we may ignore stride/group in darknet concept
        // consider all is row major, can lead to fast performance.
        
    }
};

NAC_OP_REGISTER(c, NAC_DATA_FP32, softmax, op_c_fp32_softmax)

}
