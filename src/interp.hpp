// Ağaç yürüyen yorumlayıcı.
//
// Akış kontrolü (return/break/continue) ve RaidenScript istisnaları C++
// istisnalarıyla taşınıyor — tree-walk mimarisinde en temiz yol budur; her
// visit() metodunun bir "sinyal" döndürmesine gerek kalmıyor.
//
// Performans Faz 3'ün (bytecode VM) işi. Buradaki hedef DOĞRULUK.
#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ast.hpp"
#include "diag.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "source.hpp"
#include "value.hpp"

namespace rs {

// --- akış sinyalleri ---
struct ReturnSignal { Value value; };
struct BreakSignal {};
struct ContinueSignal {};

// RaidenScript istisnası (throw ile fırlatılan veya çalışma zamanı hatası).
struct ScriptThrow {
    Value value;
    Span span;
};

class Interpreter final : public ExprVisitor, public StmtVisitor {
public:
    Interpreter(const Source& src, Diagnostics& diag);
    ~Interpreter() override;

    // Programı çalıştırır. Yakalanmamış istisna olursa false döner.
    bool run(const Program& p);

    // REPL için: tek bir deyimi çalıştırıp son ifade değerini döndürür.
    [[nodiscard]] Value lastValue() const { return son_; }
        // REPL her tur icin yeni bir Diagnostics kullaniyor.
    void setDiagnostics(Diagnostics& d) { diag_ = &d; }

    // --- gömme (Faz 4 / C API) ---

    // Host'a giden çağrı. Betikteki 'game.spawnBullet(x, y)' buraya düşer.
    //
    // DİZE KANALI. Sınırın yalnızca sayı taşıması bilinçli bir karardı: sayı
    // kopyalanır, sahiplik sorusu doğmaz. Ama metinsiz bir uygulama arayüzü
    // yazılamıyor — IBAN, açıklama, hata mesajı hep dize. Çözüm dizeleri
    // sayıya SIKIŞTIRMAK değil, argümanın yanında ayrı bir alanda taşımak:
    // sayı yolu bir bayt değişmiyor, mevcut host'lar etkilenmiyor, ve
    // "şu double aslında bir tutamak" gibi sessiz bir sözleşme doğmuyor.
    struct HostArg {
        double sayi = 0;
        const std::string* dize = nullptr;  // nullptr => argüman sayı
    };
    struct HostSonuc {
        double sayi = 0;
        bool dizeMi = false;
        std::string dize;
    };
    using HostFn = std::function<HostSonuc(const std::string& modul, const std::string& fn,
                                           const std::vector<HostArg>& args)>;

    // Host'un sağladığı modül/fonksiyon çiftleri. 'include <modul>' görüldüğünde
    // nil yerine bu tablodan bir harita bağlanır. eval'den ÖNCE kurulmalı.
    void setHostBridge(std::vector<std::pair<std::string, std::string>> kayitli, HostFn cb);

    // Global kapsamdaki bir fonksiyonu adıyla çağırır. Bulunamazsa ya da betik
    // istisna fırlatırsa false döner; sebep diag_'a yazılır.
    bool callGlobal(const std::string& ad, std::vector<Value>& args, Value& out);

private:
    // 'include' için host haritası kurar; modül kayıtlı değilse nullopt.
    std::optional<Value> hostModul(const std::string& modul);
    // --- çekirdek ---
    Value eval(const Expr* e);
    void exec(const Stmt* s);
    void execBlock(const std::vector<StmtPtr>& stmts, std::shared_ptr<Environment> env);

    [[noreturn]] void hata(Span span, const std::string& mesaj, const std::string& ipucu = {});
    [[nodiscard]] Value makeError(const std::string& mesaj);

    // --- yardımcılar ---
    Value cagir(const Value& callee, std::vector<Value>& args, Span span);
    Value cagirFn(const std::shared_ptr<FnObj>& fn, std::vector<Value>& args, Span span,
                  const Value* self);
    Value ikili(Tok op, const Value& a, const Value& b, Span span);
    Value uyeOku(const Value& obj, const std::string& ad, Span span, bool guvenli);
    Value indeksOku(const Value& obj, const Value& idx, Span span);
    void indeksYaz(const Value& obj, const Value& idx, Value v, Span span);
    Value yerlesikMetot(const Value& obj, const std::string& ad, Span span);
    std::vector<Value> gezilebilir(const Value& v, Span span);
    Value araligiAc(const RangeExpr& r, Span span);

    void preludeKur();
    void yerlesikSinifKur();
    Value stdModul(const std::string& yol);

    // --- ziyaretçi: ifadeler ---
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

    // --- ziyaretçi: deyimler ---
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
    void visit(const ImportStmt&) override;

    const Source* src_;
    Diagnostics* diag_;

    // --- gömme köprüsü ---
    std::vector<std::pair<std::string, std::string>> hostKayit_;  // (modül, fonksiyon)
    HostFn hostFn_;

    std::shared_ptr<Environment> globals_;
    std::shared_ptr<Environment> env_;
    Value son_;                       // en son değerlendirilen ifade
    Value self_;                      // metot içindeyken geçerli örnek
    std::shared_ptr<ClassObj> superOwner_;  // 'super' çözümü için

    std::shared_ptr<ClassObj> errorClass_;

    // Prelude, RaidenScript kaynağı olarak yazılıyor; AST'si programın ömrü
    // boyunca yaşamalı, o yüzden burada tutuluyor.
    std::unique_ptr<Source> preludeSrc_;
    std::unique_ptr<Diagnostics> preludeDiag_;
    std::unique_ptr<Program> preludeProg_;
};

}  // namespace rs
