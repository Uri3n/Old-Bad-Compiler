//
// Created by Diago on 2024-08-17.
//

#include <typedata.hpp>
#include <panic.hpp>
#include <cassert>


std::string
tak::TypeData::format(const uint16_t num_tabs) const {

    static constexpr std::string_view fmt_type =
        "{} - Symbol Type:   {}"
        "\n{} - Type Flags:    {}"
        "\n{} - Pointer Depth: {}"
        "\n{} - Matrix Depth:  {}"
        "\n{} - Array Lengths: {}"
        "\n{} - Type Name:     {}"
        "\n{} - Return Type:   {}"
        "\n{} - Parameters:    {}\n";

    std::string tabs;
    for(uint16_t i = 0; i < num_tabs; i++) {
        tabs += '\t';
    }


    const std::string flags = [&]() -> std::string {
        std::string _flags;
        if(this->flags & TYPE_CONSTANT)      _flags += "CONSTANT | ";
        if(this->flags & TYPE_POINTER)       _flags += "POINTER | ";
        if(this->flags & TYPE_ARRAY)         _flags += "ARRAY | ";
        if(this->flags & TYPE_PROCARG)       _flags += "PROCEDURE_ARGUMENT | ";
        if(this->flags & TYPE_NON_CONCRETE)  _flags += "NON_CONCRETE | ";
        if(this->flags & TYPE_DEFAULT_INIT)  _flags += "DEFAULT INITIALIZED | ";
        if(this->flags & TYPE_INFERRED)      _flags += "INFERRED";
        if(this->flags & TYPE_PROC_VARARGS)  _flags += "VARIADIC_ARGUMENTS | ";

        if(_flags.size() >= 2 && _flags[_flags.size()-2] == '|') {
            _flags.erase(_flags.size()-2);
        }
        if(_flags.empty()) {
            return "None";
        }
        return _flags;
    }();


    const std::string sym_t_str = [&]() -> std::string {
        if(this->kind == TYPE_KIND_PROCEDURE) return "Procedure";
        if(this->kind == TYPE_KIND_PRIMITIVE)  return "Variable";
        if(this->kind == TYPE_KIND_STRUCT)    return "Struct";
        return "None";
    }();


    const std::string type_name_str = [&]() -> std::string {
        if(const auto* is_var = std::get_if<primitive_t>(&this->name)) {
            return primitive_t_to_string(*is_var);
        }
        else if(const auto* is_struct = std::get_if<std::string>(&this->name)) {
            return fmt("{} (User Defined Struct)", *is_struct);
        }
        return "Procedure";
    }();


    const std::string return_type_data = [&]() -> std::string {
        if(this->return_type != nullptr) {
            return this->return_type->to_string();
        }
        return "N/A";
    }();


    const std::string param_data = [&]() -> std::string {
        if(this->parameters != nullptr) {
            std::string _params;
            for(const auto& param : *this->parameters) {
                _params += fmt("{}, ", param.to_string());
            }
            if(_params.size() > 2 && _params[_params.size()-2] == ',') {
                _params.erase(_params.size()-2);
            }
            return _params;
        }

        return "N/A";
    }();


    const std::string array_lengths = [&]() -> std::string {
        std::string _lengths;
        for(const auto& len : this->array_lengths) {
            _lengths += std::to_string(len) + ',';
        }
        if(!array_lengths.empty() && array_lengths.back() == ',') {
            _lengths.pop_back();
        }
        return _lengths;
    }();


    return fmt(fmt_type,
        tabs,
        sym_t_str,
        tabs,
        flags,
        tabs,
        this->pointer_depth,
        tabs,
        this->array_lengths.size(),
        tabs,
        array_lengths.empty() ? "N/A" : array_lengths,
        tabs,
        type_name_str,
        tabs,
        return_type_data,
        tabs,
        param_data
    );
}


std::string
tak::TypeData::to_string(const bool include_qualifiers, const bool include_postfixes) const {
    std::string buffer;
    bool is_proc    = false;
    bool _is_struct = false;

    if(include_qualifiers) {
        if(this->flags & TYPE_INFERRED) return "Invalid Type";
        if(this->flags & TYPE_CONSTANT) buffer += "const ";
        if(this->flags & TYPE_RVALUE)   buffer += "rvalue ";
    }

    if(const auto* is_primitive = std::get_if<primitive_t>(&this->name)) {
        buffer += primitive_t_to_string(*is_primitive);
    }
    else if(const auto* is_struct = std::get_if<std::string>(&this->name)) {
        buffer += *is_struct;
        _is_struct = true;
    }
    else {
        is_proc = true;
        buffer += "proc";
    }


    if(_is_struct && this->parameters != nullptr) {
        buffer += '[';
        for(const auto& param : *this->parameters) {
            buffer += param.to_string() + ',';
        }

        if(buffer.back() == ',') buffer.pop_back();
        buffer += ']';
    }

    if(include_postfixes) {
        if(this->flags & TYPE_POINTER) {
            for(uint16_t i = 0; i < this->pointer_depth; i++) {
                buffer += '^';
            }
        }

        if(this->flags & TYPE_ARRAY) {
            for(size_t i = 0 ; i < this->array_lengths.size(); i++) {
                if(!this->array_lengths[i]) buffer += "[]";
                else buffer += fmt("[{}]", this->array_lengths[i]);
            }
        }
    }


    if(is_proc) {
        buffer += '(';
        if(this->parameters != nullptr) {
            for(const auto& param : *this->parameters) {
                buffer += param.to_string() + ',';
            }
        }

        if(this->flags & TYPE_PROC_VARARGS) buffer += "...";
        if(buffer.back() == ',') buffer.pop_back();

        buffer += ')';
        buffer += " -> ";

        if(this->return_type != nullptr) buffer += this->return_type->to_string();
        else buffer += "void";
    }

    return buffer;
}

tak::TypeData
tak::TypeData::get_const_bool() {
    TypeData const_bool;
    const_bool.name   = PRIMITIVE_BOOLEAN;
    const_bool.kind   = TYPE_KIND_PRIMITIVE;
    const_bool.flags  = TYPE_CONSTANT | TYPE_RVALUE;

    return const_bool;
}

tak::TypeData
tak::TypeData::get_const_voidptr() {
    TypeData const_ptr;
    const_ptr.name          = PRIMITIVE_VOID;
    const_ptr.pointer_depth = 1;
    const_ptr.flags        |= TYPE_POINTER;

    return const_ptr;
}

tak::TypeData
tak::TypeData::get_const_string() {
    TypeData const_ptr;
    const_ptr.name          = PRIMITIVE_I8;
    const_ptr.pointer_depth = 1;
    const_ptr.flags        |= TYPE_POINTER;

    return const_ptr;
}

tak::TypeData
tak::TypeData::get_const_int32() {
    TypeData const_int;
    const_int.kind   = TYPE_KIND_PRIMITIVE;
    const_int.name   = PRIMITIVE_I32;
    const_int.flags |= TYPE_CONSTANT | TYPE_RVALUE;

    return const_int;
}

tak::TypeData
tak::TypeData::get_const_uint64() {
    TypeData const_int;
    const_int.kind   = TYPE_KIND_PRIMITIVE;
    const_int.name   = PRIMITIVE_U64;
    const_int.flags |= TYPE_CONSTANT | TYPE_RVALUE;

    return const_int;
}

tak::TypeData
tak::TypeData::get_const_double() {
    TypeData const_int;
    const_int.kind   = TYPE_KIND_PRIMITIVE;
    const_int.name   = PRIMITIVE_F64;
    const_int.flags |= TYPE_CONSTANT | TYPE_RVALUE;

    return const_int;
}

tak::TypeData
tak::TypeData::get_const_char() {
    TypeData const_int;
    const_int.kind   = TYPE_KIND_PRIMITIVE;
    const_int.name   = PRIMITIVE_I8;
    const_int.flags |= TYPE_CONSTANT | TYPE_RVALUE;

    return const_int;
}

tak::TypeData
tak::TypeData::to_lvalue(const TypeData& type) {
    TypeData lval = type;
    lval.flags   &= ~TYPE_RVALUE;
    return lval;
}

tak::TypeData
tak::TypeData::to_rvalue(const TypeData& type) {
    TypeData rval = type;
    rval.flags   |= TYPE_RVALUE;
    return rval;
}

bool
tak::TypeData::is_primitive() const {
    const auto* prim_t = std::get_if<primitive_t>(&this->name);
    return prim_t != nullptr
        && *prim_t != PRIMITIVE_VOID
        && !(this->flags & TYPE_POINTER)
        && !(this->flags & TYPE_ARRAY);
}

bool
tak::TypeData::is_floating_point() const {
    const auto* prim_t = std::get_if<primitive_t>(&this->name);
    return prim_t != nullptr
        && (*prim_t == PRIMITIVE_F32 || *prim_t == PRIMITIVE_F64)
        && !(this->flags & TYPE_POINTER)
        && !(this->flags & TYPE_ARRAY);
}

bool
tak::TypeData::is_signed_primitive() const {
    const auto* prim_t = std::get_if<primitive_t>(&this->name);
    return prim_t != nullptr
        && PRIMITIVE_IS_SIGNED(*prim_t)
        && !(this->flags & TYPE_POINTER)
        && !(this->flags & TYPE_ARRAY);
}

bool
tak::TypeData::is_boolean() const {
    const auto* prim_t = std::get_if<primitive_t>(&this->name);
    return prim_t != nullptr
        && *prim_t == PRIMITIVE_BOOLEAN
        && !(this->flags & TYPE_POINTER)
        && !(this->flags & TYPE_ARRAY);
}

bool
tak::TypeData::is_integer() const {
    const auto* prim_t = std::get_if<primitive_t>(&this->name);
    return prim_t != nullptr
        && *prim_t != PRIMITIVE_VOID
        && *prim_t != PRIMITIVE_F32
        && *prim_t != PRIMITIVE_F64
        && !(this->flags & TYPE_POINTER)
        && !(this->flags & TYPE_ARRAY);
}

bool
tak::TypeData::is_unsigned_primitive() const {
    const auto* prim_t = std::get_if<primitive_t>(&this->name);
    return prim_t != nullptr
        && !PRIMITIVE_IS_SIGNED(*prim_t)
        && !(this->flags & TYPE_POINTER)
        && !(this->flags & TYPE_ARRAY);
}

bool
tak::TypeData::is_struct_value_type() const {
    return this->kind == TYPE_KIND_STRUCT
        && !(this->flags & TYPE_POINTER)
        && !(this->flags & TYPE_ARRAY);
}

bool
tak::TypeData::is_f64() const {
    const auto* prim_t = std::get_if<primitive_t>(&this->name);
    return prim_t != nullptr
        && *prim_t == PRIMITIVE_F64
        && !(this->flags & TYPE_POINTER)
        && !(this->flags & TYPE_ARRAY);
}

bool
tak::TypeData::is_f32() const {
    const auto* prim_t = std::get_if<primitive_t>(&this->name);
    return prim_t != nullptr
        && *prim_t == PRIMITIVE_F32
        && !(this->flags & TYPE_POINTER)
        && !(this->flags & TYPE_ARRAY);
}

bool
tak::TypeData::is_aggregate() const {
    return this->flags & TYPE_ARRAY
    || (this->kind == TYPE_KIND_STRUCT && !(this->flags & TYPE_POINTER));
}

bool
tak::TypeData::is_non_aggregate_pointer() const {
    return this->flags & TYPE_POINTER && !(this->flags & TYPE_ARRAY);
}

std::string
tak::primitive_t_to_string(const primitive_t type) {
    switch(type) {
        case PRIMITIVE_NONE:     return "None";
        case PRIMITIVE_U8:       return "u8";
        case PRIMITIVE_I8:       return "i8";
        case PRIMITIVE_U16:      return "u16";
        case PRIMITIVE_I16:      return "i16";
        case PRIMITIVE_U32:      return "u32";
        case PRIMITIVE_I32:      return "i32";
        case PRIMITIVE_U64:      return "u64";
        case PRIMITIVE_I64:      return "i64";
        case PRIMITIVE_F32:      return "f32";
        case PRIMITIVE_F64:      return "f64";
        case PRIMITIVE_BOOLEAN:  return "bool";
        case PRIMITIVE_VOID:     return "void";
        default: break;
    }

    panic("var_t_to_string: default case reached.");
}

uint16_t
tak::primitive_t_size_bytes(const primitive_t type) {
    switch(type) {          // Assumes type is NOT a pointer.
        case PRIMITIVE_BOOLEAN:
        case PRIMITIVE_U8:
        case PRIMITIVE_I8:      return 1;
        case PRIMITIVE_U16:
        case PRIMITIVE_I16:     return 2;
        case PRIMITIVE_U32:
        case PRIMITIVE_I32:     return 4;
        case PRIMITIVE_U64:
        case PRIMITIVE_I64:     return 8;
        case PRIMITIVE_F32:     return 4;
        case PRIMITIVE_F64:     return 8;
        default: break;
    }

    panic("primitive_t_size_bytes: non size-convertible var_t passed as argument.");
}