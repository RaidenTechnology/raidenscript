// İsim çözümleyici.
//
// Yorumlayıcıdan ÖNCE çalışır ve şunları yapar (SPEC §2, §4):
//   1. Kapsam zinciri kurar; bir kapsamdaki İLK atama tanımlamadır
//   2. 'outer x = ...' dış kapsamı hedefler — dışarıda yoksa hata
//   3. Kapanışın yakaladığı değişkenleri işaretler (upvalue)
//   4. Tanımsız isimleri yakalar
//   5. Bildirilmemiş sınıf alanı için uyarı üretir
//   6. self/super'ın metot dışında kullanımını yakalar
//
// Sonuçlar AST'ye YAZILMAZ; yan tabloda tutulur. Böylece ağaç değişmez ve
// aynı ağaç üzerinde farklı geçişler bağımsız çalışabilir.
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "ast.hpp"
#include "diag.hpp"
#include "source.hpp"

namespace rs {

struct Resolution {
    enum class Kind : std::uint8_t {
        Local,    // aynı fonksiyon içinde, 'depth' kapsam yukarıda
        Upvalue,  // bir fonksiyon sınırının ötesinde — kapanış yakalar
        Global,   // en dış kapsam
        Host,     // hiçbir yerde bulunamadı; host'un sağladığı varsayılıyor
    };
    Kind kind = Kind::Global;
    int depth = 0;
};

class Resolver final : public ExprVisitor, public StmtVisitor {
public:
    Resolver(const Source& src, Diagnostics& diag) : src_(&src), diag_(&diag) {}

    void resolve(const Program& p);

    [[nodiscard]] const std::unordered_map<const Expr*, Resolution>& resolutions() const noexcept {
        return res_;
    }

private:
    struct Var {
        Span span;
        bool used = false;
        bool captured = false;
    };

    struct Scope {
        std::unordered_map<std::string, Var> vars;
        bool functionBoundary = false;  // fonksiyon/lambda gövdesi mi
    };

    // --- kapsam yönetimi ---
    void push(bool functionBoundary = false);
    void pop();
    void declare(const std::string& ad, Span span);
    [[nodiscard]] bool declaredHere(const std::string& ad) const;
    void lookup(const Expr* node, const std::string& ad, Span span, bool yazma);

    // Bir blok içindeki fn/class/trait adlarını gövdelerden ÖNCE tanımlar.
    // Böylece sıralamadan bağımsız birbirlerini çağırabilirler.
    void hoist(const std::vector<StmtPtr>& stmts);

    void resolveExpr(const Expr* e);
    void resolveStmt(const Stmt* s);
    void resolveType(const TypeNode* t);
    void resolveParams(const std::vector<Param>& ps);

    // --- ziyaretçi ---
    void visit(const IntLit&) override {}
    void visit(const FloatLit&) override {}
    void visit(const StrLit&) override {}
    void visit(const FStrLit&) override;
    void visit(const BoolLit&) override {}
    void visit(const NilLit&) override {}
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

    const Source* src_;
    Diagnostics* diag_;

    std::vector<Scope> scopes_;
    std::unordered_map<const Expr*, Resolution> res_;

    int fnDepth_ = 0;        // içinde bulunduğumuz fonksiyon sayısı
    int loopDepth_ = 0;      // break/continue geçerliliği
    bool inMethod_ = false;  // self/super geçerliliği
    bool inClassWithBase_ = false;
    const ClassDecl* currentClass_ = nullptr;
};

}  // namespace rs
