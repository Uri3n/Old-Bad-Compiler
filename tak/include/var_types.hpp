//
// Created by Diago on 2024-07-10.
//

#ifndef SYM_TYPES_HPP
#define SYM_TYPES_HPP
#include <vector>
#include <optional>
#include <string>
#include <cstdint>
#include <unordered_map>
#include <variant>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define INVALID_SYMBOL_INDEX 0
#define MAXIMUM_SYMBOL_COUNT 10000 // unused

#define PRIMITIVE_IS_FLOAT(var_type)    \
   (var_type == VAR_F32                 \
    || var_type == VAR_F64              \
)                                       \

#define PRIMITIVE_IS_INTEGRAL(var_type) \
   (var_type == tak::VAR_I8             \
    || var_type == tak::VAR_U8          \
    || var_type == tak::VAR_I16         \
    || var_type == tak::VAR_U16         \
    || var_type == tak::VAR_I32         \
    || var_type == tak::VAR_U32         \
    || var_type == tak::VAR_I64         \
    || var_type == tak::VAR_U64         \
)                                       \

#define PRIMITIVE_IS_SIGNED(var_type)   \
   (var_type == tak::VAR_I8             \
    || var_type == tak::VAR_I16         \
    || var_type == tak::VAR_I32         \
    || var_type == tak::VAR_I64         \
    || var_type == tak::VAR_F32         \
    || var_type == tak::VAR_F64         \
)                                       \

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace tak {
    enum type_flags : uint64_t {
        TYPE_FLAGS_NONE     = 0ULL,
        TYPE_CONSTANT       = 1ULL,
        TYPE_POINTER        = 1ULL << 1,
        TYPE_ARRAY          = 1ULL << 2,
        TYPE_PROCARG        = 1ULL << 3,
        TYPE_DEFAULT_INIT   = 1ULL << 4,
        TYPE_INFERRED       = 1ULL << 5,
        TYPE_NON_CONCRETE   = 1ULL << 6,
        TYPE_RVALUE         = 1ULL << 7,
        TYPE_UNINITIALIZED  = 1ULL << 8,
    };

    enum entity_flags : uint32_t {
        ENTITY_FLAGS_NONE      = 0UL,
        ENTITY_FOREIGN         = 1UL,
        ENTITY_PLACEHOLDER     = 1UL << 1,
        ENTITY_GLOBAL          = 1UL << 2,
        ENTITY_CALLCONV_C      = 1UL << 3,
        ENTITY_GENPERM         = 1UL << 4,
        ENTITY_GENBASE         = 1UL << 5,
        ENTITY_POSTP_NORECHECK = 1UL << 6,
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

    struct TypeData {
        uint16_t    pointer_depth  = 0;
        uint64_t    flags          = TYPE_FLAGS_NONE;
        type_kind_t kind           = TYPE_KIND_NONE;
        uint32_t    sym_ref        = INVALID_SYMBOL_INDEX;

        std::vector<uint32_t>                  array_lengths;          // Only multiple elements if matrix
        std::shared_ptr<std::vector<TypeData>> parameters  = nullptr;  // Can be null
        std::shared_ptr<TypeData>              return_type = nullptr;  // Can be null

        std::variant<var_t, std::string, std::monostate> name;         // name of the TYPE. not whatever is using it.

        TypeData()  = default;
        ~TypeData() = default;
    };

    struct Symbol {
        uint32_t symbol_index  = INVALID_SYMBOL_INDEX;
        uint32_t flags         = ENTITY_FLAGS_NONE;
        uint32_t line_number   = 0;
        size_t   src_pos       = 0;

        std::string name;
        std::string file;
        std::string _namespace;
        TypeData    type;

        std::vector<std::string> generic_type_names;

        ~Symbol() = default;
        Symbol()  = default;
    };

    struct MemberData {
        std::string name;                           // name of the member variable or method.
        TypeData    type;                           // empty if sym_ref is set.

        ~MemberData() = default;
        MemberData()  = default;
        MemberData(const std::string &name, const TypeData &type)
            : name(name), type(type) {}
    };

    struct UserType {
        std::vector<std::string> generic_type_names;
        std::vector<MemberData>  members;

        size_t      pos   = 0;       // Only used for error handling
        uint32_t    line  = 1;       // Only used for error handling
        uint32_t    flags = ENTITY_FLAGS_NONE;
        std::string file;            // Only used for error handling

        ~UserType() = default;
        UserType()  = default;
    };
}

#endif //SYM_TYPES_HPP
