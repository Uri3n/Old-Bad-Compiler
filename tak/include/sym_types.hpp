//
// Created by Diago on 2024-07-10.
//

#ifndef SYM_TYPES_HPP
#define SYM_TYPES_HPP
#include <vector>
#include <optional>
#include <string>
#include <cstdint>
#include <variant>


#define INVALID_SYMBOL_INDEX 0

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum sym_flags : uint16_t {
    SYM_FLAGS_NONE          = 0UL,
    SYM_IS_CONSTANT         = 1UL,
    SYM_IS_FOREIGN          = 1UL << 1,
    SYM_IS_POINTER          = 1UL << 2,
    SYM_IS_GLOBAL           = 1UL << 3,
    SYM_IS_ARRAY            = 1UL << 4,
    SYM_IS_PROCARG          = 1UL << 5,
    SYM_DEFAULT_INITIALIZED = 1UL << 6,
};

enum sym_t : uint8_t {
    SYM_NONE,
    SYM_VARIABLE,
    SYM_PROCEDURE,
    SYM_STRUCT,
};

enum var_t : uint16_t {
    VAR_NONE,
    VAR_U8,
    VAR_I8,
    VAR_U16,
    VAR_I16,
    VAR_U32,
    VAR_I32,
    VAR_U64,
    VAR_I64,
    VAR_F32,
    VAR_F64,
    VAR_BOOLEAN,
    VAR_VOID, // for void^ only.
    VAR_USER_DEFINED_STRUCT,
    VAR_USER_DEFINED_ENUM,
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct type_data {

    uint16_t pointer_depth   = 0;
    uint16_t flags           = SYM_FLAGS_NONE;
    sym_t    sym_type        = SYM_NONE;

    std::vector<uint32_t>                   array_lengths;          // Only multiple elements if matrix
    std::shared_ptr<std::vector<type_data>> parameters  = nullptr;  // Can be null, only used for procedures.
    std::shared_ptr<type_data>              return_type = nullptr;  // Can be null, only used for procedures.

    std::variant<var_t, std::string, std::monostate> name;          // name of the TYPE. not whatever is using it.

    type_data()  = default;
    ~type_data() = default;
};

struct symbol {
    uint32_t symbol_index  = INVALID_SYMBOL_INDEX;
    uint32_t line_number   = 0;
    size_t   src_pos       = 0;

    std::string name;
    type_data   type;

    ~symbol() = default;
    symbol()  = default;
};

struct member_data {

    std::string name;
    type_data   type;

    ~member_data() = default;
    member_data()  = default;
    member_data(const std::string &name, const type_data &type)
        : name(name),
          type(type) {
    }
};

#endif //SYM_TYPES_HPP
