//
// Created by Diago on 2024-08-11.
//

#ifndef SYMBOL_TABLE_HPP
#define SYMBOL_TABLE_HPP
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <var_types.hpp>
#include <cassert>
#include <panic.hpp>
#include <support.hpp>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace tak {

    class EntityTable {
    public:

        uint32_t curr_sym_index_ = INVALID_SYMBOL_INDEX;

        //
        // You might be wondering: why do some of these members, e.g. sym_table_ and type_table_
        // store pointers instead of the values directly? This is due to pointer/reference
        // invalidation. There are instances where we are holding a pointer to an object and must be certain
        // that its memory location will not change. I COULD also switch to std::list, which offers total iterator and reference
        // safety, but the lookup times will be much slower. I am considering it though. We'll have to see.
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

        EntityTable(const EntityTable&)            = delete;
        EntityTable& operator=(const EntityTable&) = delete;

        EntityTable()  = default;
        ~EntityTable();
    };
}

#endif //SYMBOL_TABLE_HPP
