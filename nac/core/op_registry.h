#ifndef NAC_OP_REGISTRY_H
#define NAC_OP_REGISTRY_H

#include "common.h"
#include "data_mm.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <iostream>
#include <utility>
#include <algorithm>
#include <atomic>
#include <vector>

#define NAC_EXPORT_API_HIDDEN
#include <nac.h>
#undef  NAC_EXPORT_API_HIDDEN

#define NAC_OP_REGISTRY_DECLARE(registry_name) \
    extern "C"  op_registry * get_registry_ ## registry_name ();

#define NAC_OP_REGISTRY_DEFINE(registry_name)  \
    extern "C"  op_registry * get_registry_##registry_name(){ \
        static  op_registry  _registry_##registry_name(#registry_name);   \
        return &_registry_##registry_name; \
    };


#define NAC_OP_REGISTER(registry_name, data_type, op_name, op_class) \
    namespace { \
        static op_register NAC_CONCAT(_##registry_name, __LINE__) \
            (get_registry_##registry_name(), data_type, #op_name, new op_class(#op_name) ) ; \
    }

#define NAC_GET_OP_REGISTRY(registry_name) get_registry_##registry_name()

#define NAC_OP_REGISTER_DM(registry_name, data_type, dm_allocator, dm_deleter, \
                            dm_memcpy_d2h, dm_memcpy_h2d, dm_memcpy_d2d, \
                            dm_unit)  \
    namespace { \
        static op_dm_register NAC_CONCAT(_##registry_name, __LINE__) \
            (NAC_GET_OP_REGISTRY(registry_name), data_type, dm_allocator,   \
                dm_deleter, dm_memcpy_d2h, dm_memcpy_h2d, dm_memcpy_d2d, dm_unit) ; \
    }


namespace nac{
/*
 * device(class op_registry)
 *   |- entry/data_type 1 -> ops(conv/maxpool/...)
 *   |- entry/data_type 2 -> ops
 *   |- ...
 */

class data_mm;
class operator_base;
class compute_device;

class op_registry{
public:
    typedef std::unordered_map<std::string, std::unique_ptr<operator_base>> op_map_type;
    typedef std::pair<int, data_mm>     data_mm_type;

    class op_entry_type{
    public:
        op_entry_type(op_registry * _regi, int _dt);
        op_registry       * registry;
        int                 data_type;  // key

        op_map_type         op_map;
        data_mm             op_dm;
        //std::atomic_uint    ref_cnt;
        unsigned int        ref_cnt;        // TODO: movable usage for atomic type
    };

    static inline std::string gen_full_op_name(const char * _registry_name, const char * _data_type_str, const char * _op_name){
        std::string full_name;
        full_name += _registry_name;
        full_name += "-";
        full_name += _data_type_str;
        full_name += "-";
        full_name += _op_name;
        return full_name;
    }
    static inline std::string gen_full_data_mm_name(const char * _registry_name, const char * _data_type_str){
        std::string full_name;
        full_name += _registry_name;
        full_name += "-";
        full_name += _data_type_str;
        return full_name;
    }

    op_registry(const char * _name);
    ~op_registry();

    int register_op_dm(int data_type, data_mm & dm);
    int register_op(int data_type, std::string op_name, operator_base * op);

    op_entry_type * get_op_entry(const char * entry_name);
    op_entry_type * get_op_entry(int data_type);
    void put_op_entry(op_entry_type * _oe);

    operator_base * get_op(op_entry_type * _oe, const char * _op_name);
    void put_op(operator_base * op);

    const std::vector<char *> &  op_entry_names() const;
    int op_entry_count() const ;

    void assign_working_device(compute_device * _dev);
    compute_device * & working_device() ;

private:
    op_entry_type & try_locate_entry(int data_type);
    //std::unordered_map<std::string, std::unique_ptr<operator_base>>  op_map;
    std::vector<op_entry_type>      op_entries_;
    std::vector<char *>             op_entry_names_;
    compute_device *                dev_;

NAC_RW_ATTR(std::string, default_entry)
NAC_R_ATTR(std::string, name)
DISABLE_COPY_AND_ASSIGN(op_registry)
};


// helper class to register op
struct op_register{
    op_register(op_registry * opr, int data_type, std::string op_name, operator_base * op);
};

//------------------------------------------------------------------------

struct op_dm_register{
    op_dm_register(op_registry * opr, int data_type, 
        data_mm::alloc_t alloc, data_mm::del_t del, 
        data_mm::cpy_t cpy_d2h, data_mm::cpy_t cpy_h2d, data_mm::cpy_t cpy_d2d,
        data_mm::unit_t unit );
};

//------------------------------------------------------------------------

// functions
int insert_registry_entry(op_registry* opr);
op_registry * get_registry_entry(std::string entry_name);
const std::unordered_set<std::string> & supported_op_names();
bool check_op_supported(std::string op_name);

int release_unused_entry(std::string entry_keep, int data_type_keep);
}

#endif