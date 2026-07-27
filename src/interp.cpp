#include "interp.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <sstream>

#include "resolver.hpp"

namespace rs {
namespace {

// Prelude'un RaidenScript'te yazılan kısmı. Error'ı burada tanımlamak,
// 'class YetersizKredi(Error)' kalıbını hiçbir özel durum kodu yazmadan çalıştırıyor.
constexpr const char* PRELUDE_SRC = R"RS(
class Error:
    fn init(self, message):
        self.message = message
)RS";

std::shared_ptr<ListObj> makeList(std::vector<Value> v = {}) {
    auto l = std::make_shared<ListObj>();
    l->items = std::move(v);
    return l;
}

std::shared_ptr<NativeObj> makeNative(std::string ad, int enAz, int enCok, NativeFn f) {
    auto n = std::make_shared<NativeObj>();
    n->name = std::move(ad);
    n->arityMin = enAz;
    n->arityMax = enCok;
    n->fn = std::move(f);
    return n;
}

// UTF-8 karakter sayısı (bayt değil) — 'Yıldırım'.len() == 8 olmalı.
std::size_t utf8Len(const std::string& s) {
    std::size_t n = 0;
    for (const char c : s) {
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) {
            ++n;
        }
    }
    return n;
}

// i'inci UTF-8 karakterinin bayt ofseti.
std::size_t utf8Offset(const std::string& s, std::size_t idx) {
    std::size_t n = 0, i = 0;
    for (; i < s.size(); ++i) {
        if ((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80) {
            if (n == idx) {
                return i;
            }
            ++n;
        }
    }
    return s.size();
}

std::string utf8Sub(const std::string& s, std::size_t bas, std::size_t son) {
    const std::size_t a = utf8Offset(s, bas);
    const std::size_t b = utf8Offset(s, son);
    return b > a ? s.substr(a, b - a) : std::string{};
}

bool sayiAl(const Value& v, double& out) {
    if (const auto* i = std::get_if<std::int64_t>(&v)) { out = static_cast<double>(*i); return true; }
    if (const auto* d = std::get_if<double>(&v)) { out = *d; return true; }
    return false;
}

}  // namespace

// ---------------------------------------------------------------- kurulum

Interpreter::Interpreter(const Source& src, Diagnostics& diag)
    : src_(&src), diag_(&diag), globals_(std::make_shared<Environment>()), env_(globals_) {
    preludeKur();
    yerlesikSinifKur();
}

Interpreter::~Interpreter() = default;

void Interpreter::yerlesikSinifKur() {
    preludeSrc_ = std::make_unique<Source>("<prelude>", PRELUDE_SRC);
    preludeDiag_ = std::make_unique<Diagnostics>(*preludeSrc_);
    Lexer lx(*preludeSrc_, *preludeDiag_);
    Parser ps(*preludeSrc_, lx.tokenize(), *preludeDiag_);
    preludeProg_ = std::make_unique<Program>(ps.parseProgram());

    for (const auto& s : preludeProg_->stmts) {
        exec(s.get());
    }
    if (Value* e = globals_->find("Error")) {
        if (auto* c = std::get_if<std::shared_ptr<ClassObj>>(e)) {
            errorClass_ = *c;
        }
    }
}

Value Interpreter::makeError(const std::string& mesaj) {
    auto inst = std::make_shared<InstanceObj>();
    inst->cls = errorClass_;
    inst->set("message", makeStr(mesaj));
    return inst;
}

void Interpreter::hata(Span span, const std::string& mesaj, const std::string& ipucu) {
    std::string tam = mesaj;
    if (!ipucu.empty()) {
        tam += " — " + ipucu;
    }
    throw ScriptThrow{makeError(tam), span};
}

void Interpreter::preludeKur() {
    auto tanim = [&](const char* ad, int a, int b, NativeFn f) {
        globals_->define(ad, makeNative(ad, a, b, std::move(f)));
    };

    tanim("print", 0, -1, [](std::vector<Value>& a) -> Value {
        for (std::size_t i = 0; i < a.size(); ++i) {
            if (i > 0) std::cout << ' ';
            std::cout << toDisplay(a[i]);
        }
        std::cout << '\n';
        return Nil{};
    });

    tanim("len", 1, 1, [this](std::vector<Value>& a) -> Value {
        if (const auto* s = std::get_if<Str>(&a[0])) {
            return static_cast<std::int64_t>(utf8Len(**s));
        }
        if (const auto* l = std::get_if<std::shared_ptr<ListObj>>(&a[0])) {
            return static_cast<std::int64_t>((*l)->items.size());
        }
        if (const auto* m = std::get_if<std::shared_ptr<MapObj>>(&a[0])) {
            return static_cast<std::int64_t>((*m)->entries.size());
        }
        hata(Span{}, "len() bu tip için tanımlı değil: " + typeName(a[0]));
    });

    tanim("type", 1, 1, [](std::vector<Value>& a) -> Value { return makeStr(typeName(a[0])); });

    tanim("str", 1, 1, [](std::vector<Value>& a) -> Value { return makeStr(toDisplay(a[0])); });

    tanim("bool", 1, 1, [](std::vector<Value>& a) -> Value { return truthy(a[0]); });

    tanim("int", 1, 1, [this](std::vector<Value>& a) -> Value {
        if (const auto* i = std::get_if<std::int64_t>(&a[0])) return *i;
        if (const auto* d = std::get_if<double>(&a[0])) return static_cast<std::int64_t>(*d);
        if (const auto* b = std::get_if<bool>(&a[0])) return static_cast<std::int64_t>(*b ? 1 : 0);
        if (const auto* s = std::get_if<Str>(&a[0])) {
            const std::string& t = **s;
            try {
                std::size_t pos = 0;
                const long long v = std::stoll(t, &pos);
                while (pos < t.size() && std::isspace(static_cast<unsigned char>(t[pos]))) ++pos;
                if (pos != t.size()) throw std::invalid_argument("");
                return static_cast<std::int64_t>(v);
            } catch (...) {
                hata(Span{}, "int() dönüştüremedi: \"" + t + "\"");
            }
        }
        hata(Span{}, "int() bu tipi dönüştüremez: " + typeName(a[0]));
    });

    tanim("float", 1, 1, [this](std::vector<Value>& a) -> Value {
        if (const auto* d = std::get_if<double>(&a[0])) return *d;
        if (const auto* i = std::get_if<std::int64_t>(&a[0])) return static_cast<double>(*i);
        if (const auto* s = std::get_if<Str>(&a[0])) {
            const std::string& t = **s;
            try {
                std::size_t pos = 0;
                const double v = std::stod(t, &pos);
                while (pos < t.size() && std::isspace(static_cast<unsigned char>(t[pos]))) ++pos;
                if (pos != t.size()) throw std::invalid_argument("");
                return v;
            } catch (...) {
                hata(Span{}, "float() dönüştüremedi: \"" + t + "\"");
            }
        }
        hata(Span{}, "float() bu tipi dönüştüremez: " + typeName(a[0]));
    });

    tanim("range", 1, 3, [this](std::vector<Value>& a) -> Value {
        auto tam = [&](const Value& v) -> std::int64_t {
            if (const auto* i = std::get_if<std::int64_t>(&v)) return *i;
            if (const auto* d = std::get_if<double>(&v)) return static_cast<std::int64_t>(*d);
            hata(Span{}, "range() tam sayı bekliyor");
        };
        std::int64_t bas = 0, son = 0, adim = 1;
        if (a.size() == 1) { son = tam(a[0]); }
        else { bas = tam(a[0]); son = tam(a[1]); if (a.size() > 2) adim = tam(a[2]); }
        if (adim == 0) hata(Span{}, "range() adımı 0 olamaz");

        std::vector<Value> out;
        if (adim > 0) { for (std::int64_t i = bas; i < son; i += adim) out.push_back(i); }
        else { for (std::int64_t i = bas; i > son; i += adim) out.push_back(i); }
        return makeList(std::move(out));
    });

    tanim("assert", 1, 2, [this](std::vector<Value>& a) -> Value {
        if (!truthy(a[0])) {
            const std::string m = a.size() > 1 ? toDisplay(a[1]) : "doğrulama başarısız";
            hata(Span{}, m);
        }
        return Nil{};
    });
}

// ---------------------------------------------------------------- çekirdek

Value Interpreter::eval(const Expr* e) {
    if (e == nullptr) {
        return Nil{};
    }
    e->accept(*this);
    return son_;
}

void Interpreter::exec(const Stmt* s) {
    if (s != nullptr) {
        s->accept(*this);
    }
}

void Interpreter::execBlock(const std::vector<StmtPtr>& stmts, std::shared_ptr<Environment> env) {
    auto onceki = env_;
    env_ = std::move(env);
    try {
        for (const auto& s : stmts) {
            exec(s.get());
        }
    } catch (...) {
        env_ = onceki;
        throw;
    }
    env_ = onceki;
}

bool Interpreter::run(const Program& p) {
    try {
        // Bildirimleri hoist et — resolver ile aynı kural.
        for (const auto& s : p.stmts) {
            if (dynamic_cast<const FnDecl*>(s.get()) || dynamic_cast<const ClassDecl*>(s.get())) {
                exec(s.get());
            }
        }
        for (const auto& s : p.stmts) {
            if (!dynamic_cast<const FnDecl*>(s.get()) && !dynamic_cast<const ClassDecl*>(s.get())) {
                exec(s.get());
            }
        }
    } catch (const ScriptThrow& t) {
        std::string mesaj = "yakalanmamış hata";
        if (const auto* inst = std::get_if<std::shared_ptr<InstanceObj>>(&t.value)) {
            if (Value* m = (*inst)->find("message")) {
                mesaj = toDisplay(*m);
            }
        } else {
            mesaj = toDisplay(t.value);
        }
        diag_->error(t.span, mesaj);
        return false;
    } catch (const ReturnSignal&) {
        return true;
    }
    return true;
}

// ---------------------------------------------------------------- ifadeler

void Interpreter::visit(const IntLit& e) { son_ = e.value; }
void Interpreter::visit(const FloatLit& e) { son_ = e.value; }
void Interpreter::visit(const StrLit& e) { son_ = makeStr(e.value); }
void Interpreter::visit(const BoolLit& e) { son_ = e.value; }
void Interpreter::visit(const NilLit&) { son_ = Nil{}; }

void Interpreter::visit(const FStrLit& e) {
    std::string out;
    for (const auto& p : e.parts) {
        if (p.expr) {
            const Value v = eval(p.expr.get());
            if (!p.format.empty() && !p.format.empty()) {
                // Basit biçim eki: '.Nf' ondalık basamak. Tam mini-dil Faz 2'de.
                if (p.format.size() >= 2 && p.format[0] == '.' &&
                    p.format.back() == 'f') {
                    const int basamak = std::atoi(p.format.substr(1).c_str());
                    double d = 0;
                    if (sayiAl(v, d)) {
                        std::ostringstream ss;
                        ss.setf(std::ios::fixed);
                        ss.precision(basamak);
                        ss << d;
                        out += ss.str();
                        continue;
                    }
                }
            }
            out += toDisplay(v);
        } else {
            out += p.literal;
        }
    }
    son_ = makeStr(std::move(out));
}

void Interpreter::visit(const Ident& e) {
    if (Value* v = env_->find(e.name)) {
        son_ = *v;
        return;
    }
    hata(e.span, "tanımsız ad: '" + e.name + "'");
}

void Interpreter::visit(const SelfExpr& e) {
    if (std::holds_alternative<Nil>(self_)) {
        hata(e.span, "'self' burada geçerli değil");
    }
    son_ = self_;
}

void Interpreter::visit(const SuperExpr& e) {
    // 'super' tek başına değer değil; super.init(...) biçiminde Member ile gelir.
    hata(e.span, "'super' tek başına kullanılamaz", "'super.metot(...)' biçiminde yaz");
}

void Interpreter::visit(const Unary& e) {
    if (e.op == Tok::KwNot) {
        son_ = !truthy(eval(e.operand.get()));
        return;
    }
    const Value v = eval(e.operand.get());
    switch (e.op) {
        case Tok::Minus:
            if (const auto* i = std::get_if<std::int64_t>(&v)) { son_ = -*i; return; }
            if (const auto* d = std::get_if<double>(&v)) { son_ = -*d; return; }
            hata(e.span, "tekli '-' sayı bekliyor, " + typeName(v) + " geldi");
        case Tok::Plus:
            son_ = v;
            return;
        case Tok::Tilde:
            if (const auto* i = std::get_if<std::int64_t>(&v)) { son_ = ~*i; return; }
            hata(e.span, "'~' tam sayı bekliyor");
        case Tok::KwAwait:
            hata(e.span, "'await' henüz desteklenmiyor", "async Faz 3'te gelecek");
        default:
            hata(e.span, "bilinmeyen tekli operatör");
    }
}

Value Interpreter::ikili(Tok op, const Value& a, const Value& b, Span span) {
    const auto* ia = std::get_if<std::int64_t>(&a);
    const auto* ib = std::get_if<std::int64_t>(&b);
    double da = 0, db = 0;
    const bool sayilar = sayiAl(a, da) && sayiAl(b, db);

    switch (op) {
        case Tok::Plus: {
            if (ia && ib) return *ia + *ib;
            if (sayilar) return da + db;
            if (const auto* sa = std::get_if<Str>(&a)) {
                if (const auto* sb = std::get_if<Str>(&b)) return makeStr(**sa + **sb);
                hata(span, "metne yalnızca metin eklenebilir",
                     "sayıyı metne çevirmek için str(...) kullan");
            }
            if (const auto* la = std::get_if<std::shared_ptr<ListObj>>(&a)) {
                if (const auto* lb = std::get_if<std::shared_ptr<ListObj>>(&b)) {
                    std::vector<Value> out = (*la)->items;
                    out.insert(out.end(), (*lb)->items.begin(), (*lb)->items.end());
                    return makeList(std::move(out));
                }
            }
            hata(span, "'+' bu tiplerde tanımlı değil: " + typeName(a) + " ve " + typeName(b));
        }
        case Tok::Minus:
            if (ia && ib) return *ia - *ib;
            if (sayilar) return da - db;
            hata(span, "'-' sayı bekliyor");
        case Tok::Star:
            if (ia && ib) return *ia * *ib;
            if (sayilar) return da * db;
            hata(span, "'*' sayı bekliyor");
        case Tok::Slash:
            // SPEC §2: '/' HER ZAMAN float döndürür.
            if (!sayilar) hata(span, "'/' sayı bekliyor");
            if (db == 0.0) hata(span, "sıfıra bölme");
            return da / db;
        case Tok::SlashSlash:
            if (!sayilar) hata(span, "'//' sayı bekliyor");
            if (db == 0.0) hata(span, "sıfıra bölme");
            if (ia && ib) {
                std::int64_t q = *ia / *ib;
                if ((*ia % *ib != 0) && ((*ia < 0) != (*ib < 0))) --q;  // aşağı yuvarla
                return q;
            }
            return std::floor(da / db);
        case Tok::Percent:
            if (!sayilar) hata(span, "'%' sayı bekliyor");
            if (db == 0.0) hata(span, "sıfıra bölme");
            if (ia && ib) {
                std::int64_t r = *ia % *ib;
                if (r != 0 && ((r < 0) != (*ib < 0))) r += *ib;
                return r;
            }
            return std::fmod(da, db);
        case Tok::StarStar:
            if (!sayilar) hata(span, "'**' sayı bekliyor");
            if (ia && ib && *ib >= 0) {
                std::int64_t sonuc = 1;
                for (std::int64_t k = 0; k < *ib; ++k) sonuc *= *ia;
                return sonuc;
            }
            return std::pow(da, db);

        case Tok::EqEq:   return valueEquals(a, b);
        case Tok::BangEq: return !valueEquals(a, b);

        case Tok::Lt: case Tok::Gt: case Tok::LtEq: case Tok::GtEq: {
            if (sayilar) {
                switch (op) {
                    case Tok::Lt:   return da < db;
                    case Tok::Gt:   return da > db;
                    case Tok::LtEq: return da <= db;
                    default:        return da >= db;
                }
            }
            if (const auto* sa = std::get_if<Str>(&a)) {
                if (const auto* sb = std::get_if<Str>(&b)) {
                    switch (op) {
                        case Tok::Lt:   return **sa < **sb;
                        case Tok::Gt:   return **sa > **sb;
                        case Tok::LtEq: return **sa <= **sb;
                        default:        return **sa >= **sb;
                    }
                }
            }
            hata(span, "karşılaştırma bu tiplerde tanımlı değil: " + typeName(a) + " ve " + typeName(b));
        }

        case Tok::KwIs: {
            // 'x is Sinif' — kalıtım zincirini de kontrol eder.
            if (const auto* c = std::get_if<std::shared_ptr<ClassObj>>(&b)) {
                if (const auto* inst = std::get_if<std::shared_ptr<InstanceObj>>(&a)) {
                    return (*inst)->cls && (*inst)->cls->isSubclassOf(c->get());
                }
                return false;
            }
            return valueEquals(a, b);
        }

        case Tok::KwIn: {
            if (const auto* sb = std::get_if<Str>(&b)) {
                if (const auto* sa = std::get_if<Str>(&a)) {
                    return (*sb)->find(**sa) != std::string::npos;
                }
                hata(span, "'in' metin içinde metin arar");
            }
            if (const auto* lb = std::get_if<std::shared_ptr<ListObj>>(&b)) {
                for (const auto& x : (*lb)->items) {
                    if (valueEquals(a, x)) return true;
                }
                return false;
            }
            if (const auto* mb = std::get_if<std::shared_ptr<MapObj>>(&b)) {
                for (const auto& [k, v] : (*mb)->entries) {
                    (void)v;
                    if (valueEquals(a, k)) return true;
                }
                return false;
            }
            hata(span, "'in' sağ tarafta metin, liste veya harita bekliyor");
        }

        case Tok::Amp:  if (ia && ib) return *ia & *ib;  hata(span, "'&' tam sayı bekliyor");
        case Tok::Pipe: if (ia && ib) return *ia | *ib;  hata(span, "'|' tam sayı bekliyor");
        case Tok::Caret:if (ia && ib) return *ia ^ *ib;  hata(span, "'^' tam sayı bekliyor");
        case Tok::LtLt: if (ia && ib) return *ia << *ib; hata(span, "'<<' tam sayı bekliyor");
        case Tok::GtGt: if (ia && ib) return *ia >> *ib; hata(span, "'>>' tam sayı bekliyor");

        default:
            hata(span, "bilinmeyen ikili operatör");
    }
}

void Interpreter::visit(const Binary& e) {
    const Value a = eval(e.left.get());
    const Value b = eval(e.right.get());
    son_ = ikili(e.op, a, b, e.span);
}

void Interpreter::visit(const Logical& e) {
    // Kısa devre — bu yüzden AST'de ayrı düğüm.
    const Value sol = eval(e.left.get());
    if (e.op == Tok::KwOr) {
        son_ = truthy(sol) ? sol : eval(e.right.get());
    } else {
        son_ = truthy(sol) ? eval(e.right.get()) : sol;
    }
}

void Interpreter::visit(const Ternary& e) {
    son_ = truthy(eval(e.cond.get())) ? eval(e.thenExpr.get()) : eval(e.elseExpr.get());
}

Value Interpreter::araligiAc(const RangeExpr& r, Span span) {
    std::int64_t bas = 0, son = 0;
    const auto tam = [&](const Expr* e, std::int64_t vars) -> std::int64_t {
        if (e == nullptr) return vars;
        const Value v = eval(e);
        if (const auto* i = std::get_if<std::int64_t>(&v)) return *i;
        if (const auto* d = std::get_if<double>(&v)) return static_cast<std::int64_t>(*d);
        hata(span, "aralık uçları tam sayı olmalı, " + typeName(v) + " geldi");
    };
    bas = tam(r.lo.get(), 0);
    if (r.hi == nullptr) {
        hata(span, "üst ucu olmayan aralık yalnızca dilim içinde kullanılabilir",
             "örnek: liste[2..]");
    }
    son = tam(r.hi.get(), 0);
    if (r.inclusive) ++son;

    if (son - bas > 10'000'000) {
        hata(span, "aralık çok büyük (10 milyondan fazla eleman)");
    }
    std::vector<Value> out;
    for (std::int64_t i = bas; i < son; ++i) out.push_back(i);
    return makeList(std::move(out));
}

void Interpreter::visit(const RangeExpr& e) { son_ = araligiAc(e, e.span); }

void Interpreter::visit(const ListLit& e) {
    std::vector<Value> out;
    out.reserve(e.items.size());
    for (const auto& x : e.items) {
        out.push_back(eval(x.get()));
    }
    son_ = makeList(std::move(out));
}

void Interpreter::visit(const MapLit& e) {
    auto m = std::make_shared<MapObj>();
    for (const auto& [k, v] : e.entries) {
        m->entries.emplace_back(eval(k.get()), eval(v.get()));
    }
    son_ = m;
}

void Interpreter::visit(const Lambda& e) {
    auto f = std::make_shared<FnObj>();
    f->name = "<lambda>";
    f->params = &e.params;
    f->body = e.bodyBlock.get();
    f->bodyExpr = e.bodyExpr.get();
    f->closure = env_;
    son_ = f;
}

void Interpreter::visit(const Call& e) {
    // super.metot(...) özel yolu
    if (const auto* m = dynamic_cast<const Member*>(e.callee.get())) {
        if (dynamic_cast<const SuperExpr*>(m->object.get()) != nullptr) {
            if (!superOwner_ || !superOwner_->base) {
                hata(e.span, "'super' için taban sınıf yok");
            }
            auto fn = superOwner_->base->findMethod(m->name);
            if (!fn) {
                hata(e.span, "taban sınıfta '" + m->name + "' metodu yok");
            }
            std::vector<Value> args;
            for (const auto& a : e.args) args.push_back(eval(a.get()));
            son_ = cagirFn(fn, args, e.span, &self_);
            return;
        }
    }

    const Value callee = eval(e.callee.get());
    std::vector<Value> args;
    args.reserve(e.args.size());
    for (const auto& a : e.args) {
        args.push_back(eval(a.get()));
    }
    son_ = cagir(callee, args, e.span);
}

Value Interpreter::cagirFn(const std::shared_ptr<FnObj>& fn, std::vector<Value>& args, Span span,
                           const Value* self) {
    auto yerel = std::make_shared<Environment>(fn->closure);

    const auto& ps = *fn->params;
    std::size_t argIdx = 0;
    for (std::size_t i = 0; i < ps.size(); ++i) {
        if (ps[i].name == "self") {
            yerel->define("self", self != nullptr ? *self : Nil{});
            continue;
        }
        if (argIdx < args.size()) {
            yerel->define(ps[i].name, args[argIdx++]);
        } else if (ps[i].defaultValue) {
            auto onceki = env_;
            env_ = fn->closure;
            Value d = eval(ps[i].defaultValue.get());
            env_ = onceki;
            yerel->define(ps[i].name, std::move(d));
        } else {
            hata(span, "'" + fn->name + "' için '" + ps[i].name + "' argümanı eksik");
        }
    }

    const Value onceSelf = self_;
    const auto onceOwner = superOwner_;
    if (self != nullptr) {
        self_ = *self;
        superOwner_ = fn->owner;
    }

    Value sonuc = Nil{};
    try {
        if (fn->bodyExpr != nullptr) {
            auto onceki = env_;
            env_ = yerel;
            sonuc = eval(fn->bodyExpr);
            env_ = onceki;
        } else if (const auto* b = dynamic_cast<const Block*>(fn->body)) {
            execBlock(b->stmts, yerel);
        }
    } catch (const ReturnSignal& r) {
        sonuc = r.value;
    } catch (...) {
        self_ = onceSelf;
        superOwner_ = onceOwner;
        throw;
    }

    self_ = onceSelf;
    superOwner_ = onceOwner;
    return sonuc;
}

Value Interpreter::cagir(const Value& callee, std::vector<Value>& args, Span span) {
    if (const auto* n = std::get_if<std::shared_ptr<NativeObj>>(&callee)) {
        const auto& no = **n;
        const int say = static_cast<int>(args.size());
        if (say < no.arityMin || (no.arityMax >= 0 && say > no.arityMax)) {
            hata(span, "'" + no.name + "' argüman sayısı uymuyor (" + std::to_string(say) + " verildi)");
        }
        return no.fn(args);
    }
    if (const auto* f = std::get_if<std::shared_ptr<FnObj>>(&callee)) {
        return cagirFn(*f, args, span, nullptr);
    }
    if (const auto* b = std::get_if<std::shared_ptr<BoundObj>>(&callee)) {
        return cagirFn((*b)->fn, args, span, &(*b)->self);
    }
    if (const auto* c = std::get_if<std::shared_ptr<ClassObj>>(&callee)) {
        auto inst = std::make_shared<InstanceObj>();
        inst->cls = *c;

        // Alan başlangıç değerleri (kalıtım zinciri tabandan başlayarak)
        std::vector<const ClassObj*> zincir;
        for (const ClassObj* k = c->get(); k != nullptr; k = k->base.get()) {
            zincir.push_back(k);
        }
        for (auto it = zincir.rbegin(); it != zincir.rend(); ++it) {
            if ((*it)->decl == nullptr) continue;
            for (const auto& f : (*it)->decl->fields) {
                inst->set(f.name, f.init ? eval(f.init.get()) : Value{Nil{}});
            }
        }

        Value self = inst;
        if (auto init = (*c)->findMethod("init")) {
            cagirFn(init, args, span, &self);
        } else if (!args.empty()) {
            hata(span, "'" + (*c)->name + "' init metodu yok ama argüman verildi");
        }
        return self;
    }
    hata(span, typeName(callee) + " çağrılabilir değil");
}

// ---------------------------------------------------------------- üye erişimi

Value Interpreter::yerlesikMetot(const Value& obj, const std::string& ad, Span span) {
    // --- string ---
    if (const auto* sp = std::get_if<Str>(&obj)) {
        const Str s = *sp;
        const auto mk = [&](const char* nm, int a, int b, NativeFn f) {
            return makeNative(nm, a, b, std::move(f));
        };
        if (ad == "len")   return mk("len", 0, 0, [s](std::vector<Value>&) -> Value { return static_cast<std::int64_t>(utf8Len(*s)); });
        if (ad == "upper") return mk("upper", 0, 0, [s](std::vector<Value>&) -> Value {
            std::string o = *s;
            // Türkçe i/İ: ASCII dışına dokunmuyoruz ama 'i' -> 'İ' özel durumu doğru olsun.
            std::string r;
            for (std::size_t i = 0; i < o.size();) {
                if (static_cast<unsigned char>(o[i]) == 0x69) { r += "İ"; ++i; continue; }  // i -> İ
                if (i + 1 < o.size() && static_cast<unsigned char>(o[i]) == 0xC4 &&
                    static_cast<unsigned char>(o[i + 1]) == 0xB1) { r += 'I'; i += 2; continue; } // ı -> I
                const unsigned char c = static_cast<unsigned char>(o[i]);
                r += (c >= 'a' && c <= 'z') ? static_cast<char>(c - 32) : o[i];
                ++i;
            }
            return makeStr(std::move(r));
        });
        if (ad == "lower") return mk("lower", 0, 0, [s](std::vector<Value>&) -> Value {
            std::string r;
            for (std::size_t i = 0; i < s->size();) {
                if (i + 1 < s->size() && static_cast<unsigned char>((*s)[i]) == 0xC4 &&
                    static_cast<unsigned char>((*s)[i + 1]) == 0xB0) { r += 'i'; i += 2; continue; }  // İ -> i
                if (static_cast<unsigned char>((*s)[i]) == 0x49) { r += "ı"; ++i; continue; }     // I -> ı
                const unsigned char c = static_cast<unsigned char>((*s)[i]);
                r += (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : (*s)[i];
                ++i;
            }
            return makeStr(std::move(r));
        });
        if (ad == "trim") return mk("trim", 0, 0, [s](std::vector<Value>&) -> Value {
            const auto a = s->find_first_not_of(" \t\n\r");
            if (a == std::string::npos) return makeStr("");
            const auto b = s->find_last_not_of(" \t\n\r");
            return makeStr(s->substr(a, b - a + 1));
        });
        if (ad == "split") return mk("split", 1, 1, [s](std::vector<Value>& a) -> Value {
            const std::string ayr = **std::get_if<Str>(&a[0]);
            std::vector<Value> out;
            if (ayr.empty()) { out.push_back(makeStr(*s)); return makeList(std::move(out)); }
            std::size_t bas = 0, k;
            while ((k = s->find(ayr, bas)) != std::string::npos) {
                out.push_back(makeStr(s->substr(bas, k - bas)));
                bas = k + ayr.size();
            }
            out.push_back(makeStr(s->substr(bas)));
            return makeList(std::move(out));
        });
        if (ad == "contains")   return mk("contains", 1, 1, [s](std::vector<Value>& a) -> Value { return s->find(**std::get_if<Str>(&a[0])) != std::string::npos; });
        if (ad == "startsWith") return mk("startsWith", 1, 1, [s](std::vector<Value>& a) -> Value { const std::string& p = **std::get_if<Str>(&a[0]); return s->rfind(p, 0) == 0; });
        if (ad == "endsWith")   return mk("endsWith", 1, 1, [s](std::vector<Value>& a) -> Value { const std::string& p = **std::get_if<Str>(&a[0]); return s->size() >= p.size() && s->compare(s->size() - p.size(), p.size(), p) == 0; });
        if (ad == "indexOf")    return mk("indexOf", 1, 1, [s](std::vector<Value>& a) -> Value { const auto k = s->find(**std::get_if<Str>(&a[0])); return k == std::string::npos ? std::int64_t{-1} : static_cast<std::int64_t>(utf8Len(s->substr(0, k))); });
        if (ad == "replace")    return mk("replace", 2, 2, [s](std::vector<Value>& a) -> Value {
            const std::string x = **std::get_if<Str>(&a[0]), y = **std::get_if<Str>(&a[1]);
            std::string r = *s;
            const auto k = r.find(x);
            if (k != std::string::npos) r.replace(k, x.size(), y);
            return makeStr(std::move(r));
        });
        if (ad == "replaceAll") return mk("replaceAll", 2, 2, [s](std::vector<Value>& a) -> Value {
            const std::string x = **std::get_if<Str>(&a[0]), y = **std::get_if<Str>(&a[1]);
            if (x.empty()) return makeStr(*s);
            std::string r;
            std::size_t bas = 0, k;
            while ((k = s->find(x, bas)) != std::string::npos) { r += s->substr(bas, k - bas) + y; bas = k + x.size(); }
            r += s->substr(bas);
            return makeStr(std::move(r));
        });
    }

    // --- liste ---
    if (const auto* lp = std::get_if<std::shared_ptr<ListObj>>(&obj)) {
        const auto l = *lp;
        if (ad == "len")  return makeNative("len", 0, 0, [l](std::vector<Value>&) -> Value { return static_cast<std::int64_t>(l->items.size()); });
        if (ad == "push") return makeNative("push", 1, -1, [l](std::vector<Value>& a) -> Value { for (auto& x : a) l->items.push_back(x); return static_cast<std::int64_t>(l->items.size()); });
        if (ad == "pop")  return makeNative("pop", 0, 0, [l](std::vector<Value>&) -> Value {
            if (l->items.empty()) return Nil{};
            Value v = l->items.back(); l->items.pop_back(); return v;
        });
        if (ad == "copy") return makeNative("copy", 0, 0, [l](std::vector<Value>&) -> Value { return makeList(l->items); });
        if (ad == "join") return makeNative("join", 1, 1, [l](std::vector<Value>& a) -> Value {
            const std::string ayr = **std::get_if<Str>(&a[0]);
            std::string r;
            for (std::size_t i = 0; i < l->items.size(); ++i) { if (i) r += ayr; r += toDisplay(l->items[i]); }
            return makeStr(std::move(r));
        });
        if (ad == "map") return makeNative("map", 1, 1, [l, span, this](std::vector<Value>& a) -> Value {
            std::vector<Value> out;
            for (auto& x : l->items) { std::vector<Value> tek{x}; out.push_back(cagir(a[0], tek, span)); }
            return makeList(std::move(out));
        });
        if (ad == "filter") return makeNative("filter", 1, 1, [l, span, this](std::vector<Value>& a) -> Value {
            std::vector<Value> out;
            for (auto& x : l->items) { std::vector<Value> tek{x}; if (truthy(cagir(a[0], tek, span))) out.push_back(x); }
            return makeList(std::move(out));
        });
        if (ad == "forEach") return makeNative("forEach", 1, 1, [l, span, this](std::vector<Value>& a) -> Value {
            for (auto& x : l->items) { std::vector<Value> tek{x}; cagir(a[0], tek, span); }
            return Nil{};
        });
        if (ad == "reduce") return makeNative("reduce", 1, 2, [l, span, this](std::vector<Value>& a) -> Value {
            Value acc = a.size() > 1 ? a[1] : Value{Nil{}};
            std::size_t i = 0;
            if (a.size() == 1) { if (l->items.empty()) return Nil{}; acc = l->items[0]; i = 1; }
            for (; i < l->items.size(); ++i) { std::vector<Value> iki{acc, l->items[i]}; acc = cagir(a[0], iki, span); }
            return acc;
        });
    }

    // --- harita ---
    if (const auto* mp = std::get_if<std::shared_ptr<MapObj>>(&obj)) {
        const auto m = *mp;
        if (ad == "len")  return makeNative("len", 0, 0, [m](std::vector<Value>&) -> Value { return static_cast<std::int64_t>(m->entries.size()); });
        if (ad == "keys") return makeNative("keys", 0, 0, [m](std::vector<Value>&) -> Value {
            std::vector<Value> out; for (auto& [k, v] : m->entries) { (void)v; out.push_back(k); } return makeList(std::move(out));
        });
        if (ad == "values") return makeNative("values", 0, 0, [m](std::vector<Value>&) -> Value {
            std::vector<Value> out; for (auto& [k, v] : m->entries) { (void)k; out.push_back(v); } return makeList(std::move(out));
        });
        if (ad == "items") return makeNative("items", 0, 0, [m](std::vector<Value>&) -> Value {
            std::vector<Value> out;
            for (auto& [k, v] : m->entries) out.push_back(makeList({k, v}));
            return makeList(std::move(out));
        });
    }

    return Nil{};
}

Value Interpreter::uyeOku(const Value& obj, const std::string& ad, Span span, bool guvenli) {
    if (guvenli && std::holds_alternative<Nil>(obj)) {
        return Nil{};
    }

    if (const auto* inst = std::get_if<std::shared_ptr<InstanceObj>>(&obj)) {
        if (Value* f = (*inst)->find(ad)) {
            return *f;
        }
        if (auto m = (*inst)->cls->findMethod(ad)) {
            auto b = std::make_shared<BoundObj>();
            b->self = obj;
            b->fn = m;
            return b;
        }
        hata(span, "'" + (*inst)->cls->name + "' nesnesinde '" + ad + "' yok");
    }

    const Value yerlesik = yerlesikMetot(obj, ad, span);
    if (!std::holds_alternative<Nil>(yerlesik)) {
        return yerlesik;
    }

    // Haritada nokta erişimi: 'olay.hedef' == 'olay["hedef"]'.
    // Host'tan gelen olay/JSON verileri harita olarak geldiği için bu şart —
    // aksi hâlde gömme API'si 'olay["hedef"]["hp"]' gibi okunmaz hâle gelir.
    // METOTLAR ÖNCELİKLİ: 'harita.len' yerleşik metottur, anahtar değil.
    if (const auto* mp = std::get_if<std::shared_ptr<MapObj>>(&obj)) {
        for (auto& [k, v] : (*mp)->entries) {
            if (const auto* ks = std::get_if<Str>(&k)) {
                if (**ks == ad) {
                    return v;
                }
            }
        }
        hata(span, "haritada '" + ad + "' anahtarı yok",
             "anahtar adı bir yerleşik metotla çakışıyorsa harita[\"" + ad + "\"] yaz");
    }

    if (std::holds_alternative<Nil>(obj)) {
        hata(span, "nil üzerinde '" + ad + "' okunamaz", "'?.' ile güvenli erişim kullanabilirsin");
    }
    hata(span, typeName(obj) + " tipinde '" + ad + "' üyesi yok");
}

void Interpreter::visit(const Member& e) {
    const Value obj = eval(e.object.get());
    son_ = uyeOku(obj, e.name, e.span, e.safe);
}

Value Interpreter::indeksOku(const Value& obj, const Value& idx, Span span) {
    // Dilim mi?
    if (const auto* l = std::get_if<std::shared_ptr<ListObj>>(&idx)) {
        (void)l;  // dilim RangeExpr olarak gelir, aşağıda ele alınıyor
    }

    if (const auto* lp = std::get_if<std::shared_ptr<ListObj>>(&obj)) {
        const auto& v = (*lp)->items;
        if (const auto* i = std::get_if<std::int64_t>(&idx)) {
            std::int64_t k = *i;
            if (k < 0) k += static_cast<std::int64_t>(v.size());
            if (k < 0 || k >= static_cast<std::int64_t>(v.size())) {
                hata(span, "liste indeksi sınır dışı: " + std::to_string(*i) +
                               " (uzunluk " + std::to_string(v.size()) + ")");
            }
            return v[static_cast<std::size_t>(k)];
        }
        hata(span, "liste indeksi tam sayı olmalı");
    }

    if (const auto* sp = std::get_if<Str>(&obj)) {
        if (const auto* i = std::get_if<std::int64_t>(&idx)) {
            const auto n = static_cast<std::int64_t>(utf8Len(**sp));
            std::int64_t k = *i;
            if (k < 0) k += n;
            if (k < 0 || k >= n) hata(span, "metin indeksi sınır dışı");
            return makeStr(utf8Sub(**sp, static_cast<std::size_t>(k), static_cast<std::size_t>(k) + 1));
        }
        hata(span, "metin indeksi tam sayı olmalı");
    }

    if (const auto* mp = std::get_if<std::shared_ptr<MapObj>>(&obj)) {
        for (auto& [k, v] : (*mp)->entries) {
            if (valueEquals(k, idx)) return v;
        }
        hata(span, "haritada böyle bir anahtar yok: " + toRepr(idx));
    }

    hata(span, typeName(obj) + " indekslenemez");
}

void Interpreter::indeksYaz(const Value& obj, const Value& idx, Value v, Span span) {
    if (const auto* lp = std::get_if<std::shared_ptr<ListObj>>(&obj)) {
        auto& items = (*lp)->items;
        if (const auto* i = std::get_if<std::int64_t>(&idx)) {
            std::int64_t k = *i;
            if (k < 0) k += static_cast<std::int64_t>(items.size());
            if (k < 0 || k >= static_cast<std::int64_t>(items.size())) {
                hata(span, "liste indeksi sınır dışı: " + std::to_string(*i));
            }
            items[static_cast<std::size_t>(k)] = std::move(v);
            return;
        }
        hata(span, "liste indeksi tam sayı olmalı");
    }
    if (const auto* mp = std::get_if<std::shared_ptr<MapObj>>(&obj)) {
        for (auto& [k, val] : (*mp)->entries) {
            if (valueEquals(k, idx)) { val = std::move(v); return; }
        }
        (*mp)->entries.emplace_back(idx, std::move(v));
        return;
    }
    hata(span, typeName(obj) + " üzerine indeksle yazılamaz");
}

void Interpreter::visit(const IndexExpr& e) {
    const Value obj = eval(e.object.get());

    // Dilim: liste[a..b], metin[a..], liste[..b]
    if (const auto* r = dynamic_cast<const RangeExpr*>(e.index.get())) {
        const auto uzunluk = [&]() -> std::int64_t {
            if (const auto* l = std::get_if<std::shared_ptr<ListObj>>(&obj)) return static_cast<std::int64_t>((*l)->items.size());
            if (const auto* s = std::get_if<Str>(&obj)) return static_cast<std::int64_t>(utf8Len(**s));
            hata(e.span, typeName(obj) + " dilimlenemez");
        }();

        const auto uc = [&](const Expr* x, std::int64_t vars) -> std::int64_t {
            if (x == nullptr) return vars;
            const Value v = eval(x);
            const auto* i = std::get_if<std::int64_t>(&v);
            if (i == nullptr) hata(e.span, "dilim ucu tam sayı olmalı");
            return *i < 0 ? *i + uzunluk : *i;
        };

        std::int64_t a = uc(r->lo.get(), 0);
        std::int64_t b = uc(r->hi.get(), uzunluk);
        if (r->inclusive) ++b;
        a = std::max<std::int64_t>(0, std::min(a, uzunluk));
        b = std::max<std::int64_t>(a, std::min(b, uzunluk));

        if (const auto* l = std::get_if<std::shared_ptr<ListObj>>(&obj)) {
            std::vector<Value> out((*l)->items.begin() + a, (*l)->items.begin() + b);
            son_ = makeList(std::move(out));
            return;
        }
        const auto* s = std::get_if<Str>(&obj);
        son_ = makeStr(utf8Sub(**s, static_cast<std::size_t>(a), static_cast<std::size_t>(b)));
        return;
    }

    son_ = indeksOku(obj, eval(e.index.get()), e.span);
}

// ---------------------------------------------------------------- deyimler

void Interpreter::visit(const ExprStmt& s) { son_ = eval(s.expr.get()); }

void Interpreter::visit(const AssignStmt& s) {
    Value deger = eval(s.value.get());

    // Bileşik atama: önce oku, işlemi uygula.
    const auto birlesik = [&](const Value& eski) -> Value {
        switch (s.op) {
            case Tok::PlusEq:       return ikili(Tok::Plus, eski, deger, s.span);
            case Tok::MinusEq:      return ikili(Tok::Minus, eski, deger, s.span);
            case Tok::StarEq:       return ikili(Tok::Star, eski, deger, s.span);
            case Tok::SlashEq:      return ikili(Tok::Slash, eski, deger, s.span);
            case Tok::SlashSlashEq: return ikili(Tok::SlashSlash, eski, deger, s.span);
            case Tok::PercentEq:    return ikili(Tok::Percent, eski, deger, s.span);
            case Tok::StarStarEq:   return ikili(Tok::StarStar, eski, deger, s.span);
            default:                return deger;
        }
    };

    if (const auto* id = dynamic_cast<const Ident*>(s.target.get())) {
        if (s.outer) {
            Value* p = env_->findOuter(id->name);
            if (p == nullptr) hata(s.span, "'outer " + id->name + "' — dış kapsamda yok");
            *p = s.op == Tok::Assign ? std::move(deger) : birlesik(*p);
            return;
        }
        if (s.op != Tok::Assign) {
            Value* p = env_->find(id->name);
            if (p == nullptr) hata(s.span, "tanımsız ad: '" + id->name + "'");
            *p = birlesik(*p);
            return;
        }
        // Var olan bir değişkene yazıyorsak onu güncelle, yoksa bu kapsamda tanımla.
        if (Value* p = env_->find(id->name)) {
            *p = std::move(deger);
        } else {
            env_->define(id->name, std::move(deger));
        }
        return;
    }

    if (const auto* m = dynamic_cast<const Member*>(s.target.get())) {
        const Value obj = eval(m->object.get());

        if (const auto* inst = std::get_if<std::shared_ptr<InstanceObj>>(&obj)) {
            if (s.op != Tok::Assign) {
                Value* p = (*inst)->find(m->name);
                if (p == nullptr) hata(s.span, "'" + m->name + "' alanı yok");
                *p = birlesik(*p);
                return;
            }
            (*inst)->set(m->name, std::move(deger));
            return;
        }

        // Haritaya nokta ile yazma: 'olay.hp = 5' == 'olay["hp"] = 5'
        if (std::holds_alternative<std::shared_ptr<MapObj>>(obj)) {
            const Value anahtar = makeStr(m->name);
            if (s.op != Tok::Assign) {
                indeksYaz(obj, anahtar, birlesik(indeksOku(obj, anahtar, s.span)), s.span);
            } else {
                indeksYaz(obj, anahtar, std::move(deger), s.span);
            }
            return;
        }

        hata(s.span, typeName(obj) + " üzerine alan yazılamaz");
    }

    if (const auto* ix = dynamic_cast<const IndexExpr*>(s.target.get())) {
        const Value obj = eval(ix->object.get());
        const Value idx = eval(ix->index.get());
        if (s.op != Tok::Assign) {
            const Value eski = indeksOku(obj, idx, s.span);
            indeksYaz(obj, idx, birlesik(eski), s.span);
            return;
        }
        indeksYaz(obj, idx, std::move(deger), s.span);
        return;
    }

    hata(s.span, "bu ifadeye atama yapılamaz");
}

void Interpreter::visit(const Block& s) {
    execBlock(s.stmts, std::make_shared<Environment>(env_));
}

void Interpreter::visit(const IfStmt& s) {
    if (truthy(eval(s.cond.get()))) {
        exec(s.thenBranch.get());
    } else {
        exec(s.elseBranch.get());
    }
}

void Interpreter::visit(const WhileStmt& s) {
    while (truthy(eval(s.cond.get()))) {
        try {
            exec(s.body.get());
        } catch (const BreakSignal&) {
            break;
        } catch (const ContinueSignal&) {
            continue;
        }
    }
}

std::vector<Value> Interpreter::gezilebilir(const Value& v, Span span) {
    if (const auto* l = std::get_if<std::shared_ptr<ListObj>>(&v)) {
        return (*l)->items;
    }
    if (const auto* m = std::get_if<std::shared_ptr<MapObj>>(&v)) {
        std::vector<Value> out;
        for (const auto& [k, val] : (*m)->entries) {
            (void)val;
            out.push_back(k);  // haritada gezinme ANAHTARLAR üzerinde
        }
        return out;
    }
    if (const auto* s = std::get_if<Str>(&v)) {
        std::vector<Value> out;
        const std::size_t n = utf8Len(**s);
        for (std::size_t i = 0; i < n; ++i) {
            out.push_back(makeStr(utf8Sub(**s, i, i + 1)));
        }
        return out;
    }
    hata(span, typeName(v) + " gezilemez", "liste, harita, metin veya aralık bekleniyordu");
}

void Interpreter::visit(const ForStmt& s) {
    const Value kaynak = eval(s.iterable.get());
    const std::vector<Value> ogeler = gezilebilir(kaynak, s.span);

    for (const auto& oge : ogeler) {
        auto yerel = std::make_shared<Environment>(env_);

        if (s.vars.size() == 1) {
            yerel->define(s.vars[0], oge);
        } else {
            // 'for k, v in harita.items()' — her öğe 2 elemanlı liste
            const auto* alt = std::get_if<std::shared_ptr<ListObj>>(&oge);
            if (alt == nullptr || (*alt)->items.size() < s.vars.size()) {
                hata(s.span, "çoklu döngü değişkeni için her öğe " +
                                 std::to_string(s.vars.size()) + " elemanlı olmalı");
            }
            for (std::size_t i = 0; i < s.vars.size(); ++i) {
                yerel->define(s.vars[i], (*alt)->items[i]);
            }
        }

        auto onceki = env_;
        env_ = yerel;
        try {
            exec(s.body.get());
        } catch (const BreakSignal&) {
            env_ = onceki;
            break;
        } catch (const ContinueSignal&) {
            env_ = onceki;
            continue;
        } catch (...) {
            env_ = onceki;
            throw;
        }
        env_ = onceki;
    }
}

void Interpreter::visit(const BreakStmt&) { throw BreakSignal{}; }
void Interpreter::visit(const ContinueStmt&) { throw ContinueSignal{}; }
void Interpreter::visit(const PassStmt&) {}

void Interpreter::visit(const ReturnStmt& s) {
    throw ReturnSignal{s.value ? eval(s.value.get()) : Value{Nil{}}};
}

void Interpreter::visit(const ThrowStmt& s) {
    throw ScriptThrow{eval(s.value.get()), s.span};
}

void Interpreter::visit(const TryStmt& s) {
    bool yakalandi = false;
    try {
        exec(s.body.get());
    } catch (const ScriptThrow& t) {
        yakalandi = true;
        auto yerel = std::make_shared<Environment>(env_);
        yerel->define(s.catchVar, t.value);
        auto onceki = env_;
        env_ = yerel;
        try {
            exec(s.catchBody.get());
        } catch (...) {
            env_ = onceki;
            if (s.finallyBody) exec(s.finallyBody.get());
            throw;
        }
        env_ = onceki;
    } catch (...) {
        // return/break/continue sinyalleri: finally yine de çalışmalı
        if (s.finallyBody) exec(s.finallyBody.get());
        throw;
    }
    (void)yakalandi;
    if (s.finallyBody) {
        exec(s.finallyBody.get());
    }
}

void Interpreter::visit(const FnDecl& s) {
    auto f = std::make_shared<FnObj>();
    f->name = s.name;
    f->params = &s.params;
    f->body = s.body.get();
    f->closure = env_;
    env_->define(s.name, f);
}

void Interpreter::visit(const ClassDecl& s) {
    std::shared_ptr<ClassObj> taban;
    for (const auto& b : s.bases) {
        if (Value* v = env_->find(b)) {
            if (auto* c = std::get_if<std::shared_ptr<ClassObj>>(v)) {
                taban = *c;   // ilk bulunan sınıf taban; gerisi trait sayılır
                break;
            }
        }
    }

    auto cls = std::make_shared<ClassObj>();
    cls->name = s.name;
    cls->base = taban;
    cls->decl = &s;
    cls->closure = env_;

    for (const auto& m : s.methods) {
        auto f = std::make_shared<FnObj>();
        f->name = m->name;
        f->params = &m->params;
        f->body = m->body.get();
        f->closure = env_;
        f->isMethod = true;
        f->owner = cls;
        cls->methods[m->name] = f;
    }

    env_->define(s.name, cls);
}

void Interpreter::visit(const TraitDecl& s) {
    // Faz 1'de trait yalnızca sözleşme; çalışma zamanı davranışı yok.
    auto cls = std::make_shared<ClassObj>();
    cls->name = s.name;
    env_->define(s.name, cls);
}

// Yerleşik std modülleri. Repo bazlı 'import' Faz 4'te gelecek; bunlar dilin
// kendi kütüphanesi, ağdan çözülmez.
Value Interpreter::stdModul(const std::string& yol) {
    if (yol != "std.math") {
        return Nil{};
    }

    auto m = std::make_shared<MapObj>();
    const auto ekle = [&](const char* ad, int enAz, int enCok, NativeFn f) {
        m->entries.emplace_back(makeStr(ad), makeNative(ad, enAz, enCok, std::move(f)));
    };
    const auto sabit = [&](const char* ad, double v) {
        m->entries.emplace_back(makeStr(ad), v);
    };

    sabit("PI", 3.14159265358979323846);
    sabit("E", 2.71828182845904523536);

    // Tek argümanlı kayan noktalı fonksiyonları tek kalıptan üret.
    const auto tekli = [&](const char* ad, double (*f)(double)) {
        ekle(ad, 1, 1, [this, f, ad](std::vector<Value>& a) -> Value {
            double x = 0;
            if (!sayiAl(a[0], x)) hata(Span{}, std::string("math.") + ad + "() sayı bekliyor");
            return f(x);
        });
    };
    tekli("sqrt", std::sqrt);
    tekli("sin", std::sin);
    tekli("cos", std::cos);
    tekli("tan", std::tan);
    tekli("log", std::log);
    tekli("exp", std::exp);
    tekli("floor", std::floor);
    tekli("ceil", std::ceil);

    ekle("abs", 1, 1, [this](std::vector<Value>& a) -> Value {
        if (const auto* i = std::get_if<std::int64_t>(&a[0])) return *i < 0 ? -*i : *i;
        double x = 0;
        if (!sayiAl(a[0], x)) hata(Span{}, "math.abs() sayı bekliyor");
        return std::fabs(x);
    });
    ekle("atan2", 2, 2, [this](std::vector<Value>& a) -> Value {
        double y = 0, x = 0;
        if (!sayiAl(a[0], y) || !sayiAl(a[1], x)) hata(Span{}, "math.atan2() sayı bekliyor");
        return std::atan2(y, x);
    });
    ekle("pow", 2, 2, [this](std::vector<Value>& a) -> Value {
        double x = 0, y = 0;
        if (!sayiAl(a[0], x) || !sayiAl(a[1], y)) hata(Span{}, "math.pow() sayı bekliyor");
        return std::pow(x, y);
    });
    ekle("min", 1, -1, [this](std::vector<Value>& a) -> Value {
        Value en = a[0];
        for (auto& v : a) if (truthy(ikili(Tok::Lt, v, en, Span{}))) en = v;
        return en;
    });
    ekle("max", 1, -1, [this](std::vector<Value>& a) -> Value {
        Value en = a[0];
        for (auto& v : a) if (truthy(ikili(Tok::Gt, v, en, Span{}))) en = v;
        return en;
    });

    return m;
}

void Interpreter::setHostBridge(std::vector<std::pair<std::string, std::string>> kayitli,
                                HostFn cb) {
    hostKayit_ = std::move(kayitli);
    hostFn_ = std::move(cb);
}

std::optional<Value> Interpreter::hostModul(const std::string& modul) {
    if (!hostFn_) {
        return std::nullopt;
    }
    auto harita = std::make_shared<MapObj>();
    for (const auto& [m, f] : hostKayit_) {
        if (m != modul) {
            continue;
        }
        // Her host fonksiyonu bir native'e sarılır: argümanlar double'a çevrilip
        // köprüye verilir, dönen sayı betiğe sonuç olur.
        const std::string fnAd = f;
        HostFn cb = hostFn_;
        harita->entries.emplace_back(
            makeStr(fnAd),
            makeNative(modul + "." + fnAd, 0, -1,
                       [modul, fnAd, cb](std::vector<Value>& a) -> Value {
                           std::vector<double> sayilar;
                           sayilar.reserve(a.size());
                           for (const auto& v : a) {
                               sayilar.push_back(toDouble(v));
                           }
                           return cb(modul, fnAd, sayilar);
                       }));
    }
    if (harita->entries.empty()) {
        return std::nullopt;
    }
    return Value{harita};
}

bool Interpreter::callGlobal(const std::string& ad, std::vector<Value>& args, Value& out) {
    Value* hedef = globals_->find(ad);
    if (hedef == nullptr) {
        diag_->error(Span{0, 0}, "'" + ad + "' adında bir global yok",
                     "betikte 'fn " + ad + "(...)' tanımlı mı?");
        return false;
    }
    try {
        out = cagir(*hedef, args, Span{0, 0});
        return true;
    } catch (const ScriptThrow& t) {
        diag_->error(t.span, "betik istisna fırlattı: " + toDisplay(t.value));
        return false;
    } catch (const ReturnSignal&) {
        // Fonksiyon gövdesi dışında return — cagirFn zaten yakalıyor, buraya
        // düşerse dilin değil host'un hatası.
        diag_->error(Span{0, 0}, "beklenmeyen 'return' sinyali");
        return false;
    }
}

void Interpreter::visit(const ImportStmt& s) {
    std::string ad = s.alias;
    if (ad.empty()) {
        const char ayrac = s.isStd ? '.' : '/';
        const auto k = s.path.rfind(ayrac);
        ad = (k == std::string::npos) ? s.path : s.path.substr(k + 1);
    }
    if (ad.empty()) {
        return;
    }

    Value modul{Nil{}};
    if (s.isInclude) {
        // 'include' std kütüphanesinden ÇÖZÜLMEZ — bağlayıcıyı host derlemeye katar.
        if (auto hm = hostModul(s.path)) {
            modul = std::move(*hm);
        } else {
            diag_->warning(s.span, "'" + s.path + "' host bağlayıcısı henüz yok",
                           "host bu modülü rs_register ile kaydetmemiş; nil olarak tanımlanıyor");
        }
    } else if (s.isStd) {
        modul = stdModul(s.path);
        if (std::holds_alternative<Nil>(modul)) {
            // Bilinmeyen std modülü: hata değil uyarı — Faz 2/3'te dolacak.
            diag_->warning(s.span, "'" + s.path + "' henüz uygulanmadı",
                           "şimdilik nil olarak tanımlanıyor");
        }
    }
    env_->define(ad, std::move(modul));
}

}  // namespace rs
