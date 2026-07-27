#include "resolver.hpp"

#include <array>
#include <string_view>

namespace rs {
namespace {

// SPEC §7.2 — 'import' gerektirmeden hazır isimler. Kasıtlı olarak dar.
constexpr std::array<std::string_view, 10> PRELUDE{
    "print", "len", "type", "int", "float", "str", "bool", "range", "assert", "Error",
};

bool preludeMi(const std::string& ad) {
    for (const auto p : PRELUDE) {
        if (p == ad) {
            return true;
        }
    }
    return false;
}

}  // namespace

// ---------------------------------------------------------------- kapsam

void Resolver::push(bool functionBoundary) {
    scopes_.push_back(Scope{{}, functionBoundary});
}

void Resolver::pop() {
    // Kullanılmayan yerel değişkenler için uyarı — en dış kapsamda değil,
    // çünkü global'ler host tarafından okunuyor olabilir.
    if (scopes_.size() > 1) {
        for (const auto& [ad, v] : scopes_.back().vars) {
            if (!v.used && ad != "self" && ad != "_") {
                diag_->warning(v.span, "'" + ad + "' tanımlandı ama hiç kullanılmadı",
                               "gerekmiyorsa sil, bilerek yok sayıyorsan adını '_' yap");
            }
        }
    }
    scopes_.pop_back();
}

void Resolver::declare(const std::string& ad, Span span) {
    if (scopes_.empty()) {
        push();
    }
    auto& s = scopes_.back().vars;
    const auto it = s.find(ad);
    if (it != s.end()) {
        return;  // aynı kapsamda yeniden atama — tanımlama değil
    }
    s.emplace(ad, Var{span, false, false});
}

bool Resolver::declaredHere(const std::string& ad) const {
    return !scopes_.empty() && scopes_.back().vars.count(ad) > 0;
}

// İsmi kapsam zincirinde arar ve çözümü yan tabloya yazar.
void Resolver::lookup(const Expr* node, const std::string& ad, Span span, bool yazma) {
    int derinlik = 0;
    bool fonksiyonSiniriGecildi = false;

    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it, ++derinlik) {
        const auto found = it->vars.find(ad);
        if (found != it->vars.end()) {
            // SADECE okuma "kullanım" sayılır. Atama saymazsa, hiç okunmayan
            // ama sürekli yazılan bir değişken "kullanılıyor" gibi görünürdü —
            // oysa tam olarak yakalamak istediğimiz ölü kod bu.
            if (!yazma) {
                found->second.used = true;
            }

            Resolution r;
            r.depth = derinlik;
            if (derinlik == static_cast<int>(scopes_.size()) - 1 && scopes_.size() > 1) {
                r.kind = Resolution::Kind::Global;
            } else if (fonksiyonSiniriGecildi) {
                r.kind = Resolution::Kind::Upvalue;
                found->second.captured = true;
            } else {
                r.kind = Resolution::Kind::Local;
            }
            if (node != nullptr) {
                res_[node] = r;
            }
            return;
        }
        if (it->functionBoundary) {
            fonksiyonSiniriGecildi = true;
        }
    }

    if (preludeMi(ad)) {
        if (node != nullptr) {
            res_[node] = Resolution{Resolution::Kind::Global, 0};
        }
        return;
    }

    // Hiçbir yerde yok. Yazma ise global tanımlama sayılır; okuma ise
    // host'un sağladığı varsayılır (gömülü dilde host global enjekte eder).
    if (yazma) {
        declare(ad, span);
        return;
    }

    if (node != nullptr) {
        res_[node] = Resolution{Resolution::Kind::Host, 0};
    }
    diag_->warning(span, "'" + ad + "' hiçbir yerde tanımlanmamış",
                   "host tarafından sağlandığı varsayılıyor; yazım hatası değilse sorun yok");
}

void Resolver::hoist(const std::vector<StmtPtr>& stmts) {
    for (const auto& s : stmts) {
        if (const auto* f = dynamic_cast<const FnDecl*>(s.get())) {
            declare(f->name, f->span);
        } else if (const auto* c = dynamic_cast<const ClassDecl*>(s.get())) {
            declare(c->name, c->span);
        } else if (const auto* t = dynamic_cast<const TraitDecl*>(s.get())) {
            declare(t->name, t->span);
        }
    }
}

void Resolver::resolveExpr(const Expr* e) {
    if (e != nullptr) {
        e->accept(*this);
    }
}

void Resolver::resolveStmt(const Stmt* s) {
    if (s != nullptr) {
        s->accept(*this);
    }
}

void Resolver::resolveType(const TypeNode*) {
    // Faz 1: tipler ayrıştırılır ama denetlenmez (SPEC §10). Faz 2'de burası dolacak.
}

void Resolver::resolveParams(const std::vector<Param>& ps) {
    for (const auto& p : ps) {
        if (p.defaultValue) {
            resolveExpr(p.defaultValue.get());  // varsayılan DIŞ kapsamda değerlendirilir
        }
    }
    for (const auto& p : ps) {
        declare(p.name, p.span);
    }
}

void Resolver::resolve(const Program& p) {
    push();  // global kapsam
    hoist(p.stmts);
    for (const auto& s : p.stmts) {
        resolveStmt(s.get());
    }
    scopes_.pop_back();  // global'de kullanılmadı uyarısı verme
}

// ---------------------------------------------------------------- ifadeler

void Resolver::visit(const Ident& e) { lookup(&e, e.name, e.span, false); }

void Resolver::visit(const SelfExpr& e) {
    if (!inMethod_) {
        diag_->error(e.span, "'self' yalnızca metot içinde kullanılabilir");
    }
}

void Resolver::visit(const SuperExpr& e) {
    if (!inMethod_) {
        diag_->error(e.span, "'super' yalnızca metot içinde kullanılabilir");
    } else if (!inClassWithBase_) {
        diag_->error(e.span, "bu sınıfın taban sınıfı yok",
                     "'super' kullanmak için 'class X(Taban):' biçiminde bir taban bildir");
    }
}

void Resolver::visit(const FStrLit& e) {
    for (const auto& p : e.parts) {
        resolveExpr(p.expr.get());
    }
}

void Resolver::visit(const Unary& e) { resolveExpr(e.operand.get()); }

void Resolver::visit(const Binary& e) {
    resolveExpr(e.left.get());
    resolveExpr(e.right.get());
}

void Resolver::visit(const Logical& e) {
    resolveExpr(e.left.get());
    resolveExpr(e.right.get());
}

void Resolver::visit(const Ternary& e) {
    resolveExpr(e.cond.get());
    resolveExpr(e.thenExpr.get());
    resolveExpr(e.elseExpr.get());
}

void Resolver::visit(const RangeExpr& e) {
    resolveExpr(e.lo.get());
    resolveExpr(e.hi.get());
}

void Resolver::visit(const Call& e) {
    resolveExpr(e.callee.get());
    for (const auto& a : e.args) {
        resolveExpr(a.get());
    }
}

void Resolver::visit(const IndexExpr& e) {
    resolveExpr(e.object.get());
    resolveExpr(e.index.get());
}

void Resolver::visit(const Member& e) {
    resolveExpr(e.object.get());
    // Alan adı çalışma zamanında çözülür — dinamik nesne modeli.
}

void Resolver::visit(const Lambda& e) {
    ++fnDepth_;
    push(true);
    resolveParams(e.params);
    if (e.bodyExpr) {
        resolveExpr(e.bodyExpr.get());
    } else {
        resolveStmt(e.bodyBlock.get());
    }
    pop();
    --fnDepth_;
}

void Resolver::visit(const ListLit& e) {
    for (const auto& x : e.items) {
        resolveExpr(x.get());
    }
}

void Resolver::visit(const MapLit& e) {
    for (const auto& [k, v] : e.entries) {
        resolveExpr(k.get());
        resolveExpr(v.get());
    }
}

// ---------------------------------------------------------------- deyimler

void Resolver::visit(const ExprStmt& s) { resolveExpr(s.expr.get()); }

void Resolver::visit(const AssignStmt& s) {
    resolveExpr(s.value.get());
    resolveType(s.declType.get());

    const auto* hedef = dynamic_cast<const Ident*>(s.target.get());
    if (hedef == nullptr) {
        resolveExpr(s.target.get());  // a.b = ... veya a[i] = ...
        return;
    }

    if (s.outer) {
        // 'outer x' — İÇİNDE BULUNDUĞUMUZ kapsamda değil, DIŞARIDA aranmalı.
        if (declaredHere(hedef->name)) {
            diag_->error(s.span, "'" + hedef->name + "' zaten bu kapsamda tanımlı",
                         "'outer' yalnızca DIŞ kapsamdaki bir değişkene yazmak içindir");
            return;
        }
        bool bulundu = false;
        for (auto it = scopes_.rbegin() + 1; it != scopes_.rend(); ++it) {
            if (it->vars.count(hedef->name) > 0) {
                bulundu = true;
                break;
            }
        }
        if (!bulundu) {
            diag_->error(s.span, "'outer " + hedef->name + "' — dış kapsamlarda böyle bir değişken yok",
                         "'outer' var olan bir değişkene yazar, yenisini tanımlamaz");
            return;
        }
        lookup(hedef, hedef->name, hedef->span, false);
        return;
    }

    // Bileşik atama (x += 1) önce OKUR: değişken zaten var olmalı.
    if (s.op != Tok::Assign && !declaredHere(hedef->name)) {
        lookup(hedef, hedef->name, hedef->span, false);
        return;
    }

    if (!declaredHere(hedef->name)) {
        declare(hedef->name, hedef->span);
    }
    lookup(hedef, hedef->name, hedef->span, true);
}

void Resolver::visit(const Block& s) {
    push();
    hoist(s.stmts);
    for (const auto& x : s.stmts) {
        resolveStmt(x.get());
    }
    pop();
}

void Resolver::visit(const IfStmt& s) {
    resolveExpr(s.cond.get());
    resolveStmt(s.thenBranch.get());
    resolveStmt(s.elseBranch.get());
}

void Resolver::visit(const WhileStmt& s) {
    resolveExpr(s.cond.get());
    ++loopDepth_;
    resolveStmt(s.body.get());
    --loopDepth_;
}

void Resolver::visit(const ForStmt& s) {
    resolveExpr(s.iterable.get());
    push();
    for (const auto& v : s.vars) {
        declare(v, s.span);
    }
    ++loopDepth_;
    resolveStmt(s.body.get());
    --loopDepth_;
    pop();
}

void Resolver::visit(const BreakStmt& s) {
    if (loopDepth_ == 0) {
        diag_->error(s.span, "'break' bir döngü içinde değil");
    }
}

void Resolver::visit(const ContinueStmt& s) {
    if (loopDepth_ == 0) {
        diag_->error(s.span, "'continue' bir döngü içinde değil");
    }
}

void Resolver::visit(const PassStmt&) {}

void Resolver::visit(const ReturnStmt& s) {
    if (fnDepth_ == 0) {
        diag_->error(s.span, "'return' bir fonksiyon içinde değil");
    }
    resolveExpr(s.value.get());
}

void Resolver::visit(const ThrowStmt& s) { resolveExpr(s.value.get()); }

void Resolver::visit(const TryStmt& s) {
    resolveStmt(s.body.get());

    push();
    declare(s.catchVar, s.span);
    resolveStmt(s.catchBody.get());
    pop();

    resolveStmt(s.finallyBody.get());
}

void Resolver::visit(const FnDecl& s) {
    declare(s.name, s.span);  // hoist edilmişse zaten var; özyineleme için garanti

    for (const auto& d : s.decorators) {
        for (const auto& a : d.args) {
            resolveExpr(a.get());
        }
    }
    resolveType(s.retType.get());

    ++fnDepth_;
    push(true);
    resolveParams(s.params);
    // Gövde zaten Block; kendi kapsamını açmaması için doğrudan gez.
    if (const auto* b = dynamic_cast<const Block*>(s.body.get())) {
        hoist(b->stmts);
        for (const auto& x : b->stmts) {
            resolveStmt(x.get());
        }
    } else {
        resolveStmt(s.body.get());
    }
    pop();
    --fnDepth_;
}

void Resolver::visit(const ClassDecl& s) {
    declare(s.name, s.span);

    for (const auto& b : s.bases) {
        lookup(nullptr, b, s.span, false);
    }

    const bool oncekiMetot = inMethod_;
    const bool oncekiTaban = inClassWithBase_;
    const ClassDecl* oncekiSinif = currentClass_;
    inClassWithBase_ = !s.bases.empty();
    currentClass_ = &s;

    // Alan başlangıç değerleri sınıf kapsamında değil, dış kapsamda çözülür.
    for (const auto& f : s.fields) {
        resolveType(f.type.get());
        resolveExpr(f.init.get());
    }

    inMethod_ = true;
    for (const auto& m : s.methods) {
        resolveStmt(m.get());
    }
    inMethod_ = oncekiMetot;
    inClassWithBase_ = oncekiTaban;
    currentClass_ = oncekiSinif;
}

void Resolver::visit(const TraitDecl& s) {
    declare(s.name, s.span);
    for (const auto& m : s.methods) {
        resolveType(m.retType.get());
    }
}

void Resolver::visit(const ImportStmt& s) {
    // 'import std.math'      -> 'math'
    // 'import "a/b/rs-http"' -> 'rs-http' son parçası
    // 'as j'              -> 'j'
    std::string ad = s.alias;
    if (ad.empty()) {
        const char ayrac = s.isStd ? '.' : '/';
        const auto k = s.path.rfind(ayrac);
        ad = (k == std::string::npos) ? s.path : s.path.substr(k + 1);
    }
    if (!ad.empty()) {
        // Köprü kalıbında bir dosya hem 'include' hem 'import' içerebilir; ikisi aynı
        // adı bağlarsa biri diğerini sessizce gölgelemesin.
        if (declaredHere(ad)) {
            diag_->error(s.span, "'" + ad + "' adı zaten bağlı",
                         "birine takma ad ver — 'import " + ad + " as " + ad + "Sw'");
        }
        declare(ad, s.span);
    }
}

}  // namespace rs
