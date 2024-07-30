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

enum ast_node_t : uint16_t {
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
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct AstNode {
    ast_node_t               type   = NODE_NONE;
    std::optional<AstNode*>  parent = std::nullopt;
    size_t                   pos    = 0;

    virtual ~AstNode() = default;
    explicit AstNode(const ast_node_t type) : type(type) {}
};

struct AstSingletonLiteral final : AstNode {
    std::string value;
    token_t     literal_type = TOKEN_NONE;

    ~AstSingletonLiteral() override = default;
    AstSingletonLiteral() : AstNode(NODE_SINGLETON_LITERAL) {}
};

struct AstBracedExpression final : AstNode {
    std::vector<AstNode*> members;

    ~AstBracedExpression() override;
    AstBracedExpression() : AstNode(NODE_BRACED_EXPRESSION) {}
};

struct AstBinexpr final : AstNode {
    token_t _operator  = TOKEN_NONE;
    AstNode* left_op   = nullptr;
    AstNode* right_op  = nullptr;

    ~AstBinexpr() override;
    AstBinexpr() : AstNode(NODE_BINEXPR) {}
};

struct AstIf final : AstNode {
    std::vector<AstNode*> body;
    AstNode*              condition = nullptr;

    ~AstIf() override;
    AstIf() : AstNode(NODE_IF) {}
};

struct AstElse final : AstNode {
    std::vector<AstNode*> body;

    ~AstElse() override;
    AstElse() : AstNode(NODE_ELSE) {}
};

struct AstBranch final : AstNode {
    std::vector<AstIf*>     conditions;               // consecutive if/else statements
    std::optional<AstElse*> _else = std::nullopt;     // can be null!

    ~AstBranch() override;
    AstBranch() : AstNode(NODE_BRANCH) {}
};

struct AstCase final : AstNode {
    AstSingletonLiteral*  value       = nullptr;
    bool                  fallthrough = false;
    std::vector<AstNode*> body;

    ~AstCase() override;
    AstCase() : AstNode(NODE_CASE) {}
};

struct AstDefault final : AstNode {                  // default case in switches.
    std::vector<AstNode*> body;

    ~AstDefault() override;
    AstDefault() : AstNode(NODE_DEFAULT) {}
};

struct AstSwitch final : AstNode {
    AstNode*              target   = nullptr;
    AstDefault*           _default = nullptr;
    std::vector<AstCase*> cases;

    ~AstSwitch() override;
    AstSwitch() : AstNode(NODE_SWITCH) {}
};

struct AstIdentifier final : AstNode {
    uint32_t symbol_index = 0;                         // INVALID_SYMBOL_INDEX

    ~AstIdentifier() override = default;
    AstIdentifier() : AstNode(NODE_IDENT) {}
};

struct AstMemberAccess final : AstNode {
    AstNode* target = nullptr;
    std::string path;

    ~AstMemberAccess() override;
    AstMemberAccess() : AstNode(NODE_MEMBER_ACCESS) {}
};

struct AstVardecl final : AstNode {
    AstIdentifier*          identifier = nullptr;
    std::optional<AstNode*> init_value = std::nullopt; // Can be null!

    ~AstVardecl() override;
    AstVardecl() : AstNode(NODE_VARDECL) {}
};

struct AstProcdecl final : AstNode {
    AstIdentifier*           identifier = nullptr;
    std::vector<AstVardecl*> parameters;
    std::vector<AstNode*>    body;

    ~AstProcdecl() override;
    AstProcdecl() : AstNode(NODE_PROCDECL) {}
};

struct AstStructdef final : AstNode {
    std::string name;

    ~AstStructdef() override = default;
    AstStructdef() : AstNode(NODE_STRUCT_DEFINITION) {}
};

struct AstCall final : AstNode {
    AstNode*              target = nullptr;
    std::vector<AstNode*> arguments;                   // Can be empty, if the procedure is "paramless".

    ~AstCall() override;
    AstCall() : AstNode(NODE_CALL) {}
};

struct AstFor final : AstNode {
    std::vector<AstNode*>   body;
    std::optional<AstNode*> init      = std::nullopt;
    std::optional<AstNode*> condition = std::nullopt;
    std::optional<AstNode*> update    = std::nullopt;

    ~AstFor() override;
    AstFor() : AstNode(NODE_FOR) {}
};

struct AstUnaryexpr final : AstNode {
    token_t   _operator = TOKEN_NONE;
    AstNode* operand    = nullptr;

    ~AstUnaryexpr() override;
    AstUnaryexpr() : AstNode(NODE_UNARYEXPR) {}
};

struct AstWhile final : AstNode {
    AstNode*              condition = nullptr;
    std::vector<AstNode*> body;

    ~AstWhile() override;
    AstWhile() : AstNode(NODE_WHILE) {}
};

struct AstBlock final : AstNode {
    std::vector<AstNode*> body;

    ~AstBlock() override;
    AstBlock() : AstNode(NODE_BLOCK) {}
};

struct AstDefer final : AstNode {
    AstNode* call = nullptr;                      // Should always be AstCall under the hood.

    ~AstDefer() override;
    AstDefer() : AstNode(NODE_DEFER) {}
};

struct AstDeferIf final : AstNode {
    AstNode* call      = nullptr;                // Should always be AstCall under the hood.
    AstNode* condition = nullptr;

    ~AstDeferIf() override;
    AstDeferIf() : AstNode(NODE_DEFER_IF) {}
};

struct AstSizeof final : AstNode {
    std::variant<TypeData, AstNode*> target = nullptr;

    ~AstSizeof() override;
    AstSizeof() : AstNode(NODE_SIZEOF) {}
};

struct AstDoWhile final : AstNode {
    AstNode*              condition = nullptr;
    std::vector<AstNode*> body;

    ~AstDoWhile() override;
    AstDoWhile() : AstNode(NODE_DOWHILE) {}
};

struct AstSubscript final : AstNode {
    AstNode* operand = nullptr;                   // The thing being subscripted.
    AstNode* value   = nullptr;                   // The index value.

    ~AstSubscript() override;
    AstSubscript() : AstNode(NODE_SUBSCRIPT) {}
};

struct AstNamespaceDecl final : AstNode {
    std::string            full_path;
    std::vector<AstNode*>  children;

    ~AstNamespaceDecl() override;
    AstNamespaceDecl() : AstNode(NODE_NAMESPACEDECL) {}
};

struct AstCast final : AstNode {
    AstNode* target = nullptr;
    TypeData type;

    ~AstCast() override;
    AstCast() : AstNode(NODE_CAST) {}
};

struct AstRet final : AstNode {
    std::optional<AstNode*> value = std::nullopt; // can be null!

    ~AstRet() override;
    AstRet() : AstNode(NODE_RET) {}
};

struct AstTypeAlias final : AstNode {
    std::string name;

    ~AstTypeAlias() override = default;
    AstTypeAlias() : AstNode(NODE_TYPE_ALIAS) {}
};

struct AstEnumdef final : AstNode {
    AstNamespaceDecl* _namespace = nullptr;
    AstTypeAlias*     alias      = nullptr;

    ~AstEnumdef() override;
    AstEnumdef() : AstNode(NODE_ENUM_DEFINITION) {}
};

struct AstCont final : AstNode {
    ~AstCont() override = default;
    AstCont() : AstNode(NODE_CONT) {}
};

struct AstBrk final : AstNode {
    ~AstBrk() override = default;
    AstBrk() : AstNode(NODE_BRK) {}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif //AST_TYPES_HPP
