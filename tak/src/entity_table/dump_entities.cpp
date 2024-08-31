//
// Created by Diago on 2024-08-20.
//

#include <entity_table.hpp>


void
tak::EntityTable::dump_types() {

    if(type_table_.empty()) {
        print("No user-defined types exist.");
        return;
    }

    bold_underline(" -- USER DEFINED TYPES -- ");
    for(const auto &[name, type] : type_table_) {
        bold("~ {}{} ~", name, type->flags & ENTITY_PLACEHOLDER ? " (Placeholder Type)" : "");
        print("  Members:");
        for(size_t i = 0; i < type->members.size(); ++i) {
            print("    {}. {}", i + 1, type->members[i].name);
            print("{}", type->members[i].type.format(1));
        }
    }

    print("");
}

void
tak::EntityTable::dump_symbols() {

    static constexpr std::string_view fmt_sym =
        "~ {}{} ~"
        "\n - Symbol Index:  {}"
        "\n - Line Number:   {}"
        "\n - File Position: {}"
        "\n - Namespace:     {}"
        "\n - Symbol Flags:  {}"
        "\n{}"; //< type data

    bold_underline(" -- SYMBOL TABLE -- ");

    for(const auto &[index, sym] : sym_table_) {

        const std::string symflags = [&]() -> std::string {
            std::string _symflags;
            if(sym->flags & ENTITY_PLACEHOLDER) _symflags += "PLACEHOLDER | ";
            if(sym->flags & ENTITY_GLOBAL)      _symflags += "GLOBAL | ";
            if(sym->flags & ENTITY_FOREIGN)     _symflags += "FOREIGN | ";
            if(sym->flags & ENTITY_INTERNAL)    _symflags += "INTERNAL | ";
            if(sym->flags & ENTITY_CALLCONV_C)  _symflags += "CALLCONV_C | ";
            if(sym->flags & ENTITY_GENPERM)     _symflags += "GENERIC_PERMUTATION |";
            if(sym->flags & ENTITY_GENBASE)     _symflags += "GENERIC_BASE | ";

            if(_symflags.empty()) {
                return "None";
            }
            _symflags.erase(_symflags.size() - 2);
            return _symflags;
        }();

        const std::string generic_names = [&]() -> std::string {
            if(sym->generic_type_names.empty()) {
                return "";
            }

            std::string _generic_names = "[";
            for(const auto& name : sym->generic_type_names) {
                _generic_names += name + ',';
            }

            if(!_generic_names.empty() && _generic_names.back() == ',') {
                _generic_names.erase(_generic_names.size()-1);
            }

            _generic_names += ']';
            return _generic_names;
        }();

        print(fmt_sym,
            sym->name,
            generic_names,
            sym->symbol_index,
            sym->line_number,
            sym->src_pos,
            sym->_namespace,
            symflags,
            sym->type.format()
        );
    }

    print("");
}
