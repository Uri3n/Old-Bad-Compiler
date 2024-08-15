//
// Created by Diago on 2024-07-02.
//

#ifndef AST_TYPES_HPP
#define AST_TYPES_HPP
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <var_types.hpp>
#include <token.hpp>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define NODE_VALID_SUBEXPRESSION(node_type)        \
 (node_type == tak::NODE_CALL                      \
    || node_type == tak::NODE_IDENT                \
    || node_type == tak::NODE_BINEXPR              \
    || node_type == tak::NODE_SINGLETON_LITERAL    \
    || node_type == tak::NODE_UNARYEXPR            \
    || node_type == tak::NODE_BRACED_EXPRESSION    \
    || node_type == tak::NODE_CAST                 \
    || node_type == tak::NODE_SUBSCRIPT            \
    || node_type == tak::NODE_CAST                 \
    || node_type == tak::NODE_MEMBER_ACCESS        \
    || node_type == tak::NODE_SIZEOF               \
)                                                  \

#define NODE_VALID_AT_TOPLEVEL(node_type)          \
   (node_type == tak::NODE_VARDECL                 \
    || node_type == tak::NODE_STRUCT_DEFINITION    \
    || node_type == tak::NODE_NAMESPACEDECL        \
    || node_type == tak::NODE_PROCDECL             \
    || node_type == tak::NODE_INCLUDE_STMT         \
    || node_type == tak::NODE_ENUM_DEFINITION      \
    || node_type == tak::NODE_TYPE_ALIAS           \
)                                                  \

#define NODE_EXPR_NEVER_NEEDS_TERMINAL(node_type)  \
   (node_type == tak::NODE_PROCDECL                \
    || node_type == tak::NODE_BRANCH               \
    || node_type == tak::NODE_IF                   \
    || node_type == tak::NODE_ELSE                 \
    || node_type == tak::NODE_FOR                  \
    || node_type == tak::NODE_WHILE                \
    || node_type == tak::NODE_PROCDECL             \
    || node_type == tak::NODE_SWITCH               \
    || node_type == tak::NODE_NAMESPACEDECL        \
    || node_type == tak::NODE_BLOCK                \
    || node_type == tak::NODE_STRUCT_DEFINITION    \
    || node_type == tak::NODE_ENUM_DEFINITION      \
)                                                  \

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace tak {

    enum node_t : uint16_t {
        NODE_NONE,
        NODE_VARDECL,
        NODE_PROCDECL,
        NODE_BINEXPR,
        NODE_UNARYEXPR,
        NODE_IDENT,
        NODE_BRANCH,
        NODE_IF,
        NODE_ELSE,
        NODE_FOR,
        NODE_SWITCH,
        NODE_CASE,
        NODE_DEFAULT,
        NODE_WHILE,
        NODE_DOWHILE,
        NODE_BLOCK,
        NODE_CALL,
        NODE_BRK,
        NODE_CONT,
        NODE_RET,
        NODE_DEFER,
        NODE_DEFER_IF,
        NODE_SIZEOF,
        NODE_SINGLETON_LITERAL,
        NODE_BRACED_EXPRESSION,
        NODE_STRUCT_DEFINITION,
        NODE_ENUM_DEFINITION,
        NODE_SUBSCRIPT,
        NODE_NAMESPACEDECL,
        NODE_CAST,
        NODE_TYPE_ALIAS,
        NODE_MEMBER_ACCESS,
        NODE_INCLUDE_STMT,
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    struct AstNode {
        node_t                   type   = NODE_NONE;
        std::optional<AstNode*>  parent = std::nullopt;
        std::string file;
        size_t      src_pos = 0;
        uint32_t    line    = 1;

        virtual ~AstNode() = default;
        explicit AstNode(
            const node_t type,
            const size_t src_pos,
            const uint32_t line,
            const std::string& file
        ) : type(type), file(file), src_pos(src_pos), line(line) {}
    };

    struct AstSingletonLiteral final : AstNode {
        std::string value;
        token_t     literal_type = TOKEN_NONE;

        ~AstSingletonLiteral() override = default;
        AstSingletonLiteral(
            const size_t src_pos,
            const uint32_t line,
            const std::string& file
        ) : AstNode(NODE_SINGLETON_LITERAL, src_pos, line, file) {}
    };

    struct AstBracedExpression final : AstNode {
        std::vector<AstNode*> members;

        ~AstBracedExpression() override;
        AstBracedExpression(
            const size_t src_pos,
            const uint32_t line,
            const std::string& file
        ) : AstNode(NODE_BRACED_EXPRESSION, src_pos, line, file) {}
    };

    struct AstBinexpr final : AstNode {
        token_t _operator  = TOKEN_NONE;
        AstNode* left_op   = nullptr;
        AstNode* right_op  = nullptr;

        ~AstBinexpr() override;
        AstBinexpr(
            const size_t src_pos,
            const uint32_t line,
            const std::string& file
        ) : AstNode(NODE_BINEXPR, src_pos, line, file) {}
    };

    struct AstIf final : AstNode {
        std::vector<AstNode*> body;
        AstNode*              condition = nullptr;

        ~AstIf() override;
        AstIf(
            const size_t src_pos,
            const uint32_t line,
            const std::string& file
        ) : AstNode(NODE_IF, src_pos, line, file) {}
    };

    struct AstElse final : AstNode {
        std::vector<AstNode*> body;

        ~AstElse() override;
        AstElse(
            const size_t src_pos,
            const uint32_t line,
            const std::string& file
        ) : AstNode(NODE_ELSE, src_pos, line, file) {}
    };

    struct AstBranch final : AstNode {
        std::vector<AstIf*>     conditions;               // consecutive if/else statements
        std::optional<AstElse*> _else = std::nullopt;     // can be null!

        ~AstBranch() override;
        AstBranch(
            const size_t src_pos,
            const uint32_t line,
            const std::string& file
        ) : AstNode(NODE_BRANCH, src_pos, line, file) {}
    };

    struct AstCase final : AstNode {
        AstSingletonLiteral*  value       = nullptr;
        bool                  fallthrough = false;
        std::vector<AstNode*> body;

        ~AstCase() override;
        AstCase(
            const size_t src_pos,
            const uint32_t line,
            const std::string& file
        ) : AstNode(NODE_CASE, src_pos, line, file) {}
    };

    struct AstDefault final : AstNode {                  // default case in switches.
        std::vector<AstNode*> body;

        ~AstDefault() override;
        AstDefault(
            const size_t src_pos,
            const uint32_t line,
            const std::string& file
        ) : AstNode(NODE_DEFAULT, src_pos, line, file) {}
    };

    struct AstSwitch final : AstNode {
        AstNode*              target   = nullptr;
        AstDefault*           _default = nullptr;
        std::vector<AstCase*> cases;

        ~AstSwitch() override;
        AstSwitch(
            const size_t src_pos,
            const uint32_t line,
            const std::string& file
        ) : AstNode(NODE_SWITCH, src_pos, line, file) {}
    };

    struct AstIdentifier final : AstNode {
        uint32_t symbol_index = 0;                         // INVALID_SYMBOL_INDEX

        ~AstIdentifier() override = default;
        AstIdentifier(
            const size_t src_pos,
            const uint32_t line,
            const std::string& file
        ) : AstNode(NODE_IDENT, src_pos, line, file) {}
    };

    struct AstMemberAccess final : AstNode {
        AstNode* target = nullptr;
        std::string path;

        ~AstMemberAccess() override;
        AstMemberAccess(
            const size_t src_pos,
            const uint32_t line,
            const std::string& file
        ) : AstNode(NODE_MEMBER_ACCESS, src_pos, line, file) {}
    };

    struct AstVardecl final : AstNode {
        AstIdentifier*          identifier = nullptr;
        std::optional<AstNode*> init_value = std::nullopt; // Can be null!

        ~AstVardecl() override;
        AstVardecl(
            const size_t src_pos,
            const uint32_t line,
            const std::string& file
        ) : AstNode(NODE_VARDECL, src_pos, line, file) {}
    };

    struct AstProcdecl final : AstNode {
        AstIdentifier*           identifier = nullptr;
        std::vector<AstVardecl*> parameters;
        std::vector<AstNode*>    children;

        ~AstProcdecl() override;
        AstProcdecl(
            const size_t src_pos,
            const uint32_t line,
            const std::string& file
        ) : AstNode(NODE_PROCDECL, src_pos, line, file) {}
    };

    struct AstStructdef final : AstNode {
        std::string name;

        ~AstStructdef() override = default;
        AstStructdef(
            const size_t src_pos,
            const uint32_t line,
            const std::string& file
        ) : AstNode(NODE_STRUCT_DEFINITION, src_pos, line, file) {}
    };

    struct AstCall final : AstNode {
        AstNode*              target = nullptr;
        std::vector<AstNode*> arguments;                   // Can be empty, if the procedure is "paramless".

        ~AstCall() override;
        AstCall(
            const size_t src_pos,
            const uint32_t line,
            const std::string& file
        ) : AstNode(NODE_CALL, src_pos, line, file) {}
    };

    struct AstFor final : AstNode {
        std::vector<AstNode*>   body;
        std::optional<AstNode*> init      = std::nullopt;
        std::optional<AstNode*> condition = std::nullopt;
        std::optional<AstNode*> update    = std::nullopt;

        ~AstFor() override;
        AstFor(
            const size_t src_pos,
            const uint32_t line,
            const std::string& file
        ) : AstNode(NODE_FOR, src_pos, line, file) {}
    };

    struct AstUnaryexpr final : AstNode {
        token_t   _operator = TOKEN_NONE;
        AstNode* operand    = nullptr;

        ~AstUnaryexpr() override;
        AstUnaryexpr(
            const size_t src_pos,
            const uint32_t line,
            const std::string& file
        ) : AstNode(NODE_UNARYEXPR, src_pos, line, file) {}
    };

    struct AstWhile final : AstNode {
        AstNode*              condition = nullptr;
        std::vector<AstNode*> body;

        ~AstWhile() override;
        AstWhile(
            const size_t src_pos,
            const uint32_t line,
            const std::string& file
        ) : AstNode(NODE_WHILE, src_pos, line, file) {}
    };

    struct AstBlock final : AstNode {
        std::vector<AstNode*> children;

        ~AstBlock() override;
        AstBlock(
            const size_t src_pos,
            const uint32_t line,
            const std::string& file
        ) : AstNode(NODE_BLOCK, src_pos, line, file) {}
    };

    struct AstDefer final : AstNode {
        AstNode* call = nullptr;                      // Should always be AstCall under the hood.

        ~AstDefer() override;
        AstDefer(
            const size_t src_pos,
            const uint32_t line,
            const std::string& file
        ) : AstNode(NODE_DEFER, src_pos, line, file) {}
    };

    struct AstDeferIf final : AstNode {
        AstNode* call      = nullptr;                // Should always be AstCall under the hood.
        AstNode* condition = nullptr;

        ~AstDeferIf() override;
        AstDeferIf(
            const size_t src_pos,
            const uint32_t line,
            const std::string& file
        ) : AstNode(NODE_DEFER_IF, src_pos, line, file) {}
    };

    struct AstSizeof final : AstNode {
        std::variant<TypeData, AstNode*> target = nullptr;

        ~AstSizeof() override;
        AstSizeof(
            const size_t src_pos,
            const uint32_t line,
            const std::string& file
        ) : AstNode(NODE_SIZEOF, src_pos, line, file) {}
    };

    struct AstDoWhile final : AstNode {
        AstNode*              condition = nullptr;
        std::vector<AstNode*> body;

        ~AstDoWhile() override;
        AstDoWhile(
            const size_t src_pos,
            const uint32_t line,
            const std::string& file
        ) : AstNode(NODE_DOWHILE, src_pos, line, file) {}
    };

    struct AstSubscript final : AstNode {
        AstNode* operand = nullptr;                   // The thing being subscripted.
        AstNode* value   = nullptr;                   // The index value.

        ~AstSubscript() override;
        AstSubscript(
            const size_t src_pos,
            const uint32_t line,
            const std::string& file
        ) : AstNode(NODE_SUBSCRIPT, src_pos, line, file) {}
    };

    struct AstNamespaceDecl final : AstNode {
        std::string            full_path;
        std::vector<AstNode*>  children;

        ~AstNamespaceDecl() override;
        AstNamespaceDecl(
            const size_t src_pos,
            const uint32_t line,
            const std::string& file
        ) : AstNode(NODE_NAMESPACEDECL, src_pos, line, file) {}
    };

    struct AstCast final : AstNode {
        AstNode* target = nullptr;
        TypeData type;

        ~AstCast() override;
        AstCast(
            const size_t src_pos,
            const uint32_t line,
            const std::string& file
        ) : AstNode(NODE_CAST, src_pos, line, file) {}
    };

    struct AstRet final : AstNode {
        std::optional<AstNode*> value = std::nullopt; // can be null!

        ~AstRet() override;
        AstRet(
            const size_t src_pos,
            const uint32_t line,
            const std::string& file
        ) : AstNode(NODE_RET, src_pos, line, file) {}
    };

    struct AstTypeAlias final : AstNode {
        std::string name;

        ~AstTypeAlias() override = default;
        AstTypeAlias(
            const size_t src_pos,
            const uint32_t line,
            const std::string& file
        ) : AstNode(NODE_TYPE_ALIAS, src_pos, line, file) {}
    };

    struct AstIncludeStmt final : AstNode {
        std::string name;

        ~AstIncludeStmt() override = default;
        AstIncludeStmt(
            const size_t src_pos,
            const uint32_t line,
            const std::string& file
        ) : AstNode(NODE_INCLUDE_STMT, src_pos, line, file) {}
    };

    struct AstEnumdef final : AstNode {
        AstNamespaceDecl* _namespace = nullptr;
        AstTypeAlias*     alias      = nullptr;

        ~AstEnumdef() override;
        AstEnumdef(
            const size_t src_pos,
            const uint32_t line,
            const std::string& file
        ) : AstNode(NODE_ENUM_DEFINITION, src_pos, line, file) {}
    };

    struct AstCont final : AstNode {
        ~AstCont() override = default;
        AstCont(
            const size_t src_pos,
            const uint32_t line,
            const std::string& file
        ) : AstNode(NODE_CONT, src_pos, line, file) {}
    };

    struct AstBrk final : AstNode {
        ~AstBrk() override = default;
        AstBrk(
            const size_t src_pos,
            const uint32_t line,
            const std::string& file
        ) : AstNode(NODE_BRK, src_pos, line, file) {}
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
}
#endif //AST_TYPES_HPP
