#include "graph.h"
#include "context.h"
#include "node.h"
#include "tensor.h"
#include "workspace.h"

namespace nac{

graph::graph(context * _ctx) :ctx(_ctx), execution_thread(nullptr), execution_need_exit(false),
        in_execution(false), prepared(false)
{}
graph::~graph(){
    if(execution_thread){
        execution_need_exit = true;
        execution_thread->join();
        delete execution_thread;
        in_execution = false;
    }
    release_nodes();
    release_inputs();
}

void graph::prepare(){
    if(prepared){
        NAC_WARNING("have prepared before, will do nothing");
        return;
    }

    // set graph input to the first node
    nodes[0]->set_inputs(inputs_list.data(), inputs_list.size());

    for(auto & n : nodes){
        n->prepare();
    }
    prepared = true;
}

void graph::start_inference(int loop, bool need_wait){
    if(nodes.size() == 0){
        NAC_ERROR("no node attached to this graph, will not execute");
        return;
    }

    if(need_wait){
        in_execution = true;
        for(int i=0;i<loop;i++){
            inference_once();
        }
        in_execution = false;
    } else {
        if(!execution_thread && !in_execution){
            in_execution = true;
            execution_need_exit = false;
            execution_thread = new std::thread([&, this](){
                if(loop<=0){
                    while(!execution_need_exit) // give a chance that infinit loop can stop
                        inference_once();
                }else{
                    while(!execution_need_exit && loop--)
                        inference_once();
                }
            });
        }
    }
}

void * graph::request_workspace_with_dm(int _bytes, data_mm * _dm){
    if(!ws)
        ws.reset(new workspace(_dm));
    NAC_ASSERT(ws->dm() == _dm);
    return ws->request(_bytes);
}

void graph::wait(){
    // block for execution finish
    if(execution_thread && in_execution){
        execution_thread->join();
        delete execution_thread;
        in_execution = false;
    }
}

void graph::attach_node(node * n){
    if(in_execution)
        return;
    n->gr() = this;
    nodes.emplace_back(n);
}
void graph::attach_nodes(node ** n, int num){
    if(in_execution)
        return;
    for(int i=0;i<num;i++){
        n[i]->gr() = this;
        nodes.emplace_back(n[i]);
    }
}
void graph::attach_nodes(std::vector<node*> & n_vec){
    if(in_execution)
        return;
    for(auto & n : n_vec){
        n->gr() = this;
        nodes.emplace_back(n);
    }
}
void graph::release_nodes(){
    if(in_execution)
        return;
    for(auto it = nodes.begin(); it != nodes.end();){
        it = nodes.erase(it);
    }
}
void graph::release_inputs(){
    if(in_execution)
        return;
    for(auto it = inputs.begin(); it != inputs.end();){
        it = inputs.erase(it);
    }
    inputs_list.clear();
}

void graph::feed_input(tensor * t){
    inputs.emplace_back(t);
    inputs_list.push_back(t);
}
void graph::feed_inputs(tensor ** t, int num){
    for(int i=0;i<num;i++){
        inputs.emplace_back(t[i]);
        inputs_list.push_back(t[i]);
    }
}
void graph::feed_inputs(std::vector<tensor*> & t_vec){
    for(auto & t : t_vec){
        inputs.emplace_back(t);
        inputs_list.push_back(t);
    }
}

tensor * graph::output(int idx) const{
    // TODO: aync query output
    // assume output is from the last of nodes
    if(nodes.empty()){
        NAC_ERROR("no op node in current graph, get the output is not valid");
        return nullptr;
    }
    NAC_ASSERT(idx<nodes.size(), "request idx:", idx, " is bigger than total nodes number", nodes.size());
    return nodes.at(idx)->output(idx);;
}

void graph::inference_once(){
    start_observe();
    for(auto & n : nodes)
        n->forward();

    loop_cnt++;
    stop_observe();
}


}