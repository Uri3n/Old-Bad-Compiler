//
// Created by Diago on 2024-07-10.
//

#ifndef TYPEDATA_HPP
#define TYPEDATA_HPP
#include <vector>
#include <optional>
#include <string>
#include <cstdint>
#include <unordered_map>
#include <source_location>
#include <memory>
#include <token.hpp>
#include <variant>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define INVALID_SYMBOL_INDEX 0
#define MAXIMUM_SYMBOL_COUNT 10000 // unused

#define PRIMITIVE_IS_FLOAT(var_type)    \
   (var_type == tak::PRIMITIVE_F32      \
    || var_type == tak::PRIMITIVE_F64   \
)                                       \

#define PRIMITIVE_IS_INTEGRAL(var_type) \
   (var_type == tak::PRIMITIVE_I8       \
    || var_type == tak::PRIMITIVE_U8    \
    || var_type == tak::PRIMITIVE_I16   \
    || var_type == tak::PRIMITIVE_U16   \
    || var_type == tak::PRIMITIVE_I32   \
    || var_type == tak::PRIMITIVE_U32   \
    || var_type == tak::PRIMITIVE_I64   \
    || var_type == tak::PRIMITIVE_U64   \
)                                       \

#define PRIMITIVE_IS_SIGNED(var_type)   \
   (var_type == tak::PRIMITIVE_I8       \
    || var_type == tak::PRIMITIVE_I16   \
    || var_type == tak::PRIMITIVE_I32   \
    || var_type == tak::PRIMITIVE_I64   \
    || var_type == tak::PRIMITIVE_F32   \
    || var_type == tak::PRIMITIVE_F64   \
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
        TYPE_PROC_VARARGS   = 1ULL << 8,
    };

    enum type_kind_t : uint8_t {
        TYPE_KIND_NONE,
        TYPE_KIND_PRIMITIVE,
        TYPE_KIND_PROCEDURE,
        TYPE_KIND_STRUCT,
    };

    enum primitive_t : uint16_t {
        PRIMITIVE_NONE,
        PRIMITIVE_U8,
        PRIMITIVE_I8,
        PRIMITIVE_U16,
        PRIMITIVE_I16,
        PRIMITIVE_U32,
        PRIMITIVE_I32,
        PRIMITIVE_U64,
        PRIMITIVE_I64,
        PRIMITIVE_F32,
        PRIMITIVE_F64,
        PRIMITIVE_BOOLEAN,
        PRIMITIVE_VOID,
    };

    uint16_t    primitive_t_size_bytes(primitive_t type);
    std::string primitive_t_to_string(primitive_t type);

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class TypeData {
    public:
        uint32_t    sym_ref         = INVALID_SYMBOL_INDEX;
        type_kind_t kind            = TYPE_KIND_NONE;
        uint16_t    pointer_depth   = 0;
        uint64_t    flags           = TYPE_FLAGS_NONE;

        std::vector<uint32_t>                  array_lengths;          // Only multiple elements if matrix
        std::shared_ptr<std::vector<TypeData>> parameters  = nullptr;  // Can be null
        std::shared_ptr<TypeData>              return_type = nullptr;  // Can be null
        std::variant<primitive_t, std::string, std::monostate> name;   // name of the TYPE. not whatever is using it.

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        static TypeData get_const_int32();
        static TypeData get_const_uint64();
        static TypeData get_const_double();
        static TypeData get_const_char();
        static TypeData get_const_bool();
        static TypeData get_const_voidptr();
        static TypeData get_const_string();

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        static std::optional<TypeData> get_contained(const TypeData& type);
        static std::optional<TypeData> get_pointer_to(const TypeData& type);
        static std::optional<TypeData> get_lowest_array_type(const TypeData& type);
        static TypeData to_lvalue(const TypeData& type);
        static TypeData to_rvalue(const TypeData& type);

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        std::string to_string(bool include_qualifiers = true, bool include_postfixes = true) const;
        std::string format(uint16_t num_tabs = 0) const;
        bool is_primitive() const;
        bool is_floating_point() const;
        bool is_signed_primitive() const;
        bool is_boolean() const;
        bool is_unsigned_primitive() const;
        bool is_struct_value_type() const;
        bool is_f64() const;
        bool is_f32() const;
        bool is_integer() const;
        bool is_aggregate() const;
        bool is_non_aggregate_pointer() const;
        bool array_has_inferred_sizes() const;
        bool is_invalid_in_inferred_context() const;
        bool is_reassignable() const;
        bool is_returntype_lvalue_eligible() const;
        bool is_bwop_eligible() const;
        bool is_lop_eligible() const;
        bool is_cast_eligible() const;
        bool flip_sign();

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        static bool identical(const TypeData& first, const TypeData& second);
        static bool is_cast_permissible(const TypeData& from, const TypeData& to);
        static bool is_coercion_permissible(TypeData& left, const TypeData& right);
        static bool are_arrays_equivalent(const TypeData& first, const TypeData& second);
        static bool type_promote_non_concrete(TypeData& left, const TypeData& right);
        static bool can_operator_be_applied_to(token_t _operator, const TypeData& type);
        static bool is_arithmetic_eligible(const TypeData& type, token_t _operator);

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        bool operator==(const TypeData& other) const { return identical(*this, other); }
        bool operator!=(const TypeData& other) const { return !identical(*this, other); }
        std::optional<TypeData> operator*()    const { return get_contained(*this); }

        TypeData()  = default;
        ~TypeData() = default;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

}

#endif //TYPEDATA_HPP
