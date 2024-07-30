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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define INVALID_SYMBOL_INDEX 0
#define MAXIMUM_SYMBOL_COUNT 10000 // unused

#define PRIMITIVE_IS_SIGNED(var_type) (var_type == VAR_I8   \
    || var_type == VAR_I16                                  \
    || var_type == VAR_I32                                  \
    || var_type == VAR_I64                                  \
    || var_type == VAR_F32                                  \
    || var_type == VAR_F64                                  \
)                                                           \

#define PRIMITIVE_IS_FLOAT(var_type) (var_type == VAR_F32   \
    || var_type == VAR_F64                                  \
)                                                           \

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum type_flags : uint64_t {
    TYPE_FLAGS_NONE     = 0ULL,
    TYPE_CONSTANT       = 1ULL,
    TYPE_FOREIGN        = 1ULL << 1,
    TYPE_POINTER        = 1ULL << 2,
    TYPE_GLOBAL         = 1ULL << 3,
    TYPE_ARRAY          = 1ULL << 4,
    TYPE_PROCARG        = 1ULL << 5,
    TYPE_DEFAULT_INIT   = 1ULL << 6,
    TYPE_INFERRED       = 1ULL << 7,
    TYPE_NON_CONCRETE   = 1ULL << 8,
    TYPE_RVALUE         = 1ULL << 9,
    TYPE_UNINITIALIZED  = 1ULL << 10,
};

enum type_kind_t : uint8_t {
    TYPE_KIND_NONE,
    TYPE_KIND_VARIABLE,
    TYPE_KIND_PROCEDURE,
    TYPE_KIND_STRUCT,
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
    VAR_VOID,
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct type_data {
    uint16_t    pointer_depth  = 0;
    uint64_t    flags          = TYPE_FLAGS_NONE;
    type_kind_t kind           = TYPE_KIND_NONE;

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
