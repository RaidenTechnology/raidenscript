#include "parser.hpp"

#include <utility>

#include "lexer.hpp"

namespace rs {

// ---------------------------------------------------------------- imleç

const Token& Parser::peek(std::size_t ileri) const {
    const std::size_t k = i_ + ileri;
    return k < toks_.size() ? toks_[k] : toks_.back();
}

const Token& Parser::previous() const {
    return i_ > 0 ? toks_[i_ - 1] : toks_.front();
}

const Token& Parser::advance() {
    if (!atEnd()) {
        ++i_;
    }
    return previous();
}

bool Parser::match(Tok k) {
    if (!check(k)) {
        return false;
    }
    advance();
    return true;
}

bool Parser::matchAny(std::initializer_list<Tok> ks) {
    for (const Tok k : ks) {
        if (check(k)) {
            advance();
            return true;
        }
    }
    return false;
}

const Token& Parser::expect(Tok k, std::string_view ne) {
    if (check(k)) {
        return advance();
    }
    err(peek(), std::string(ne) + " bekleniyordu, '" + std::string(tokName(peek().kind)) + "' bulundu");
    return peek();
}

void Parser::err(const Token& t, std::string mesaj, std::string ipucu) {
    if (hataSayisi_ >= MAKS_HATA) {
        return;
    }
    ++hataSayisi_;
    diag_->error(t.span, std::move(mesaj), std::move(ipucu));
}

void Parser::skipNewlines() {
    while (check(Tok::Newline)) {
        advance();
    }
}

// Deyim sonu. Satır sonu ya da ';' tüketilir; '}' / DEDENT / dosya sonu ise
// tüketilmeden kabul edilir — tek satırlık `=> { x = 1 }` bunu gerektiriyor.
void Parser::endOfStatement() {
    if (match(Tok::Newline) || match(Tok::Semicolon)) {
        return;
    }
    if (check(Tok::RBrace) || check(Tok::Dedent) || atEnd()) {
        return;
    }
    err(peek(), std::string("satır sonu bekleniyordu, '") +
                    std::string(tokName(peek().kind)) + "' bulundu");
}

void Parser::synchronize() {
    // Hatadan sonra güvenli bir yere atla: satır sonu, blok çıkışı veya
    // yeni bir deyim başlangıcı.
    while (!atEnd()) {
        if (previous().kind == Tok::Newline) {
            return;
        }
        switch (peek().kind) {
            case Tok::KwFn: case Tok::KwClass: case Tok::KwTrait: case Tok::KwIf:
            case Tok::KwWhile: case Tok::KwFor: case Tok::KwReturn: case Tok::KwTry:
            case Tok::KwImport: case Tok::KwInclude: case Tok::Dedent:
                return;
            default:
                advance();
        }
    }
}

template <class T>
std::unique_ptr<T> Parser::make(Span span) {
    auto p = std::make_unique<T>();
    p->span = span;
    return p;
}

bool Parser::assignable(const Expr& e) noexcept {
    return dynamic_cast<const Ident*>(&e) != nullptr ||
           dynamic_cast<const Member*>(&e) != nullptr ||
           dynamic_cast<const IndexExpr*>(&e) != nullptr;
}

// ---------------------------------------------------------------- program

Program Parser::parseProgram() {
    Program prog;
    skipNewlines();
    while (!atEnd()) {
        if (match(Tok::Indent)) {
            err(previous(), "beklenmeyen girinti",
                "bu satır bir bloğun içinde değil");
            continue;
        }
        if (match(Tok::Dedent)) {
            continue;
        }
        if (auto s = statement()) {
            prog.stmts.push_back(std::move(s));
        }
        skipNewlines();
    }
    return prog;
}

// ---------------------------------------------------------------- deyimler

StmtPtr Parser::statement() {
    switch (peek().kind) {
        case Tok::KwIf:    return ifStatement();
        case Tok::KwWhile: return whileStatement();
        case Tok::KwFor:   return forStatement();
        case Tok::KwTry:   return tryStatement();
        case Tok::KwClass: return classDecl();
        case Tok::KwTrait: return traitDecl();
        case Tok::KwFn:    return functionDecl({}, false);
        case Tok::At:      {
            auto dek = decorators();
            const bool async = match(Tok::KwAsync);
            if (!check(Tok::KwFn)) {
                err(peek(), "dekoratörden sonra fonksiyon bekleniyordu");
                synchronize();
                return nullptr;
            }
            return functionDecl(std::move(dek), async);
        }
        case Tok::KwAsync:
            if (peek(1).kind == Tok::KwFn) {
                advance();
                return functionDecl({}, true);
            }
            break;
        default:
            break;
    }
    return simpleStatement();
}

StmtPtr Parser::simpleStatement() {
    const Token& bas = peek();

    if (check(Tok::KwImport) || check(Tok::KwInclude)) {
        return ImportStatement();
    }

    if (match(Tok::KwBreak)) {
        auto s = make<BreakStmt>(previous().span);
        endOfStatement();
        return s;
    }
    if (match(Tok::KwContinue)) {
        auto s = make<ContinueStmt>(previous().span);
        endOfStatement();
        return s;
    }
    if (match(Tok::KwPass)) {
        auto s = make<PassStmt>(previous().span);
        endOfStatement();
        return s;
    }
    if (match(Tok::KwReturn)) {
        auto s = make<ReturnStmt>(previous().span);
        if (!check(Tok::Newline) && !atEnd()) {
            s->value = expression();
        }
        endOfStatement();
        return s;
    }
    if (match(Tok::KwThrow)) {
        auto s = make<ThrowStmt>(previous().span);
        s->value = expression();
        endOfStatement();
        return s;
    }

    const bool dis = match(Tok::KwOuter);

    auto sol = expression();
    if (!sol) {
        synchronize();
        return nullptr;
    }

    // 'x: int = 5' — tip notasyonlu tanım
    TypePtr tip;
    if (check(Tok::Colon) && dynamic_cast<Ident*>(sol.get()) != nullptr) {
        advance();
        tip = typeNode();
    }

    if (matchAny({Tok::Assign, Tok::PlusEq, Tok::MinusEq, Tok::StarEq, Tok::SlashEq,
                  Tok::SlashSlashEq, Tok::PercentEq, Tok::StarStarEq})) {
        const Tok op = previous().kind;
        if (!assignable(*sol)) {
            err(previous(), "bu ifadeye atama yapılamaz",
                "sol tarafta bir değişken, alan (a.b) veya indeks (a[i]) olmalı");
        }
        auto s = make<AssignStmt>(Span::merge(bas.span, previous().span));
        s->target = std::move(sol);
        s->op = op;
        s->declType = std::move(tip);
        s->outer = dis;
        s->value = expression();
        endOfStatement();
        return s;
    }

    if (dis) {
        err(bas, "'outer' bir atamadan önce gelmeli");
    }
    if (tip) {
        err(bas, "tip notasyonu var ama atama yok",
            "'x: int = 0' biçiminde başlangıç değeri ver");
    }

    auto s = make<ExprStmt>(sol->span);
    s->expr = std::move(sol);
    endOfStatement();
    return s;
}

StmtPtr Parser::block() {
    expect(Tok::Colon, "':'");

    // Tek satır kısayolu:  if x: return 1
    if (!check(Tok::Newline)) {
        auto b = make<Block>(peek().span);
        if (auto s = simpleStatement()) {
            b->stmts.push_back(std::move(s));
        }
        return b;
    }

    endOfStatement();
    if (!check(Tok::Indent)) {
        err(peek(), "girintili blok bekleniyordu",
            "':' sonrası satırda girintiyi artır");
        return make<Block>(peek().span);
    }
    advance();  // INDENT

    auto b = make<Block>(previous().span);
    skipNewlines();
    while (!check(Tok::Dedent) && !atEnd()) {
        if (auto s = statement()) {
            b->stmts.push_back(std::move(s));
        } else {
            synchronize();
        }
        skipNewlines();
    }
    expect(Tok::Dedent, "blok sonu");
    return b;
}

StmtPtr Parser::braceBlock() {
    const Token& ac = expect(Tok::LBrace, "'{'");
    auto b = make<Block>(ac.span);
    skipNewlines();

    // Çok satırlı gövde kendi girinti bağlamını açar; tek satırlık gövde açmaz.
    const bool girintili = match(Tok::Indent);
    skipNewlines();

    while (!check(Tok::RBrace) && !check(Tok::Dedent) && !atEnd()) {
        if (auto s = statement()) {
            b->stmts.push_back(std::move(s));
        } else {
            synchronize();
        }
        skipNewlines();
        while (match(Tok::Semicolon)) {
            skipNewlines();
        }
    }

    if (girintili) {
        expect(Tok::Dedent, "blok sonu");
        skipNewlines();
    }
    expect(Tok::RBrace, "'}'");
    return b;
}

StmtPtr Parser::ifStatement() {
    const Token& bas = advance();  // if / elif
    auto s = make<IfStmt>(bas.span);
    s->cond = expression();
    s->thenBranch = block();

    skipNewlines();
    if (check(Tok::KwElif)) {
        s->elseBranch = ifStatement();
    } else if (match(Tok::KwElse)) {
        s->elseBranch = block();
    }
    return s;
}

StmtPtr Parser::whileStatement() {
    const Token& bas = advance();
    auto s = make<WhileStmt>(bas.span);
    s->cond = expression();
    s->body = block();
    return s;
}

StmtPtr Parser::forStatement() {
    const Token& bas = advance();
    auto s = make<ForStmt>(bas.span);

    do {
        const Token& ad = expect(Tok::Ident, "döngü değişkeni");
        s->vars.push_back(ad.text);
    } while (match(Tok::Comma));

    expect(Tok::KwIn, "'in'");
    s->iterable = expression();
    s->body = block();
    return s;
}

StmtPtr Parser::tryStatement() {
    const Token& bas = advance();
    auto s = make<TryStmt>(bas.span);
    s->body = block();

    skipNewlines();
    if (!check(Tok::KwCatch)) {
        err(peek(), "'try' bloğu 'catch' ile devam etmeli");
        return s;
    }
    advance();
    const Token& degisken = expect(Tok::Ident, "hata değişkeni");
    s->catchVar = degisken.text;
    s->catchBody = block();

    skipNewlines();
    if (match(Tok::KwFinally)) {
        s->finallyBody = block();
    }
    return s;
}

StmtPtr Parser::ImportStatement() {
    const Token& bas = advance();
    auto s = make<ImportStmt>(bas.span);
    s->isInclude = (bas.kind == Tok::KwInclude);

    if (check(Tok::Str)) {
        const Token& yol = advance();
        s->path = yol.text;
        // 'include' derleme anında çözülür: ortada repo çekecek bir çalışma zamanı yok.
        if (s->isInclude) {
            err(yol, "'include' tırnaklı yol alamaz",
                "repo bağımlılıkları çalışma zamanında çözülür, 'import' kullan");
        }
        if (match(Tok::At)) {
            const Token& surum = expect(Tok::Str, "sürüm etiketi");
            s->version = surum.text;
            if (s->isInclude) {
                err(surum, "'include' sürüm etiketi alamaz",
                    "sürüm çözümü ağ gerektirir, 'import ... @ \"v0.3.1\"' kullan");
            }
        }
    } else {
        s->isStd = true;
        s->path = expect(Tok::Ident, "modül adı").text;
        while (match(Tok::Dot)) {
            s->path += '.';
            s->path += expect(Tok::Ident, "alt modül adı").text;
        }
    }

    if (match(Tok::KwAs)) {
        s->alias = expect(Tok::Ident, "takma ad").text;
    }
    endOfStatement();
    return s;
}

std::vector<Decorator> Parser::decorators() {
    std::vector<Decorator> out;
    while (match(Tok::At)) {
        Decorator d;
        d.span = previous().span;
        d.name = expect(Tok::Ident, "dekoratör adı").text;
        if (match(Tok::LParen)) {
            if (!check(Tok::RParen)) {
                do {
                    d.args.push_back(expression());
                } while (match(Tok::Comma));
            }
            expect(Tok::RParen, "')'");
        }
        endOfStatement();
        skipNewlines();
        out.push_back(std::move(d));
    }
    return out;
}

std::vector<Param> Parser::paramList() {
    std::vector<Param> out;
    if (check(Tok::RParen)) {
        return out;
    }
    do {
        Param p;
        if (check(Tok::KwSelf)) {
            p.span = advance().span;
            p.name = "self";
        } else {
            const Token& ad = expect(Tok::Ident, "parametre adı");
            p.span = ad.span;
            p.name = ad.text;
        }
        if (match(Tok::Colon)) {
            p.type = typeNode();
        }
        if (match(Tok::Assign)) {
            p.defaultValue = expression();
        }
        out.push_back(std::move(p));
    } while (match(Tok::Comma));
    return out;
}

StmtPtr Parser::functionDecl(std::vector<Decorator> dek, bool isAsync) {
    const Token& bas = expect(Tok::KwFn, "'fn'");
    auto f = make<FnDecl>(bas.span);
    f->isAsync = isAsync;
    f->decorators = std::move(dek);
    f->name = expect(Tok::Ident, "fonksiyon adı").text;

    expect(Tok::LParen, "'('");
    f->params = paramList();
    expect(Tok::RParen, "')'");

    if (match(Tok::Arrow)) {
        f->retType = typeNode();
    }
    f->body = block();
    return f;
}

StmtPtr Parser::classDecl() {
    const Token& bas = advance();
    auto c = make<ClassDecl>(bas.span);
    c->name = expect(Tok::Ident, "sınıf adı").text;

    if (match(Tok::LParen)) {
        if (!check(Tok::RParen)) {
            do {
                c->bases.push_back(expect(Tok::Ident, "taban sınıf veya trait adı").text);
            } while (match(Tok::Comma));
        }
        expect(Tok::RParen, "')'");
    }

    expect(Tok::Colon, "':'");
    endOfStatement();
    if (!check(Tok::Indent)) {
        err(peek(), "sınıf gövdesi girintili olmalı");
        return c;
    }
    advance();
    skipNewlines();

    while (!check(Tok::Dedent) && !atEnd()) {
        if (check(Tok::KwFn) || check(Tok::At) || check(Tok::KwAsync)) {
            auto dekler = check(Tok::At) ? decorators() : std::vector<Decorator>{};
            const bool async = match(Tok::KwAsync);
            auto m = functionDecl(std::move(dekler), async);
            // FnDecl'e güvenle indir: functionDecl her zaman FnDecl üretir.
            c->methods.emplace_back(static_cast<FnDecl*>(m.release()));
        } else if (check(Tok::Ident)) {
            // Alan bildirimi. Tip İSTEĞE BAĞLI: 'ad = "Falcon"' de geçerli,
            // tipi başlangıç değerinden çıkarılır (kademeli tip felsefesi).
            ClassDecl::Field alan;
            const Token& ad = advance();
            alan.span = ad.span;
            alan.name = ad.text;
            if (match(Tok::Colon)) {
                alan.type = typeNode();
            }
            if (match(Tok::Assign)) {
                alan.init = expression();
            } else if (!alan.type) {
                err(ad, "alan bildiriminde tip veya başlangıç değeri olmalı",
                    "'hp: int' ya da 'hp = 100' yaz");
            }
            endOfStatement();
            c->fields.push_back(std::move(alan));
        } else {
            err(peek(), "sınıf gövdesinde alan veya metot bekleniyordu");
            synchronize();
        }
        skipNewlines();
    }
    expect(Tok::Dedent, "sınıf gövdesi sonu");
    return c;
}

StmtPtr Parser::traitDecl() {
    const Token& bas = advance();
    auto t = make<TraitDecl>(bas.span);
    t->name = expect(Tok::Ident, "trait adı").text;

    expect(Tok::Colon, "':'");
    endOfStatement();
    if (!check(Tok::Indent)) {
        err(peek(), "trait gövdesi girintili olmalı");
        return t;
    }
    advance();
    skipNewlines();

    while (!check(Tok::Dedent) && !atEnd()) {
        if (!check(Tok::KwFn)) {
            err(peek(), "trait gövdesinde sadece metot imzası olabilir",
                "gövdesiz yazılır: fn ciz(self, yzc)");
            synchronize();
            skipNewlines();
            continue;
        }
        advance();
        TraitDecl::Sig sig;
        const Token& ad = expect(Tok::Ident, "metot adı");
        sig.span = ad.span;
        sig.name = ad.text;
        expect(Tok::LParen, "'('");
        sig.params = paramList();
        expect(Tok::RParen, "')'");
        if (match(Tok::Arrow)) {
            sig.retType = typeNode();
        }
        endOfStatement();
        t->methods.push_back(std::move(sig));
        skipNewlines();
    }
    expect(Tok::Dedent, "trait gövdesi sonu");
    return t;
}

// ---------------------------------------------------------------- tipler

TypePtr Parser::typeNode() {
    auto t = std::make_unique<TypeNode>();
    t->span = peek().span;

    if (match(Tok::KwFn)) {
        t->isFn = true;
        t->name = "fn";
        expect(Tok::LParen, "'('");
        if (!check(Tok::RParen)) {
            do {
                t->fnParams.push_back(typeNode());
            } while (match(Tok::Comma));
        }
        expect(Tok::RParen, "')'");
        if (match(Tok::Arrow)) {
            t->fnRet = typeNode();
        }
    } else {
        t->name = expect(Tok::Ident, "tip adı").text;
        if (match(Tok::LBracket)) {
            do {
                t->args.push_back(typeNode());
            } while (match(Tok::Comma));
            expect(Tok::RBracket, "']'");
        }
    }

    if (match(Tok::Question)) {
        t->nullable = true;
    }
    t->span = Span::merge(t->span, previous().span);
    return t;
}

// ---------------------------------------------------------------- ifadeler

namespace {

// '(' konumundan başlayıp eşleşen ')' sonrasında '=>' var mı diye bakar.
// Lambda ile parantezli ifadeyi ayırt etmenin tek güvenilir yolu bu.
bool lambdaMi(const std::vector<Token>& t, std::size_t i) {
    int derinlik = 0;
    for (std::size_t k = i; k < t.size(); ++k) {
        switch (t[k].kind) {
            case Tok::LParen: case Tok::LBracket: case Tok::LBrace:
                ++derinlik;
                break;
            case Tok::RParen: case Tok::RBracket: case Tok::RBrace:
                --derinlik;
                if (derinlik == 0) {
                    return k + 1 < t.size() && t[k + 1].kind == Tok::FatArrow;
                }
                break;
            case Tok::Eof:
                return false;
            default:
                break;
        }
    }
    return false;
}

// Bir aralığın üst ucu var mı? '2..' ve '2..]' ayrımı için.
bool ifadeBaslayabilir(Tok k) {
    switch (k) {
        case Tok::RBracket: case Tok::RParen: case Tok::RBrace:
        case Tok::Comma: case Tok::Newline: case Tok::Colon: case Tok::Eof:
            return false;
        default:
            return true;
    }
}

}  // namespace

// --- kademe 0: koşullu ifade (sağ birleşmeli) ---
ExprPtr Parser::expression() {
    auto sol = orExpr();
    if (!check(Tok::KwIf)) {
        return sol;
    }
    advance();
    auto t = make<Ternary>(sol ? sol->span : previous().span);
    t->thenExpr = std::move(sol);
    t->cond = orExpr();
    expect(Tok::KwElse, "'else'");
    t->elseExpr = expression();   // sağ birleşmeli
    if (t->elseExpr) {
        t->span = Span::merge(t->span, t->elseExpr->span);
    }
    return t;
}

// --- kademe 1: or ---
ExprPtr Parser::orExpr() {
    auto sol = andExpr();
    while (match(Tok::KwOr)) {
        auto n = make<Logical>(sol ? sol->span : previous().span);
        n->op = Tok::KwOr;
        n->left = std::move(sol);
        n->right = andExpr();
        if (n->right) { n->span = Span::merge(n->span, n->right->span); }
        sol = std::move(n);
    }
    return sol;
}

// --- kademe 2: and ---
ExprPtr Parser::andExpr() {
    auto sol = notExpr();
    while (match(Tok::KwAnd)) {
        auto n = make<Logical>(sol ? sol->span : previous().span);
        n->op = Tok::KwAnd;
        n->left = std::move(sol);
        n->right = notExpr();
        if (n->right) { n->span = Span::merge(n->span, n->right->span); }
        sol = std::move(n);
    }
    return sol;
}

// --- kademe 3: not (önek, sağ birleşmeli) ---
ExprPtr Parser::notExpr() {
    if (match(Tok::KwNot)) {
        auto n = make<Unary>(previous().span);
        n->op = Tok::KwNot;
        n->operand = notExpr();
        if (n->operand) { n->span = Span::merge(n->span, n->operand->span); }
        return n;
    }
    return comparison();
}

// --- kademe 4: karşılaştırma ---
ExprPtr Parser::comparison() {
    auto sol = rangeExpr();
    while (matchAny({Tok::EqEq, Tok::BangEq, Tok::Lt, Tok::Gt, Tok::LtEq, Tok::GtEq,
                     Tok::KwIs, Tok::KwIn})) {
        const Tok op = previous().kind;
        auto n = make<Binary>(sol ? sol->span : previous().span);
        n->op = op;
        n->left = std::move(sol);
        n->right = rangeExpr();
        if (n->right) { n->span = Span::merge(n->span, n->right->span); }
        sol = std::move(n);
    }
    return sol;
}

// --- kademe 5: aralık ---
ExprPtr Parser::rangeExpr() {
    auto sol = bitOr();
    if (check(Tok::DotDot) || check(Tok::DotDotEq)) {
        const bool kapsayan = peek().kind == Tok::DotDotEq;
        advance();
        auto r = make<RangeExpr>(sol ? sol->span : previous().span);
        r->inclusive = kapsayan;
        r->lo = std::move(sol);
        if (ifadeBaslayabilir(peek().kind)) {   // '2..' açık uçlu olabilir
            r->hi = bitOr();
            if (r->hi) { r->span = Span::merge(r->span, r->hi->span); }
        }
        return r;
    }
    return sol;
}

// --- kademe 6-9: bit operatörleri ---
//
// Lexer bu token'ları en baştan üretiyordu ve yorumlayıcıda karşılıkları vardı,
// ama gramerde hiçbir kademe onları okumuyordu: 'bayrak & 4' yazan betik
// "ifade bekleniyordu, '&' bulundu" alıyordu ve iki katmandaki kod ulaşılamaz
// duruyordu. Öncelik sırası Python/C ile aynı: | < ^ < & < kaydırma < + -.
// Böylece 'bayraklar & MASKE == 0' gibi bir ifadede karşılaştırma en dışta kalır.
ExprPtr Parser::bitOr() {
    auto sol = bitXor();
    while (matchAny({Tok::Pipe})) {
        auto n = make<Binary>(sol ? sol->span : previous().span);
        n->op = Tok::Pipe;
        n->left = std::move(sol);
        n->right = bitXor();
        if (n->right) { n->span = Span::merge(n->span, n->right->span); }
        sol = std::move(n);
    }
    return sol;
}

ExprPtr Parser::bitXor() {
    auto sol = bitAnd();
    while (matchAny({Tok::Caret})) {
        auto n = make<Binary>(sol ? sol->span : previous().span);
        n->op = Tok::Caret;
        n->left = std::move(sol);
        n->right = bitAnd();
        if (n->right) { n->span = Span::merge(n->span, n->right->span); }
        sol = std::move(n);
    }
    return sol;
}

ExprPtr Parser::bitAnd() {
    auto sol = shift();
    while (matchAny({Tok::Amp})) {
        auto n = make<Binary>(sol ? sol->span : previous().span);
        n->op = Tok::Amp;
        n->left = std::move(sol);
        n->right = shift();
        if (n->right) { n->span = Span::merge(n->span, n->right->span); }
        sol = std::move(n);
    }
    return sol;
}

ExprPtr Parser::shift() {
    auto sol = sum();
    while (matchAny({Tok::LtLt, Tok::GtGt})) {
        const Tok op = previous().kind;
        auto n = make<Binary>(sol ? sol->span : previous().span);
        n->op = op;
        n->left = std::move(sol);
        n->right = sum();
        if (n->right) { n->span = Span::merge(n->span, n->right->span); }
        sol = std::move(n);
    }
    return sol;
}

// --- kademe 10: + - ---
ExprPtr Parser::sum() {
    auto sol = product();
    while (matchAny({Tok::Plus, Tok::Minus})) {
        const Tok op = previous().kind;
        auto n = make<Binary>(sol ? sol->span : previous().span);
        n->op = op;
        n->left = std::move(sol);
        n->right = product();
        if (n->right) { n->span = Span::merge(n->span, n->right->span); }
        sol = std::move(n);
    }
    return sol;
}

// --- kademe 11: * / // % ---
ExprPtr Parser::product() {
    auto sol = unary();
    while (matchAny({Tok::Star, Tok::Slash, Tok::SlashSlash, Tok::Percent})) {
        const Tok op = previous().kind;
        auto n = make<Binary>(sol ? sol->span : previous().span);
        n->op = op;
        n->left = std::move(sol);
        n->right = unary();
        if (n->right) { n->span = Span::merge(n->span, n->right->span); }
        sol = std::move(n);
    }
    return sol;
}

// --- kademe 8: tekli - + ~ await ---
ExprPtr Parser::unary() {
    if (matchAny({Tok::Minus, Tok::Plus, Tok::Tilde, Tok::KwAwait})) {
        const Tok op = previous().kind;
        auto n = make<Unary>(previous().span);
        n->op = op;
        n->operand = unary();
        if (n->operand) { n->span = Span::merge(n->span, n->operand->span); }
        return n;
    }
    return power();
}

// --- kademe 9: ** (SAĞ birleşmeli: 2**3**2 == 2**9) ---
ExprPtr Parser::power() {
    auto sol = postfix();
    if (match(Tok::StarStar)) {
        auto n = make<Binary>(sol ? sol->span : previous().span);
        n->op = Tok::StarStar;
        n->left = std::move(sol);
        n->right = unary();   // sağa iner → sağ birleşme
        if (n->right) { n->span = Span::merge(n->span, n->right->span); }
        return n;
    }
    return sol;
}

// --- kademe 10: sonek () [] . ?. ---
ExprPtr Parser::postfix() {
    auto e = primary();
    while (true) {
        if (match(Tok::LParen)) {
            auto c = make<Call>(e ? e->span : previous().span);
            c->callee = std::move(e);
            if (!check(Tok::RParen)) {
                do {
                    c->args.push_back(expression());
                } while (match(Tok::Comma));
            }
            const Token& kapa = expect(Tok::RParen, "')'");
            c->span = Span::merge(c->span, kapa.span);
            e = std::move(c);
        } else if (match(Tok::LBracket)) {
            auto ix = make<IndexExpr>(e ? e->span : previous().span);
            ix->object = std::move(e);

            if (check(Tok::DotDot) || check(Tok::DotDotEq)) {
                // '[..3]' — alt ucu olmayan dilim
                const bool kapsayan = peek().kind == Tok::DotDotEq;
                advance();
                auto r = make<RangeExpr>(previous().span);
                r->inclusive = kapsayan;
                if (ifadeBaslayabilir(peek().kind)) {
                    r->hi = sum();
                }
                ix->index = std::move(r);
            } else {
                ix->index = expression();
            }

            const Token& kapa = expect(Tok::RBracket, "']'");
            ix->span = Span::merge(ix->span, kapa.span);
            e = std::move(ix);
        } else if (check(Tok::Dot) || check(Tok::QuestionDot)) {
            const bool guvenli = peek().kind == Tok::QuestionDot;
            advance();
            auto m = make<Member>(e ? e->span : previous().span);
            m->object = std::move(e);
            m->safe = guvenli;
            const Token& ad = expect(Tok::Ident, "alan veya metot adı");
            m->name = ad.text;
            m->span = Span::merge(m->span, ad.span);
            e = std::move(m);
        } else {
            break;
        }
    }
    return e;
}

// --- yaprak ---
ExprPtr Parser::primary() {
    const Token& t = peek();

    switch (t.kind) {
        case Tok::Int: {
            advance();
            auto n = make<IntLit>(t.span);
            n->value = t.ival;
            return n;
        }
        case Tok::Float: {
            advance();
            auto n = make<FloatLit>(t.span);
            n->value = t.fval;
            return n;
        }
        case Tok::Str: case Tok::RawStr: {
            advance();
            auto n = make<StrLit>(t.span);
            n->value = t.text;
            n->raw = (t.kind == Tok::RawStr);
            return n;
        }
        case Tok::FStr:
            advance();
            return fstring(t);
        case Tok::KwTrue: case Tok::KwFalse: {
            advance();
            auto n = make<BoolLit>(t.span);
            n->value = (t.kind == Tok::KwTrue);
            return n;
        }
        case Tok::KwNil:
            advance();
            return make<NilLit>(t.span);
        case Tok::KwSelf:
            advance();
            return make<SelfExpr>(t.span);
        case Tok::KwSuper:
            advance();
            return make<SuperExpr>(t.span);
        case Tok::Ident: {
            advance();
            auto n = make<Ident>(t.span);
            n->name = t.text;
            return n;
        }
        case Tok::LParen:
            return lambdaOrGrouped();
        case Tok::LBracket:
            return listLiteral();
        case Tok::LBrace:
            return mapLiteral();
        default:
            break;
    }

    err(t, std::string("ifade bekleniyordu, '") + std::string(tokName(t.kind)) + "' bulundu");
    advance();
    return nullptr;
}

ExprPtr Parser::lambdaOrGrouped() {
    if (!lambdaMi(toks_, i_)) {
        advance();                       // '('
        auto e = expression();
        expect(Tok::RParen, "')'");
        return e;
    }

    const Token& bas = advance();        // '('
    auto l = make<Lambda>(bas.span);
    l->params = paramList();
    expect(Tok::RParen, "')'");
    expect(Tok::FatArrow, "'=>'");

    // SPEC §9 kural 4: '=>' sonrası '{' HER ZAMAN bloktur.
    if (check(Tok::LBrace)) {
        l->bodyBlock = braceBlock();
    } else {
        l->bodyExpr = expression();
        if (l->bodyExpr) { l->span = Span::merge(l->span, l->bodyExpr->span); }
    }
    return l;
}

ExprPtr Parser::listLiteral() {
    const Token& bas = advance();   // '['
    auto n = make<ListLit>(bas.span);
    skipNewlines();
    if (!check(Tok::RBracket)) {
        do {
            skipNewlines();
            if (check(Tok::RBracket)) { break; }   // sondaki virgüle izin ver
            n->items.push_back(expression());
            skipNewlines();
        } while (match(Tok::Comma));
    }
    const Token& kapa = expect(Tok::RBracket, "']'");
    n->span = Span::merge(n->span, kapa.span);
    return n;
}

ExprPtr Parser::mapLiteral() {
    const Token& bas = advance();   // '{'
    auto n = make<MapLit>(bas.span);
    skipNewlines();
    if (!check(Tok::RBrace)) {
        do {
            skipNewlines();
            if (check(Tok::RBrace)) { break; }     // sondaki virgüle izin ver
            auto anahtar = expression();
            expect(Tok::Colon, "':'");
            auto deger = expression();
            n->entries.emplace_back(std::move(anahtar), std::move(deger));
            skipNewlines();
        } while (match(Tok::Comma));
    }
    const Token& kapa = expect(Tok::RBrace, "'}'");
    n->span = Span::merge(n->span, kapa.span);
    return n;
}

// f-string gövdesini alt-lexer'la çözer (SPEC §9 kural 3).
// '{{' ve '}}' düz süslü parantez kaçışlarıdır.
ExprPtr Parser::fstring(const Token& t) {
    auto n = make<FStrLit>(t.span);
    n->raw = t.text;

    const std::string& s = t.text;
    std::string duz;

    for (std::size_t k = 0; k < s.size();) {
        if (s[k] == '{' && k + 1 < s.size() && s[k + 1] == '{') {
            duz += '{';
            k += 2;
            continue;
        }
        if (s[k] == '}' && k + 1 < s.size() && s[k + 1] == '}') {
            duz += '}';
            k += 2;
            continue;
        }
        if (s[k] != '{') {
            duz += s[k];
            ++k;
            continue;
        }

        // '{' — ifadeyi topla, iç içe süslü parantezleri ve tırnakları say.
        std::size_t j = k + 1;
        int derinlik = 1;
        char tirnak = '\0';
        while (j < s.size() && derinlik > 0) {
            const char c = s[j];
            if (tirnak != '\0') {
                if (c == tirnak) { tirnak = '\0'; }
            } else if (c == '"' || c == '\'') {
                tirnak = c;
            } else if (c == '{') {
                ++derinlik;
            } else if (c == '}') {
                --derinlik;
                if (derinlik == 0) { break; }
            }
            ++j;
        }
        if (derinlik != 0) {
            err(t, "f-string içinde kapanmamış '{'",
                "düz süslü parantez yazmak için '{{' kullan");
            break;
        }

        std::string ic = s.substr(k + 1, j - k - 1);

        // ':' sonrası biçim eki — ama tırnak/parantez içindeki ':' sayılmaz.
        std::string bicim;
        {
            int d2 = 0;
            char q2 = '\0';
            for (std::size_t m = 0; m < ic.size(); ++m) {
                const char c = ic[m];
                if (q2 != '\0') { if (c == q2) { q2 = '\0'; } continue; }
                if (c == '"' || c == '\'') { q2 = c; continue; }
                if (c == '(' || c == '[' || c == '{') { ++d2; continue; }
                if (c == ')' || c == ']' || c == '}') { --d2; continue; }
                if (c == ':' && d2 == 0) {
                    bicim = ic.substr(m + 1);
                    ic = ic.substr(0, m);
                    break;
                }
            }
        }

        if (!duz.empty()) {
            FStrLit::Part p;
            p.literal = std::move(duz);
            n->parts.push_back(std::move(p));
            duz.clear();
        }

        // Alt-lexer + alt-parser. Hatalar ana kaynaktaki f-string span'ine bağlanır
        // ki tanılama geçici Source'a işaret etmesin.
        Source alt("<f-string>", ic);
        Diagnostics altTani(alt);
        Lexer altLexer(alt, altTani);
        Parser altParser(alt, altLexer.tokenize(), altTani);
        auto ifade = altParser.expression();

        if (altTani.hasErrors() || !ifade) {
            std::string mesaj = "f-string içindeki ifade çözümlenemedi: {" + ic + "}";
            std::string ipucu;
            if (!altTani.all().empty()) {
                ipucu = altTani.all().front().message;
            }
            err(t, std::move(mesaj), std::move(ipucu));
        } else {
            FStrLit::Part p;
            p.expr = std::move(ifade);
            p.format = std::move(bicim);
            n->parts.push_back(std::move(p));
        }

        k = j + 1;
    }

    if (!duz.empty()) {
        FStrLit::Part p;
        p.literal = std::move(duz);
        n->parts.push_back(std::move(p));
    }
    return n;
}

}  // namespace rs
