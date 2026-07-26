// Sözcüksel çözümleyici.
//
// SPEC §9'daki dört kuralı uygular:
//   1. Parantez derinliği girintiyi bastırır
//   2. brace_block içinde INDENT/DEDENT yok, NEWLINE var
//   3. f-string'in {...} bölümü alt-lexer'a bırakılır (gövde ham tutulur)
//   4. '=>' sonrası '{' HER ZAMAN bloktur, map literal'i değil
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "diag.hpp"
#include "source.hpp"
#include "token.hpp"

namespace rs {

class Lexer {
public:
    Lexer(const Source& src, Diagnostics& diag) noexcept : src_(&src), diag_(&diag) {}

    // Tüm kaynağı tarar. Hatalar diag'a yazılır; taramaya elden geldiğince
    // devam edilir ki kullanıcı tek seferde birden çok hata görsün.
    [[nodiscard]] std::vector<Token> tokenize();

private:
    // İç içe geçme türü. '{' iki farklı şey olabildiği için ayrı tutuluyor.
    enum class Nest : std::uint8_t { Paren, Bracket, MapBrace, BlockBrace };

    // --- imleç ---
    [[nodiscard]] bool eof() const noexcept { return i_ >= src_->size(); }
    [[nodiscard]] char peek(std::uint32_t ileri = 0) const noexcept;
    char advance() noexcept;
    bool match(char beklenen) noexcept;

    // --- üretim ---
    void emit(Tok kind, Span span);
    void emitValue(Token t);

    // --- katmanlar ---
    void handleIndentation();
    void skipInlineSpace();
    void scanToken();
    void scanIdentOrKeyword();
    void scanNumber();
    void scanString(char tirnak, bool ham, bool bicimli, std::uint32_t basla);
    void scanOperator();

    // --- yardımcılar ---
    [[nodiscard]] bool newlineAllowed() const noexcept;
    [[nodiscard]] bool indentTracked() const noexcept { return nesting_.empty(); }
    [[nodiscard]] Span here(std::uint32_t basla) const noexcept;
    void closeNest(Nest beklenen, Tok kind, Span span);

    const Source* src_;
    Diagnostics* diag_;

    std::uint32_t i_ = 0;
    std::vector<Token> out_;
    std::vector<std::uint32_t> indents_{0};
    std::vector<Nest> nesting_;
    bool lineStart_ = true;
    Tok lastSignificant_ = Tok::Newline;  // '=>' sonrası '{' ayrımı için
};

}  // namespace rs
