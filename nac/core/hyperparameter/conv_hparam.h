#if defined(_NAC_INCLUDE_HYPERPARAMETER_DETAIL)
#include "../common.h"


class conv_hparam : public hyperparameter{
public:
    virtual void map(hparam_map * pmap){
        kernel_ = pmap->find_int("kernel", 0);
        padding_ = pmap->find_int("padding", 0);
        stride_ = pmap->find_int("stride", 1);
        filters_ = pmap->find_int("filters", 0);
        act_type_ = hparam_to_act(pmap->find("activation").c_str());
    }
private:
    virtual void outsize(int * out_w, int * out_h, int * out_c, int * out_n){
        int w = op_node()->input()->w();
        int h = op_node()->input()->h();
        //int c = op_node()->input()->c();
        int n = op_node()->input()->n();

        *out_n = n;
        *out_c = filters();
        *out_h = (h + 2*padding()-kernel())/stride() +1 ;
        *out_w = (w + 2*padding()-kernel())/stride() +1 ;
    }
    NAC_RW_ATTR(int, kernel)
    NAC_RW_ATTR(int, padding)
    NAC_RW_ATTR(int, stride)
    NAC_RW_ATTR(int, filters)
    // NAC_RW_ATTR(int, batch_norm)
    NAC_RW_ATTR(activation_type, act_type)
    // NAC_RW_ATTR(int, dilation)
    // NAC_RW_ATTR(int, group)
};

#endif