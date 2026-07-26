// AST'yi okunur biçimde yazdıran ziyaretçi.
//
// Ziyaretçi (Visitor) kalıbının ilk gerçek kullanıcısı. Aynı arayüzü sonra
// resolver ve yorumlayıcı da uygulayacak — ağacın kendisi hiç değişmeden.
#pragma once

#include <iosfwd>
#include <string>

#include "ast.hpp"

namespace rs {

class AstDumper final : public ExprVisitor, public StmtVisitor {
public:
    AstDumper(std::ostream& os, const Source& src) : os_(&os), src_(&src) {}

    void dump(const Program& p);

private:
    void visit(const IntLit&) override;
    void visit(const FloatLit&) override;
    void visit(const StrLit&) override;
    void visit(const FStrLit&) override;
    void visit(const BoolLit&) override;
    void visit(const NilLit&) override;
    void visit(const Ident&) override;
    void visit(const SelfExpr&) override;
    void visit(const SuperExpr&) override;
    void visit(const Unary&) override;
    void visit(const Binary&) override;
    void visit(const Logical&) override;
    void visit(const Ternary&) override;
    void visit(const RangeExpr&) override;
    void visit(const Call&) override;
    void visit(const IndexExpr&) override;
    void visit(const Member&) override;
    void visit(const Lambda&) override;
    void visit(const ListLit&) override;
    void visit(const MapLit&) override;

    void visit(const ExprStmt&) override;
    void visit(const AssignStmt&) override;
    void visit(const Block&) override;
    void visit(const IfStmt&) override;
    void visit(const WhileStmt&) override;
    void visit(const ForStmt&) override;
    void visit(const BreakStmt&) override;
    void visit(const ContinueStmt&) override;
    void visit(const PassStmt&) override;
    void visit(const ReturnStmt&) override;
    void visit(const ThrowStmt&) override;
    void visit(const TryStmt&) override;
    void visit(const FnDecl&) override;
    void visit(const ClassDecl&) override;
    void visit(const TraitDecl&) override;
    void visit(const UseStmt&) override;

    void satir(std::string_view etiket, std::string ek = {});
    void alt(const Expr* e);
    void alt(const Stmt* s);
    void tipYaz(const TypeNode* t);
    [[nodiscard]] std::string tipMetni(const TypeNode* t) const;

    std::ostream* os_;
    const Source* src_;
    int derinlik_ = 0;
};

}  // namespace rs
