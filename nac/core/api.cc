#include <nac.h>
// exported header first, in case we hidden api in the following


// TODO: backend select
#ifndef NAC_BREW_BACKEND_DARKNET
#define NAC_BREW_BACKEND_DARKNET    1
#endif

#include "context.h"
#include "compute_device.h"
#include "hyperparameter.h"
#include "operator_base.h"
#include "node.h"
#include "graph.h"
#include "op_registry.h"
#include "tensor.h"
#include "brew.h"
#include "args_map.h"


/* export api */

using namespace nac;

/* context related api */
static bool is_device_available(compute_device * dev){
    std::vector<compute_device*> available_devs;
    probe_compute_devices(available_devs);
    bool in_list= false;
    for(auto & cdev : available_devs){
        if(cdev == dev){
            in_list = true;
            break;
        }
    }
    return in_list;
}

NAC_EXPORT nac_context nac_context_create(nac_device *  devices, int num_device){
    if(unlikely(!devices || num_device==0 ))
        return nullptr;
    compute_device ** ds = (compute_device**)devices;
    for(int i=0;i<num_device;i++)
        if(!is_device_available(ds[i]))
            return nullptr;

    context * ctx = new context(ds, num_device);
    return ctx;
}

NAC_EXPORT nac_status nac_context_release(nac_context ctx){
    if(unlikely(!ctx))
        return NAC_INVALID_ARG;
    delete (context*)ctx;
    return NAC_SUCCESS;
}

/* device related api */
NAC_EXPORT nac_status nac_device_get_info(nac_device dev, struct nac_device_info * info){
    if(unlikely(!dev || !info))
        return NAC_INVALID_ARG;
    compute_device * d = (compute_device *)dev;
    if(!is_device_available(d))
        return NAC_INVALID_DEVICE;

    info->dev_name = const_cast<char*>(d->name().c_str());

    info->num_op_entries = d->registry()->op_entry_count();
    info->op_entry_names = const_cast<char**>(d->registry()->op_entry_names().data());
    // std::string cur_en = dev->current_entry_name();

    return NAC_SUCCESS;
}

NAC_EXPORT nac_status nac_device_get(nac_device ** devices, int * num_devices){
    compute_device ** devs;
    int num_devs;
    if(unlikely(!devices || !num_devices))
        return NAC_INVALID_ARG;
    probe_compute_devices(&devs, &num_devs);
    if(num_devs == 0)
        return NAC_DEVICE_NOT_FOUND;

    *devices = (void**)devs;
    *num_devices = num_devs;

    return NAC_SUCCESS;
}

NAC_EXPORT nac_status nac_device_put(nac_device dev){
    (void)dev;
    return NAC_SUCCESS;
}

/* op_entry related api */
NAC_EXPORT nac_op_entry nac_op_entry_get(nac_device dev, const char * entry_name){
    if(unlikely(!dev || !entry_name))
        return nullptr;
    compute_device * d = (compute_device *)dev;

    return d->registry()->get_op_entry(entry_name);
}

NAC_EXPORT nac_status nac_op_entry_put(nac_op_entry op_entry){
    if(unlikely(!op_entry))
        return NAC_INVALID_ARG;
    op_registry::op_entry_type * ope = (op_registry::op_entry_type *)op_entry;
    ope->registry->put_op_entry(ope);
    return NAC_SUCCESS;
}

/* hparam related api */
NAC_EXPORT nac_hparam nac_hparam_create(const char * op_name){
    return new hparam_map(op_name);
}
NAC_EXPORT nac_status nac_hparam_set(nac_hparam hparam, const char * param_name, const char * value){
    if(unlikely(!hparam || !param_name || ! value))
        return NAC_INVALID_ARG;

    hparam_map * hp = (hparam_map *)hparam;
    int rtn = hp->insert(param_name, value);
    if(rtn == 0)
        return NAC_SUCCESS;
    else
        return NAC_DUPLICATED_PARAM_NAME;
}
NAC_EXPORT nac_status nac_hparam_release(nac_hparam hparam){
    if(unlikely(!hparam))
        return NAC_INVALID_ARG;
    hparam_map * hp = (hparam_map *)hparam;
    delete hp;
    return NAC_SUCCESS;
}

/* brew related api */

NAC_EXPORT nac_brew nac_brew_create(nac_context ctx, const char * args){
    if(unlikely(!ctx||!args))
        return nullptr;
#if NAC_BREW_BACKEND_DARKNET
    args_map * _args = new args_map();
    if(!_args->parse(args)){
        NAC_ERROR("fail to parse args:", args);
        delete _args;
        return nullptr;
    }
    brew * br = new brew_darknet(_args);
    return (void*)br;
#endif
    return nullptr;
}
NAC_EXPORT nac_status nac_brew_release(nac_brew br){
    if(unlikely(!br))
        return NAC_INVALID_ARG;
    brew * b = (brew*)br;
    delete b;
    return NAC_SUCCESS;
}

/* node related api */
NAC_EXPORT nac_node nac_node_create(nac_context ctx, nac_op_entry op_entry,  const char * op_name){
    if(unlikely(!ctx || ! op_entry || !op_name))
        return nullptr;
    if(!check_op_supported(op_name)){
        NAC_ERROR("request op:", op_name, " is not supported");
        return nullptr;
    }

    context * c = (context*)ctx;
    op_registry::op_entry_type * ope = (op_registry::op_entry_type *)op_entry;

    bool found = false;
    auto & dev =  ope->registry->working_device();
    for(auto & d : c->devices()){
        if(d == dev){
            found = true;
            break;
        }
    }

    if(!found){
        NAC_ERROR("count not find the dev");
        return nullptr;
    }

    op_registry * opr = dev->registry();
    operator_base * op;

    op = opr->get_op(ope, op_name  /*node name same as op name*/ );
    if(!op){
        NAC_ERROR("count not find node:", op_name, " in dev:", dev->name(), " with entry:", data_type_to_str(ope->data_type));
        return nullptr;
    }

    node * the_node = new node(c);
    the_node->attach_op(op);

    return the_node;
}

NAC_EXPORT nac_status nac_node_feed_weights(nac_node nd, nac_tensor * weights, int num_weights){
    if(unlikely(!nd || !weights || num_weights==0))
        return NAC_INVALID_ARG;
    node * n = (node*)nd;
    n->feed_weights((tensor**)weights, num_weights);
    return NAC_SUCCESS;
}
NAC_EXPORT nac_status nac_node_set_hparam(nac_node nd, nac_hparam hparam){
    if(unlikely(!nd || !hparam))
        return NAC_INVALID_ARG;
    
    node * n = (node*)nd;
    hparam_map * hp = (hparam_map*)hparam;
    n->feed_hparam(hyperparameter_factory(hp->name().c_str(), hp));
    return NAC_SUCCESS;
}
NAC_EXPORT nac_status nac_node_release(nac_node nd){
    if(unlikely(!nd))
        return NAC_INVALID_ARG;

    node* n = (node*)nd;
    delete n;
    return NAC_SUCCESS;
}

/* graph related api */

NAC_EXPORT nac_graph nac_graph_create(nac_context ctx){
    if(unlikely(!ctx))
        return nullptr;
    nac_graph gr = new graph((context*)ctx);
    return gr;
}
NAC_EXPORT nac_status nac_graph_attach_node(nac_graph gr, nac_node * nodes, int num){
    if(unlikely(!gr || !nodes || num==0))
        return NAC_INVALID_ARG;
    graph * g = (graph*)gr;
    g->attach_nodes((node**)nodes, num);
    return NAC_SUCCESS;
}
NAC_EXPORT nac_status nac_graph_feed_inputs(nac_graph gr, nac_tensor * inputs, int num_inputs){
    if(unlikely(!gr || !inputs || num_inputs==0))
        return NAC_INVALID_ARG;
    graph * g = (graph*)gr;
    g->feed_inputs((tensor**)inputs, num_inputs);
    return NAC_SUCCESS;
}
NAC_EXPORT nac_status nac_graph_prepare(nac_graph gr){
    if(unlikely(!gr))
        return NAC_INVALID_ARG;
    graph * g = (graph*)gr;
    g->prepare();
    return NAC_SUCCESS;
}
NAC_EXPORT nac_status nac_graph_start_inference(nac_graph gr, int loop, int need_wait){
    if(unlikely(!gr))
        return NAC_INVALID_ARG;

    graph * g = (graph*)gr;
    g->start_inference(loop, need_wait?true:false);
    return NAC_SUCCESS;
}
NAC_EXPORT nac_status nac_graph_wait(nac_graph gr){
    if(unlikely(!gr))
        return NAC_INVALID_ARG;
    graph * g = (graph*)gr;
    g->wait();
    return NAC_SUCCESS;
}
NAC_EXPORT nac_status nac_graph_get_result(nac_graph gr, nac_tensor * out){
    // currently only 1 output
    if(unlikely(!gr || !out))
        return NAC_INVALID_ARG;
    graph * g = (graph*)gr;
    //*num_outs = 1;
    *out = (void*)g->output(0);
    return NAC_SUCCESS;
}

NAC_EXPORT nac_status nac_graph_release(nac_graph gr){
    if(unlikely(!gr))
        return NAC_INVALID_ARG;
    delete (graph*)gr;
    return NAC_SUCCESS;
}

/* tensor related api */

NAC_EXPORT nac_tensor nac_tensor_create(nac_op_entry op_entry, int w, int h, int c, int n){
    if(op_entry){
        op_registry::op_entry_type * ope = (op_registry::op_entry_type *)op_entry;
        return new tensor(w,h,c,n,&ope->op_dm);
    }
    else
        return new tensor(w,h,c,n);
}
NAC_EXPORT nac_status nac_tensor_release(nac_tensor t){
    if(unlikely(!t))
        return NAC_INVALID_ARG;

    delete (tensor*)t;
    return NAC_SUCCESS;
}

NAC_EXPORT nac_status nac_tensor_set_data_raw(nac_tensor t, void * data){
    if(unlikely(!t))
        return NAC_INVALID_ARG;
    tensor * ts = (tensor*)t;
    ts->feed_external(data);
    return NAC_SUCCESS;
}
NAC_EXPORT void * nac_tensor_get_data_raw(nac_tensor t){
    if(unlikely(!t))
        return nullptr;
    tensor * ts = (tensor*)t;
    return ts->data();
}

NAC_EXPORT nac_status nac_tensor_get_info(nac_tensor t, nac_tensor_info * info){
    if(unlikely(!t || !info))
        return NAC_INVALID_ARG;
    tensor * ts = (tensor*)t;
    info->w = ts->w();
    info->h = ts->h();
    info->c = ts->c();
    info->n = ts->n();
    info->raw_data = ts->data();
    return NAC_SUCCESS;
}

NAC_EXPORT nac_status nac_tensor_copy_data(void * src, int src_offset, void * dest, int dest_offset, 
                                            int nmemb, enum tensor_copy_direct direction){
    if(unlikely(!src || !dest))
        return NAC_INVALID_ARG;
    int rtn;
    if(direction == TENSOR_COPY_D2H){
        tensor * t = static_cast<tensor*>(src);
        rtn = t->copy_from_host(dest, dest_offset, src_offset, nmemb);
        if(rtn == 0)
            return NAC_SUCCESS;
    }else if(direction == TENSOR_COPY_H2D){
        tensor * t = static_cast<tensor*>(dest);
        rtn = t->copy_to_host(src, src_offset, dest_offset, nmemb);
        if(rtn == 0)
            return NAC_SUCCESS;
    }else if(direction == TENSOR_COPY_D2D){
        tensor * t_src = static_cast<tensor*>(src);
        tensor * t_dest = static_cast<tensor*>(dest);
        //rtn = t_src->copy(t_dest,dest_offset, src_offset, nmemb  );
        rtn = t_dest->copy(*t_src,src_offset, dest_offset, nmemb  );
        if(rtn == 0)
            return NAC_SUCCESS;
    }
    else
        return NAC_INVALID_TENSOR_COPY_DIRECT;
    
    return NAC_TENSOR_COPY_FAIL;
}
