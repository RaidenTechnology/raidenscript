#include "lexer.hpp"

#include <cstdlib>
#include <string>

namespace rs {
namespace {

bool harfMi(char c) noexcept {
    const auto b = static_cast<unsigned char>(c);
    // ASCII harfler, '_' ve TÜM UTF-8 çok baytlıları (>= 0x80).
    // Bu sayede 'ateş', 'öğüşçı', 'yıldırım' geçerli tanımlayıcı olur —
    // Türkçe yazan biri için dilin en somut kolaylıklarından biri.
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || b >= 0x80;
}

bool rakamMi(char c) noexcept { return c >= '0' && c <= '9'; }

bool onaltilikMi(char c) noexcept {
    return rakamMi(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

// Kod noktasını UTF-8 olarak ekler (\u{...} kaçışı için).
void utf8Ekle(std::string& s, std::uint32_t cp) {
    if (cp < 0x80) {
        s += static_cast<char>(cp);
    } else if (cp < 0x800) {
        s += static_cast<char>(0xC0 | (cp >> 6));
        s += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        s += static_cast<char>(0xE0 | (cp >> 12));
        s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        s += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        s += static_cast<char>(0xF0 | (cp >> 18));
        s += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        s += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

}  // namespace

// ---------------------------------------------------------------- imleç

char Lexer::peek(std::uint32_t ileri) const noexcept {
    const std::uint32_t k = i_ + ileri;
    return k < src_->size() ? src_->text()[k] : '\0';
}

char Lexer::advance() noexcept {
    return eof() ? '\0' : src_->text()[i_++];
}

bool Lexer::match(char beklenen) noexcept {
    if (peek() != beklenen) {
        return false;
    }
    ++i_;
    return true;
}

Span Lexer::here(std::uint32_t basla) const noexcept {
    return Span{basla, i_ - basla};
}

// ---------------------------------------------------------------- üretim

void Lexer::emit(Tok kind, Span span) {
    Token t;
    t.kind = kind;
    t.span = span;
    emitValue(std::move(t));
}

void Lexer::emitValue(Token t) {
    // NEWLINE tekrarını yut: boş satırlar deyim ayracı üretmemeli.
    if (t.kind == Tok::Newline && (out_.empty() || out_.back().kind == Tok::Newline)) {
        return;
    }
    if (t.kind != Tok::Newline && t.kind != Tok::Indent && t.kind != Tok::Dedent) {
        lastSignificant_ = t.kind;
    }
    out_.push_back(std::move(t));
}

bool Lexer::newlineAllowed() const noexcept {
    // Kural 1: parantez/köşeli/map-süslü içinde satır sonu yutulur.
    // Kural 2: blok-süslü içinde satır sonu ANLAMLIDIR (deyimleri ayırır)
    //          ve girinti de izlenir (bkz indentTracked).
    return nesting_.empty() || nesting_.back() == Nest::BlockBrace;
}

// ---------------------------------------------------------------- girinti

void Lexer::handleIndentation() {
    const std::uint32_t satirBasi = i_;
    std::uint32_t genislik = 0;

    while (!eof()) {
        const char c = peek();
        if (c == ' ') {
            ++genislik;
            ++i_;
        } else if (c == '\t') {
            diag_->error(Span{i_, 1}, "girintide sekme karakteri",
                         "RaidenScript girintide sadece boşluk kabul eder; "
                         "editörünü 4 boşluk kullanacak şekilde ayarla");
            ++i_;
        } else {
            break;
        }
    }

    // Boş satır veya sadece yorum içeren satır: girinti hiç sayılmaz.
    if (eof() || peek() == '\n' || peek() == '#') {
        return;
    }

    if (genislik > indents_.back()) {
        indents_.push_back(genislik);
        emit(Tok::Indent, Span{satirBasi, genislik});
        return;
    }

    while (genislik < indents_.back()) {
        indents_.pop_back();
        emit(Tok::Dedent, Span{i_, 0});
    }

    if (genislik != indents_.back()) {
        diag_->error(Span{satirBasi, genislik},
                     "girinti hiçbir açık bloğa denk gelmiyor",
                     "beklenen girinti " + std::to_string(indents_.back()) +
                         " boşluk, bulunan " + std::to_string(genislik));
        indents_.back() = genislik;  // kurtarma: devam edebilmek için hizala
    }
}

void Lexer::skipInlineSpace() {
    while (!eof()) {
        const char c = peek();
        if (c == ' ' || c == '\t' || c == '\r') {
            ++i_;
        } else if (c == '#') {
            while (!eof() && peek() != '\n') {
                ++i_;
            }
        } else {
            break;
        }
    }
}

// ---------------------------------------------------------------- ana döngü

std::vector<Token> Lexer::tokenize() {
    while (true) {
        if (lineStart_ && indentTracked()) {
            handleIndentation();
            lineStart_ = false;
        }

        skipInlineSpace();

        if (eof()) {
            break;
        }

        if (peek() == '\n') {
            ++i_;
            if (newlineAllowed()) {
                emit(Tok::Newline, Span{i_ - 1, 1});
            }
            if (indentTracked()) {
                lineStart_ = true;
            }
            continue;
        }

        scanToken();
    }

    // Dosya sonu: son satırı kapat, açık blokları kapat.
    if (!out_.empty() && out_.back().kind != Tok::Newline) {
        emit(Tok::Newline, Span{src_->size(), 0});
    }
    while (indents_.size() > 1) {
        indents_.pop_back();
        emit(Tok::Dedent, Span{src_->size(), 0});
    }
    for (const auto n : nesting_) {
        const char* ad = (n == Nest::Paren) ? "(" : (n == Nest::Bracket) ? "[" : "{";
        diag_->error(Span{src_->size(), 0},
                     std::string("dosya sonunda kapanmamış '") + ad + "'");
    }
    emit(Tok::Eof, Span{src_->size(), 0});
    return std::move(out_);
}

// ---------------------------------------------------------------- token

void Lexer::scanToken() {
    const std::uint32_t basla = i_;
    const char c = peek();

    // r"..." ve f"..." önekleri
    if ((c == 'r' || c == 'f') && (peek(1) == '"' || peek(1) == '\'')) {
        const bool ham = (c == 'r');
        ++i_;
        const char tirnak = advance();
        scanString(tirnak, ham, !ham, basla);
        return;
    }

    if (harfMi(c)) {
        scanIdentOrKeyword();
        return;
    }
    if (rakamMi(c)) {
        scanNumber();
        return;
    }
    if (c == '"' || c == '\'') {
        ++i_;
        scanString(c, false, false, basla);
        return;
    }

    scanOperator();
}

void Lexer::scanIdentOrKeyword() {
    const std::uint32_t basla = i_;
    while (!eof() && (harfMi(peek()) || rakamMi(peek()))) {
        ++i_;
    }
    const std::string_view metin = src_->text().substr(basla, i_ - basla);

    if (const auto kw = keywordLookup(metin)) {
        emit(*kw, here(basla));
        return;
    }
    Token t;
    t.kind = Tok::Ident;
    t.span = here(basla);
    t.text = std::string(metin);
    emitValue(std::move(t));
}

void Lexer::scanNumber() {
    const std::uint32_t basla = i_;
    std::string ham;
    bool ondalik = false;

    // 0x / 0b önekleri
    if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X' || peek(1) == 'b' || peek(1) == 'B')) {
        const bool onaltilik = (peek(1) == 'x' || peek(1) == 'X');
        i_ += 2;
        const std::uint32_t basamakBasi = i_;
        while (!eof() && (peek() == '_' || (onaltilik ? onaltilikMi(peek()) : (peek() == '0' || peek() == '1')))) {
            if (peek() != '_') {
                ham += peek();
            }
            ++i_;
        }
        if (i_ == basamakBasi || ham.empty()) {
            diag_->error(here(basla), onaltilik ? "eksik onaltılık sayı" : "eksik ikilik sayı",
                         onaltilik ? "örnek: 0xFF" : "örnek: 0b1010");
            emit(Tok::Int, here(basla));
            return;
        }
        Token t;
        t.kind = Tok::Int;
        t.span = here(basla);
        t.ival = static_cast<std::int64_t>(std::strtoull(ham.c_str(), nullptr, onaltilik ? 16 : 2));
        emitValue(std::move(t));
        return;
    }

    while (!eof()) {
        const char c = peek();
        if (rakamMi(c)) {
            ham += c;
            ++i_;
        } else if (c == '_') {
            ++i_;  // okunabilirlik ayracı, değere girmez
        } else if (c == '.' && !ondalik && rakamMi(peek(1))) {
            // '1..10' aralığını bozmamak için: '.' ancak ardından RAKAM gelirse
            // ondalık noktadır. Aksi hâlde '..' operatörüne bırakılır.
            ondalik = true;
            ham += c;
            ++i_;
        } else if ((c == 'e' || c == 'E') &&
                   (rakamMi(peek(1)) || ((peek(1) == '+' || peek(1) == '-') && rakamMi(peek(2))))) {
            ondalik = true;
            ham += c;
            ++i_;
            if (peek() == '+' || peek() == '-') {
                ham += peek();
                ++i_;
            }
        } else {
            break;
        }
    }

    Token t;
    t.span = here(basla);
    if (ondalik) {
        t.kind = Tok::Float;
        t.fval = std::strtod(ham.c_str(), nullptr);
    } else {
        t.kind = Tok::Int;
        t.ival = static_cast<std::int64_t>(std::strtoll(ham.c_str(), nullptr, 10));
    }
    emitValue(std::move(t));
}

void Lexer::scanString(char tirnak, bool ham, bool bicimli, std::uint32_t basla) {
    // Üç tırnak mı? (çağrıldığında ilk tırnak zaten yutulmuş durumda)
    bool uclu = false;
    if (peek() == tirnak && peek(1) == tirnak) {
        i_ += 2;
        uclu = true;
    }

    std::string deger;
    bool kapandi = false;

    while (!eof()) {
        const char c = peek();

        if (c == tirnak) {
            if (!uclu) {
                ++i_;
                kapandi = true;
                break;
            }
            if (peek(1) == tirnak && peek(2) == tirnak) {
                i_ += 3;
                kapandi = true;
                break;
            }
            deger += c;
            ++i_;
            continue;
        }

        if (c == '\n' && !uclu) {
            break;  // tek satırlık metin satır sonunda kapanmalı
        }

        // f-string ve ham metinde kaçış çözülmez: gövde ham korunur.
        // f-string'in {...} bölümünü parser alt-lexer'la tarayacak (SPEC §9 kural 3).
        if (c == '\\' && !ham && !bicimli) {
            ++i_;
            const char k = advance();
            switch (k) {
                case 'n':  deger += '\n'; break;
                case 't':  deger += '\t'; break;
                case 'r':  deger += '\r'; break;
                case '0':  deger += '\0'; break;
                case '\\': deger += '\\'; break;
                case '"':  deger += '"';  break;
                case '\'': deger += '\''; break;
                case 'u': {
                    if (peek() != '{') {
                        diag_->error(Span{i_, 1}, "\\u sonrası '{' bekleniyordu",
                                     "örnek: \\u{1F600}");
                        break;
                    }
                    ++i_;
                    std::string onaltilik;
                    while (!eof() && peek() != '}' && onaltilikMi(peek())) {
                        onaltilik += advance();
                    }
                    if (!match('}')) {
                        diag_->error(Span{i_, 1}, "kapanmamış \\u{...} kaçışı");
                        break;
                    }
                    utf8Ekle(deger, static_cast<std::uint32_t>(std::strtoul(onaltilik.c_str(), nullptr, 16)));
                    break;
                }
                default:
                    diag_->error(Span{i_ - 1, 2},
                                 std::string("bilinmeyen kaçış dizisi '\\") + k + "'",
                                 "ters bölü karakterini olduğu gibi yazmak için '\\\\' kullan");
                    deger += k;
                    break;
            }
            continue;
        }

        deger += c;
        ++i_;
    }

    if (!kapandi) {
        diag_->error(Span{basla, 1}, "kapanmamış metin",
                     uclu ? "üç tırnak burada açılmış ve hiç kapanmamış"
                          : "tek satırlık metin aynı satırda kapanmalı");
    }

    Token t;
    t.kind = bicimli ? Tok::FStr : (ham ? Tok::RawStr : Tok::Str);
    t.span = here(basla);
    t.text = std::move(deger);
    t.tripleQuoted = uclu;
    emitValue(std::move(t));
}

void Lexer::closeNest(Nest beklenen, Tok kind, Span span) {
    if (nesting_.empty()) {
        diag_->error(span, std::string("eşleşmeyen '") + std::string(tokLexeme(kind)) + "'");
    } else if (nesting_.back() != beklenen) {
        diag_->error(span, std::string("yanlış kapanış '") + std::string(tokLexeme(kind)) + "'",
                     "içteki ayraç önce kapanmalı");
        nesting_.pop_back();
    } else {
        nesting_.pop_back();
    }
    emit(kind, span);
}

void Lexer::scanOperator() {
    const std::uint32_t basla = i_;
    const char c = advance();

    switch (c) {
        case '(': nesting_.push_back(Nest::Paren);   emit(Tok::LParen, here(basla)); return;
        case '[': nesting_.push_back(Nest::Bracket); emit(Tok::LBracket, here(basla)); return;
        case ')': closeNest(Nest::Paren,   Tok::RParen,   here(basla)); return;
        case ']': closeNest(Nest::Bracket, Tok::RBracket, here(basla)); return;

        case '{': {
            // KURAL 4: '=>' sonrası '{' HER ZAMAN bloktur.
            // Bu ayrım olmadan '(x) => {...}' ile map literal'i ayırt edilemez.
            const bool blok = (lastSignificant_ == Tok::FatArrow);
            nesting_.push_back(blok ? Nest::BlockBrace : Nest::MapBrace);
            emit(Tok::LBrace, here(basla));
            return;
        }
        case '}': {
            if (!nesting_.empty() && nesting_.back() == Nest::BlockBrace) {
                closeNest(Nest::BlockBrace, Tok::RBrace, here(basla));
            } else {
                closeNest(Nest::MapBrace, Tok::RBrace, here(basla));
            }
            return;
        }

        case '+': emit(match('=') ? Tok::PlusEq : Tok::Plus, here(basla)); return;
        case '~': emit(Tok::Tilde, here(basla)); return;
        case ',': emit(Tok::Comma, here(basla)); return;
        case ':': emit(Tok::Colon, here(basla)); return;
        case ';': emit(Tok::Semicolon, here(basla)); return;
        case '@': emit(Tok::At, here(basla)); return;
        case '&': emit(Tok::Amp, here(basla)); return;
        case '|': emit(Tok::Pipe, here(basla)); return;
        case '^': emit(Tok::Caret, here(basla)); return;

        case '-':
            if (match('>')) { emit(Tok::Arrow, here(basla)); return; }
            emit(match('=') ? Tok::MinusEq : Tok::Minus, here(basla));
            return;

        case '*':
            if (match('*')) { emit(match('=') ? Tok::StarStarEq : Tok::StarStar, here(basla)); return; }
            emit(match('=') ? Tok::StarEq : Tok::Star, here(basla));
            return;

        case '/':
            if (match('/')) { emit(match('=') ? Tok::SlashSlashEq : Tok::SlashSlash, here(basla)); return; }
            emit(match('=') ? Tok::SlashEq : Tok::Slash, here(basla));
            return;

        case '%': emit(match('=') ? Tok::PercentEq : Tok::Percent, here(basla)); return;

        case '=':
            if (match('>')) { emit(Tok::FatArrow, here(basla)); return; }
            emit(match('=') ? Tok::EqEq : Tok::Assign, here(basla));
            return;

        case '!':
            if (match('=')) { emit(Tok::BangEq, here(basla)); return; }
            diag_->error(here(basla), "beklenmeyen '!'", "olumsuzlama için 'not' kullan");
            return;

        case '<':
            if (match('<')) { emit(Tok::LtLt, here(basla)); return; }
            emit(match('=') ? Tok::LtEq : Tok::Lt, here(basla));
            return;

        case '>':
            if (match('>')) { emit(Tok::GtGt, here(basla)); return; }
            emit(match('=') ? Tok::GtEq : Tok::Gt, here(basla));
            return;

        case '.':
            if (match('.')) { emit(match('=') ? Tok::DotDotEq : Tok::DotDot, here(basla)); return; }
            emit(Tok::Dot, here(basla));
            return;

        case '?':
            // '?.' güvenli erişim, çıplak '?' ise nullable tip eki (SPEC §4: 'Oyuncu?').
            // Lexer hangisinin geçerli olduğuna karar VERMEZ — bağlamı parser bilir.
            if (match('.')) { emit(Tok::QuestionDot, here(basla)); return; }
            emit(Tok::Question, here(basla));
            return;

        default: {
            std::string mesaj = "beklenmeyen karakter";
            if (static_cast<unsigned char>(c) >= 0x20 && static_cast<unsigned char>(c) < 0x7F) {
                mesaj += std::string(" '") + c + "'";
            }
            diag_->error(here(basla), mesaj);
            return;
        }
    }
}

}  // namespace rs
