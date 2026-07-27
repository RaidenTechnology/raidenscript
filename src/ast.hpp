// Soyut sözdizimi ağacı.
//
// Temsil kararı: sanal sınıf hiyerarşisi + Ziyaretçi (Visitor).
// std::variant + std::visit alternatifi düşünüldü ama 30+ alternatifle özyinelemeli
// variant hem derleme süresini şişiriyor hem de hata mesajlarını okunmaz kılıyor.
// accept() gövdelerini 30 kez elle yazmamak için CRTP kullanılıyor.
#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "source.hpp"
#include "token.hpp"

namespace rs {

struct Expr;
struct Stmt;
struct TypeNode;

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;
using TypePtr = std::unique_ptr<TypeNode>;

// ---------------------------------------------------------------- tip notasyonu

// Faz 1'de ayrıştırılır ama DENETLENMEZ (SPEC §10). Faz 2'de zorlanacak.
struct TypeNode {
    std::string name;              // "int", "list", "Oyuncu", "fn"
    std::vector<TypePtr> args;     // list[int] → args = [int]
    bool nullable = false;         // T?
    bool isFn = false;             // fn(int) -> bool
    std::vector<TypePtr> fnParams;
    TypePtr fnRet;
    Span span;
};

struct Param {
    std::string name;
    TypePtr type;          // nullable
    ExprPtr defaultValue;  // nullable
    Span span;
};

// ---------------------------------------------------------------- ifadeler

struct IntLit;   struct FloatLit; struct StrLit;   struct FStrLit;
struct BoolLit;  struct NilLit;   struct Ident;    struct SelfExpr;
struct SuperExpr; struct Unary;   struct Binary;   struct Logical;
struct Ternary;  struct RangeExpr; struct Call;    struct IndexExpr;
struct Member;   struct Lambda;   struct ListLit;  struct MapLit;

struct ExprVisitor {
    virtual ~ExprVisitor() = default;
    virtual void visit(const IntLit&) = 0;
    virtual void visit(const FloatLit&) = 0;
    virtual void visit(const StrLit&) = 0;
    virtual void visit(const FStrLit&) = 0;
    virtual void visit(const BoolLit&) = 0;
    virtual void visit(const NilLit&) = 0;
    virtual void visit(const Ident&) = 0;
    virtual void visit(const SelfExpr&) = 0;
    virtual void visit(const SuperExpr&) = 0;
    virtual void visit(const Unary&) = 0;
    virtual void visit(const Binary&) = 0;
    virtual void visit(const Logical&) = 0;
    virtual void visit(const Ternary&) = 0;
    virtual void visit(const RangeExpr&) = 0;
    virtual void visit(const Call&) = 0;
    virtual void visit(const IndexExpr&) = 0;
    virtual void visit(const Member&) = 0;
    virtual void visit(const Lambda&) = 0;
    virtual void visit(const ListLit&) = 0;
    virtual void visit(const MapLit&) = 0;
};

struct Expr {
    Span span;
    virtual ~Expr() = default;
    virtual void accept(ExprVisitor&) const = 0;
};

// CRTP: accept() gövdesi tek yerde yazılır, her düğüm miras alır.
template <class D>
struct ExprNode : Expr {
    void accept(ExprVisitor& v) const override { v.visit(static_cast<const D&>(*this)); }
};

struct IntLit : ExprNode<IntLit> { std::int64_t value = 0; };
struct FloatLit : ExprNode<FloatLit> { double value = 0.0; };
struct StrLit : ExprNode<StrLit> { std::string value; bool raw = false; };

// f-string. Gövde HAM tutulur; `parts` alt-lexer'la çözülmüş parçalar
// (metin, ifade, metin, ...). SPEC §9 kural 3.
struct FStrLit : ExprNode<FStrLit> {
    struct Part {
        std::string literal;  // düz metin parçası (expr boşsa geçerli)
        ExprPtr expr;         // {...} içindeki ifade
        std::string format;   // ':.2f' gibi biçim eki (Faz 2'de yorumlanacak)
    };
    std::vector<Part> parts;
    std::string raw;
};

struct BoolLit : ExprNode<BoolLit> { bool value = false; };
struct NilLit : ExprNode<NilLit> {};
struct Ident : ExprNode<Ident> { std::string name; };
struct SelfExpr : ExprNode<SelfExpr> {};
struct SuperExpr : ExprNode<SuperExpr> {};

struct Unary : ExprNode<Unary> {
    Tok op = Tok::Minus;   // Minus, Plus, Tilde, KwNot, KwAwait
    ExprPtr operand;
};

struct Binary : ExprNode<Binary> {
    Tok op = Tok::Plus;
    ExprPtr left, right;
};

// 'and' / 'or' ayrı düğüm: kısa devre değerlendirme Binary'den farklı çalışır.
struct Logical : ExprNode<Logical> {
    Tok op = Tok::KwAnd;
    ExprPtr left, right;
};

struct Ternary : ExprNode<Ternary> {
    ExprPtr thenExpr, cond, elseExpr;   // <then> if <cond> else <else>
};

struct RangeExpr : ExprNode<RangeExpr> {
    ExprPtr lo, hi;         // ikisi de nullptr OLABİLİR (açık uçlu dilim)
    bool inclusive = false; // ..=
};

struct Call : ExprNode<Call> {
    ExprPtr callee;
    std::vector<ExprPtr> args;
};

struct IndexExpr : ExprNode<IndexExpr> {
    ExprPtr object;
    ExprPtr index;   // RangeExpr ise dilimdir
};

struct Member : ExprNode<Member> {
    ExprPtr object;
    std::string name;
    bool safe = false;   // ?.
};

struct Lambda : ExprNode<Lambda> {
    std::vector<Param> params;
    ExprPtr bodyExpr;    // (x) => x * 2
    StmtPtr bodyBlock;   // (x) => { ... }   — ikisinden biri dolu
};

struct ListLit : ExprNode<ListLit> { std::vector<ExprPtr> items; };
struct MapLit : ExprNode<MapLit> {
    std::vector<std::pair<ExprPtr, ExprPtr>> entries;
};

// ---------------------------------------------------------------- deyimler

struct ExprStmt;  struct AssignStmt; struct Block;     struct IfStmt;
struct WhileStmt; struct ForStmt;    struct BreakStmt; struct ContinueStmt;
struct PassStmt;  struct ReturnStmt; struct ThrowStmt; struct TryStmt;
struct FnDecl;    struct ClassDecl;  struct TraitDecl; struct ImportStmt;

struct StmtVisitor {
    virtual ~StmtVisitor() = default;
    virtual void visit(const ExprStmt&) = 0;
    virtual void visit(const AssignStmt&) = 0;
    virtual void visit(const Block&) = 0;
    virtual void visit(const IfStmt&) = 0;
    virtual void visit(const WhileStmt&) = 0;
    virtual void visit(const ForStmt&) = 0;
    virtual void visit(const BreakStmt&) = 0;
    virtual void visit(const ContinueStmt&) = 0;
    virtual void visit(const PassStmt&) = 0;
    virtual void visit(const ReturnStmt&) = 0;
    virtual void visit(const ThrowStmt&) = 0;
    virtual void visit(const TryStmt&) = 0;
    virtual void visit(const FnDecl&) = 0;
    virtual void visit(const ClassDecl&) = 0;
    virtual void visit(const TraitDecl&) = 0;
    virtual void visit(const ImportStmt&) = 0;
};

struct Stmt {
    Span span;
    virtual ~Stmt() = default;
    virtual void accept(StmtVisitor&) const = 0;
};

template <class D>
struct StmtNode : Stmt {
    void accept(StmtVisitor& v) const override { v.visit(static_cast<const D&>(*this)); }
};

struct ExprStmt : StmtNode<ExprStmt> { ExprPtr expr; };

struct AssignStmt : StmtNode<AssignStmt> {
    ExprPtr target;             // Ident, Member veya IndexExpr
    Tok op = Tok::Assign;       // '=' veya bileşik atama
    TypePtr declType;           // 'x: int = 5' — nullable
    ExprPtr value;
    bool outer = false;         // 'outer x = ...'
};

struct Block : StmtNode<Block> { std::vector<StmtPtr> stmts; };

struct IfStmt : StmtNode<IfStmt> {
    ExprPtr cond;
    StmtPtr thenBranch;
    StmtPtr elseBranch;   // elif → iç içe IfStmt; nullable
};

struct WhileStmt : StmtNode<WhileStmt> { ExprPtr cond; StmtPtr body; };

struct ForStmt : StmtNode<ForStmt> {
    std::vector<std::string> vars;   // for k, v in ...
    ExprPtr iterable;
    StmtPtr body;
};

struct BreakStmt : StmtNode<BreakStmt> {};
struct ContinueStmt : StmtNode<ContinueStmt> {};
struct PassStmt : StmtNode<PassStmt> {};
struct ReturnStmt : StmtNode<ReturnStmt> { ExprPtr value; };   // nullable
struct ThrowStmt : StmtNode<ThrowStmt> { ExprPtr value; };

struct TryStmt : StmtNode<TryStmt> {
    StmtPtr body;
    std::string catchVar;
    StmtPtr catchBody;
    StmtPtr finallyBody;   // nullable
};

struct Decorator {
    std::string name;
    std::vector<ExprPtr> args;
    Span span;
};

struct FnDecl : StmtNode<FnDecl> {
    std::string name;
    std::vector<Param> params;
    TypePtr retType;      // nullable
    StmtPtr body;
    bool isAsync = false;
    std::vector<Decorator> decorators;
};

struct ClassDecl : StmtNode<ClassDecl> {
    struct Field {
        std::string name;
        TypePtr type;
        ExprPtr init;   // nullable
        Span span;
    };
    std::string name;
    std::vector<std::string> bases;   // taban sınıf + trait'ler
    std::vector<Field> fields;
    std::vector<std::unique_ptr<FnDecl>> methods;
};

struct TraitDecl : StmtNode<TraitDecl> {
    struct Sig {
        std::string name;
        std::vector<Param> params;
        TypePtr retType;
        Span span;
    };
    std::string name;
    std::vector<Sig> methods;
};

struct ImportStmt : StmtNode<ImportStmt> {
    std::string path;      // "std.math" veya "github.com/x/y"
    std::string alias;     // 'as j' — boş olabilir
    std::string version;   // '@ "v0.3.1"' — boş olabilir
    bool isStd = false;    // noktalı ad mı, tırnaklı yol mu
    bool isInclude = false;   // 'include' ile mi geldi — derleme aninda cozulur
};

// Bir kaynak dosyanın tamamı.
struct Program {
    std::vector<StmtPtr> stmts;
};

}  // namespace rs
