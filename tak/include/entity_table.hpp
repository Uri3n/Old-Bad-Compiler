//
// Created by Diago on 2024-08-11.
//

#ifndef ENTITY_TABLE_HPP
#define ENTITY_TABLE_HPP
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <typedata.hpp>
#include <cassert>
#include <panic.hpp>
#include <util.hpp>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace tak {

    enum entity_flags : uint32_t {
        ENTITY_FLAGS_NONE      = 0UL,
        ENTITY_FOREIGN         = 1UL,
        ENTITY_PLACEHOLDER     = 1UL << 1,
        ENTITY_GLOBAL          = 1UL << 2,
        ENTITY_FOREIGN_C       = 1UL << 3,
        ENTITY_GENPERM         = 1UL << 4,
        ENTITY_GENBASE         = 1UL << 5,
        ENTITY_POSTP_NORECHECK = 1UL << 6,
        ENTITY_INTERNAL        = 1UL << 7,
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

        static std::string llvm_legal_index(uint32_t index);

        ~Symbol() = default;
        Symbol()  = default;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    struct MemberData {
        std::string name;  // name of the member variable or method.
        TypeData    type;  // empty if sym_ref is set.

        ~MemberData() = default;
        MemberData()  = default;
        MemberData(const std::string &name, const TypeData &type)
            : name(name), type(type) {}
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    struct UserType {
        std::vector<std::string> generic_type_names;
        std::vector<MemberData>  members;

        uint32_t    flags = ENTITY_FLAGS_NONE;
        size_t      pos   = 0;  // Only used for error handling
        uint32_t    line  = 1;  // Only used for error handling
        std::string file;       // Only used for error handling

        ~UserType() = default;
        UserType()  = default;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class EntityTable {
    public:

        uint32_t curr_sym_index_ = INVALID_SYMBOL_INDEX;

        //
        // sym_table_ and type_table_ both store pointers to
        // their value types to prevent iterator/pointer invalidation on rehash.
        //

        std::vector<std::string>                               namespace_stack_;
        std::vector<std::unordered_map<std::string, uint32_t>> scope_stack_;
        std::unordered_map<uint32_t, Symbol*>                  sym_table_;
        std::unordered_map<std::string, UserType*>             type_table_;
        std::unordered_map<std::string, TypeData>              type_aliases_;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        void push_scope();
        void pop_scope();
        bool scoped_symbol_exists(const std::string& name);
        bool scoped_symbol_exists_at_current_scope(const std::string& name);
        uint32_t lookup_scoped_symbol(const std::string& name);

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        void     delete_unique_symbol(uint32_t symbol_index);
        uint32_t create_placeholder_symbol(const std::string& name, const std::string& file, size_t src_index, uint32_t line_number);
        Symbol*  lookup_unique_symbol(uint32_t symbol_index);
        Symbol*  create_generic_proc_permutation(const Symbol* original, std::vector<TypeData>&& generic_params);
        Symbol*  create_symbol(
            const std::string& name,
            const std::string& file,
            size_t src_index,
            uint32_t line_number,
            type_kind_t sym_type,
            uint64_t sym_flags,
            const std::optional<TypeData>& data = std::nullopt
        );

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        bool enter_namespace(const std::string& name);
        bool namespace_exists(const std::string& name);
        void leave_namespace();

        std::string namespace_as_string();
        std::string get_canonical_name(std::string name, bool is_symbol);
        std::string get_canonical_type_name(const std::string& name);
        std::string get_canonical_sym_name(const std::string& name);

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        bool create_type(const std::string& name, std::vector<MemberData>&& type_data);
        bool create_placeholder_type(const std::string& name, size_t pos, uint32_t line, const std::string& file);
        bool type_exists(const std::string& name);
        void delete_type(const std::string& name);

        std::vector<MemberData>* lookup_type_members(const std::string& name);
        UserType*                lookup_type(const std::string& name);

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        TypeData lookup_type_alias(const std::string& name);
        bool     create_type_alias(const std::string& name, const TypeData& data);
        bool     type_alias_exists(const std::string& name);
        void     delete_type_alias(const std::string& name);

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        void dump_symbols();
        void dump_types();

        EntityTable(const EntityTable&)            = delete;
        EntityTable& operator=(const EntityTable&) = delete;

        EntityTable()  = default;
        ~EntityTable();
    };
}

#endif //ENTITY_TABLE_HPP
