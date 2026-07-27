#include "astdump.hpp"

#include <ostream>

namespace rs {
namespace {
struct Kademe {
    int* d;
    explicit Kademe(int* p) : d(p) { ++*d; }
    ~Kademe() { --*d; }
    Kademe(const Kademe&) = delete;
    Kademe& operator=(const Kademe&) = delete;
};
}  // namespace

void AstDumper::satir(std::string_view etiket, std::string ek) {
    for (int i = 0; i < derinlik_; ++i) {
        *os_ << "  ";
    }
    *os_ << etiket;
    if (!ek.empty()) {
        *os_ << ' ' << ek;
    }
    *os_ << '\n';
}

void AstDumper::alt(const Expr* e) {
    Kademe k(&derinlik_);
    if (e == nullptr) {
        satir("<eksik>");
        return;
    }
    e->accept(*this);
}

void AstDumper::alt(const Stmt* s) {
    Kademe k(&derinlik_);
    if (s == nullptr) {
        satir("<eksik>");
        return;
    }
    s->accept(*this);
}

std::string AstDumper::tipMetni(const TypeNode* t) const {
    if (t == nullptr) {
        return {};
    }
    std::string s;
    if (t->isFn) {
        s = "fn(";
        for (std::size_t i = 0; i < t->fnParams.size(); ++i) {
            if (i > 0) { s += ", "; }
            s += tipMetni(t->fnParams[i].get());
        }
        s += ")";
        if (t->fnRet) {
            s += " -> " + tipMetni(t->fnRet.get());
        }
    } else {
        s = t->name;
        if (!t->args.empty()) {
            s += "[";
            for (std::size_t i = 0; i < t->args.size(); ++i) {
                if (i > 0) { s += ", "; }
                s += tipMetni(t->args[i].get());
            }
            s += "]";
        }
    }
    if (t->nullable) {
        s += "?";
    }
    return s;
}

void AstDumper::tipYaz(const TypeNode* t) {
    if (t != nullptr) {
        Kademe k(&derinlik_);
        satir("tip:", tipMetni(t));
    }
}

void AstDumper::dump(const Program& p) {
    satir("program", "(" + std::to_string(p.stmts.size()) + " deyim)");
    for (const auto& s : p.stmts) {
        alt(s.get());
    }
}

// ---------------------------------------------------------------- ifadeler

void AstDumper::visit(const IntLit& e) { satir("int", std::to_string(e.value)); }
void AstDumper::visit(const FloatLit& e) { satir("float", std::to_string(e.value)); }
void AstDumper::visit(const StrLit& e) { satir(e.raw ? "rawstr" : "str", "\"" + e.value + "\""); }
void AstDumper::visit(const BoolLit& e) { satir("bool", e.value ? "true" : "false"); }
void AstDumper::visit(const NilLit&) { satir("nil"); }
void AstDumper::visit(const Ident& e) { satir("ad", e.name); }
void AstDumper::visit(const SelfExpr&) { satir("self"); }
void AstDumper::visit(const SuperExpr&) { satir("super"); }

void AstDumper::visit(const FStrLit& e) {
    satir("fstring", "(" + std::to_string(e.parts.size()) + " parça)");
    for (const auto& p : e.parts) {
        if (p.expr) {
            Kademe k(&derinlik_);
            satir("ifade", p.format.empty() ? "" : "biçim=" + p.format);
            alt(p.expr.get());
        } else {
            Kademe k(&derinlik_);
            satir("metin", "\"" + p.literal + "\"");
        }
    }
}

void AstDumper::visit(const Unary& e) {
    satir("tekli", std::string(tokLexeme(e.op)));
    alt(e.operand.get());
}

void AstDumper::visit(const Binary& e) {
    satir("ikili", std::string(tokLexeme(e.op)));
    alt(e.left.get());
    alt(e.right.get());
}

void AstDumper::visit(const Logical& e) {
    satir("mantık", std::string(tokLexeme(e.op)));
    alt(e.left.get());
    alt(e.right.get());
}

void AstDumper::visit(const Ternary& e) {
    satir("koşullu");
    { Kademe k(&derinlik_); satir("koşul:"); }
    alt(e.cond.get());
    { Kademe k(&derinlik_); satir("ise:"); }
    alt(e.thenExpr.get());
    { Kademe k(&derinlik_); satir("değilse:"); }
    alt(e.elseExpr.get());
}

void AstDumper::visit(const RangeExpr& e) {
    satir("aralık", e.inclusive ? "..=" : "..");
    if (e.lo) { alt(e.lo.get()); } else { Kademe k(&derinlik_); satir("<açık alt uç>"); }
    if (e.hi) { alt(e.hi.get()); } else { Kademe k(&derinlik_); satir("<açık üst uç>"); }
}

void AstDumper::visit(const Call& e) {
    satir("çağrı", "(" + std::to_string(e.args.size()) + " argüman)");
    alt(e.callee.get());
    for (const auto& a : e.args) {
        alt(a.get());
    }
}

void AstDumper::visit(const IndexExpr& e) {
    satir("indeks");
    alt(e.object.get());
    alt(e.index.get());
}

void AstDumper::visit(const Member& e) {
    satir(e.safe ? "alan?." : "alan.", e.name);
    alt(e.object.get());
}

void AstDumper::visit(const Lambda& e) {
    std::string p;
    for (std::size_t i = 0; i < e.params.size(); ++i) {
        if (i > 0) { p += ", "; }
        p += e.params[i].name;
        if (e.params[i].type) { p += ": " + tipMetni(e.params[i].type.get()); }
    }
    satir("lambda", "(" + p + ")");
    if (e.bodyExpr) { alt(e.bodyExpr.get()); } else { alt(e.bodyBlock.get()); }
}

void AstDumper::visit(const ListLit& e) {
    satir("liste", "(" + std::to_string(e.items.size()) + ")");
    for (const auto& x : e.items) {
        alt(x.get());
    }
}

void AstDumper::visit(const MapLit& e) {
    satir("harita", "(" + std::to_string(e.entries.size()) + ")");
    for (const auto& [a, d] : e.entries) {
        alt(a.get());
        alt(d.get());
    }
}

// ---------------------------------------------------------------- deyimler

void AstDumper::visit(const ExprStmt& s) {
    satir("ifade-deyimi");
    alt(s.expr.get());
}

void AstDumper::visit(const AssignStmt& s) {
    std::string ek = std::string(tokLexeme(s.op));
    if (s.outer) { ek = "outer " + ek; }
    satir("atama", ek);
    tipYaz(s.declType.get());
    alt(s.target.get());
    alt(s.value.get());
}

void AstDumper::visit(const Block& s) {
    satir("blok", "(" + std::to_string(s.stmts.size()) + ")");
    for (const auto& x : s.stmts) {
        alt(x.get());
    }
}

void AstDumper::visit(const IfStmt& s) {
    satir("if");
    alt(s.cond.get());
    alt(s.thenBranch.get());
    if (s.elseBranch) {
        { Kademe k(&derinlik_); satir("else:"); }
        alt(s.elseBranch.get());
    }
}

void AstDumper::visit(const WhileStmt& s) {
    satir("while");
    alt(s.cond.get());
    alt(s.body.get());
}

void AstDumper::visit(const ForStmt& s) {
    std::string v;
    for (std::size_t i = 0; i < s.vars.size(); ++i) {
        if (i > 0) { v += ", "; }
        v += s.vars[i];
    }
    satir("for", v);
    alt(s.iterable.get());
    alt(s.body.get());
}

void AstDumper::visit(const BreakStmt&) { satir("break"); }
void AstDumper::visit(const ContinueStmt&) { satir("continue"); }
void AstDumper::visit(const PassStmt&) { satir("pass"); }

void AstDumper::visit(const ReturnStmt& s) {
    satir("return");
    if (s.value) { alt(s.value.get()); }
}

void AstDumper::visit(const ThrowStmt& s) {
    satir("throw");
    alt(s.value.get());
}

void AstDumper::visit(const TryStmt& s) {
    satir("try");
    alt(s.body.get());
    { Kademe k(&derinlik_); satir("catch", s.catchVar); }
    alt(s.catchBody.get());
    if (s.finallyBody) {
        { Kademe k(&derinlik_); satir("finally:"); }
        alt(s.finallyBody.get());
    }
}

void AstDumper::visit(const FnDecl& s) {
    std::string p;
    for (std::size_t i = 0; i < s.params.size(); ++i) {
        if (i > 0) { p += ", "; }
        p += s.params[i].name;
        if (s.params[i].type) { p += ": " + tipMetni(s.params[i].type.get()); }
        if (s.params[i].defaultValue) { p += "=…"; }
    }
    std::string basl = s.name + "(" + p + ")";
    if (s.retType) { basl += " -> " + tipMetni(s.retType.get()); }
    satir(s.isAsync ? "async fn" : "fn", basl);
    for (const auto& d : s.decorators) {
        Kademe k(&derinlik_);
        satir("@" + d.name);
    }
    alt(s.body.get());
}

void AstDumper::visit(const ClassDecl& s) {
    std::string b;
    for (std::size_t i = 0; i < s.bases.size(); ++i) {
        if (i > 0) { b += ", "; }
        b += s.bases[i];
    }
    satir("class", s.name + (b.empty() ? "" : "(" + b + ")"));
    for (const auto& f : s.fields) {
        Kademe k(&derinlik_);
        satir("alan", f.name + ": " + tipMetni(f.type.get()));
        if (f.init) { alt(f.init.get()); }
    }
    for (const auto& m : s.methods) {
        alt(m.get());
    }
}

void AstDumper::visit(const TraitDecl& s) {
    satir("trait", s.name);
    for (const auto& m : s.methods) {
        Kademe k(&derinlik_);
        std::string p;
        for (std::size_t i = 0; i < m.params.size(); ++i) {
            if (i > 0) { p += ", "; }
            p += m.params[i].name;
        }
        satir("imza", m.name + "(" + p + ")");
    }
}

void AstDumper::visit(const ImportStmt& s) {
    std::string ek = s.path;
    if (!s.version.empty()) { ek += " @ " + s.version; }
    if (!s.alias.empty()) { ek += " as " + s.alias; }
    satir(s.isInclude ? "include" : (s.isStd ? "import std" : "import repo"), ek);
}

}  // namespace rs
