//
// Created by Diago on 2024-08-17.
//

#include <typedata.hpp>
#include <panic.hpp>
#include <cassert>


std::string
tak::TypeData::format(const TypeData& type, const uint16_t num_tabs) {

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
        if(type.flags & TYPE_CONSTANT)      _flags += "CONSTANT | ";
        if(type.flags & TYPE_POINTER)       _flags += "POINTER | ";
        if(type.flags & TYPE_ARRAY)         _flags += "ARRAY | ";
        if(type.flags & TYPE_PROCARG)       _flags += "PROCEDURE_ARGUMENT | ";
        if(type.flags & TYPE_NON_CONCRETE)  _flags += "NON_CONCRETE | ";
        if(type.flags & TYPE_DEFAULT_INIT)  _flags += "DEFAULT INITIALIZED | ";
        if(type.flags & TYPE_INFERRED)      _flags += "INFERRED";
        if(type.flags & TYPE_PROC_VARARGS)  _flags += "VARIADIC_ARGUMENTS | ";

        if(_flags.size() >= 2 && _flags[_flags.size()-2] == '|') {
            _flags.erase(_flags.size()-2);
        }
        if(_flags.empty()) {
            return "None";
        }
        return _flags;
    }();


    const std::string sym_t_str = [&]() -> std::string {
        if(type.kind == TYPE_KIND_PROCEDURE) return "Procedure";
        if(type.kind == TYPE_KIND_PRIMITIVE)  return "Variable";
        if(type.kind == TYPE_KIND_STRUCT)    return "Struct";
        return "None";
    }();


    const std::string type_name_str = [&]() -> std::string {
        if(const auto* is_var = std::get_if<primitive_t>(&type.name)) {
            return primitive_t_to_string(*is_var);
        }
        else if(const auto* is_struct = std::get_if<std::string>(&type.name)) {
            return fmt("{} (User Defined Struct)", *is_struct);
        }
        return "Procedure";
    }();


    const std::string return_type_data = [&]() -> std::string {
        if(type.return_type != nullptr) {
            return TypeData::to_string(*type.return_type);
        }
        return "N/A";
    }();


    const std::string param_data = [&]() -> std::string {
        if(type.parameters != nullptr) {
            std::string _params;
            for(const auto& param : *type.parameters) {
                _params += fmt("{}, ", TypeData::to_string(param));
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
        for(const auto& len : type.array_lengths) {
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
        type.pointer_depth,
        tabs,
        type.array_lengths.size(),
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
tak::TypeData::to_string(const TypeData& type, const bool include_qualifiers, const bool include_postfixes) {

    std::string buffer;
    bool is_proc    = false;
    bool _is_struct = false;

    if(include_qualifiers) {
        if(type.flags & TYPE_INFERRED) return "Invalid Type";
        if(type.flags & TYPE_CONSTANT) buffer += "const ";
        if(type.flags & TYPE_RVALUE)   buffer += "rvalue ";
    }

    if(const auto* is_primitive = std::get_if<primitive_t>(&type.name)) {
        buffer += primitive_t_to_string(*is_primitive);
    }
    else if(const auto* is_struct = std::get_if<std::string>(&type.name)) {
        buffer += *is_struct;
        _is_struct = true;
    }
    else {
        is_proc = true;
        buffer += "proc";
    }


    if(_is_struct && type.parameters != nullptr) {
        buffer += '[';
        for(const auto& param : *type.parameters) {
            buffer += TypeData::to_string(param) + ',';
        }

        if(buffer.back() == ',') buffer.pop_back();
        buffer += ']';
    }

    if(include_postfixes) {
        if(type.flags & TYPE_POINTER) {
            for(uint16_t i = 0; i < type.pointer_depth; i++) {
                buffer += '^';
            }
        }

        if(type.flags & TYPE_ARRAY) {
            for(size_t i = 0 ; i < type.array_lengths.size(); i++) {
                if(!type.array_lengths[i]) buffer += "[]";
                else buffer += fmt("[{}]", type.array_lengths[i]);
            }
        }
    }


    if(is_proc) {
        buffer += '(';
        if(type.parameters != nullptr) {
            for(const auto& param : *type.parameters) {
                buffer += TypeData::to_string(param) + ',';
            }
        }

        if(type.flags & TYPE_PROC_VARARGS) buffer += "...";
        if(buffer.back() == ',') buffer.pop_back();

        buffer += ')';
        buffer += " -> ";

        if(type.return_type != nullptr) buffer += TypeData::to_string(*type.return_type);
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

bool
tak::TypeData::is_primitive(const TypeData& type) {
    const auto* prim_t = std::get_if<primitive_t>(&type.name);
    return prim_t != nullptr
        && *prim_t != PRIMITIVE_VOID
        && !(type.flags & TYPE_POINTER)
        && !(type.flags & TYPE_ARRAY);
}

bool
tak::TypeData::is_floating_point(const TypeData& type) {
    const auto* prim_t = std::get_if<primitive_t>(&type.name);
    return prim_t != nullptr
        && (*prim_t == PRIMITIVE_F32 || *prim_t == PRIMITIVE_F64)
        && !(type.flags & TYPE_POINTER)
        && !(type.flags & TYPE_ARRAY);
}

bool
tak::TypeData::is_signed_primitive(const TypeData& type) {
    const auto* prim_t = std::get_if<primitive_t>(&type.name);
    return prim_t != nullptr
        && PRIMITIVE_IS_SIGNED(*prim_t)
        && !(type.flags & TYPE_POINTER)
        && !(type.flags & TYPE_ARRAY);
}

bool
tak::TypeData::is_integer(const TypeData& type) {
    const auto* prim_t = std::get_if<primitive_t>(&type.name);
    return prim_t != nullptr
        && *prim_t != PRIMITIVE_VOID
        && (*prim_t != PRIMITIVE_F32 && *prim_t != PRIMITIVE_F64)
        && !(type.flags & TYPE_POINTER)
        && !(type.flags & TYPE_ARRAY);
}

bool
tak::TypeData::is_unsigned_primitive(const TypeData& type) {
    const auto* prim_t = std::get_if<primitive_t>(&type.name);
    return prim_t != nullptr
        && !PRIMITIVE_IS_SIGNED(*prim_t)
        && !(type.flags & TYPE_POINTER)
        && !(type.flags & TYPE_ARRAY);
}

bool
tak::TypeData::is_struct_value_type(const TypeData& type) {
    return type.kind == TYPE_KIND_STRUCT
        && !(type.flags & TYPE_POINTER)
        && !(type.flags & TYPE_ARRAY);
}

bool
tak::TypeData::is_aggregate(const TypeData& type) {
    return type.flags & TYPE_ARRAY
    || (type.kind == TYPE_KIND_STRUCT && !(type.flags & TYPE_POINTER));
}

bool
tak::TypeData::is_non_aggregate_pointer(const TypeData& type) {
    return type.flags & TYPE_POINTER && !(type.flags & TYPE_ARRAY);
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