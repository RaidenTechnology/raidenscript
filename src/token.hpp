// Token tipleri.
//
// SPEC §7.1: 28 anahtar kelime + operatör olarak and/or/not/is.
// Bu sayı v1.0'a kadar 30'u geçmeyecek — yeni anahtar kelime eklemek bilinçli
// bir bütçe kararıdır, refleks değil.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "source.hpp"

namespace rs {

enum class Tok : std::uint8_t {
    // --- yapısal ---
    Newline,   // mantıksal satır sonu
    Indent,
    Dedent,
    Eof,

    // --- literaller / adlar ---
    Ident,
    Int,
    Float,
    Str,       // "..." veya '...' (kaçışlar çözülmüş)
    RawStr,    // r"..." (kaçış yok)
    FStr,      // f"..." — gövde HAM tutulur, parser alt-lexer'la tarar (SPEC §9 kural 3)

    // --- anahtar kelimeler (28) ---
    KwFn, KwClass, KwTrait, KwView, KwUse, KwAs, KwOuter,
    KwIf, KwElif, KwElse, KwWhile, KwFor, KwIn, KwBreak,
    KwContinue, KwReturn, KwTry, KwCatch, KwFinally, KwThrow,
    KwAsync, KwAwait, KwSelf, KwSuper, KwTrue, KwFalse, KwNil, KwPass,

    // --- operatör anahtar kelimeleri ---
    KwAnd, KwOr, KwNot, KwIs,

    // --- aritmetik ---
    Plus, Minus, Star, Slash, SlashSlash, Percent, StarStar,

    // --- atama (SPEC §2: bileşik atama deyimdir, ifade değil) ---
    Assign, PlusEq, MinusEq, StarEq, SlashEq, SlashSlashEq, PercentEq, StarStarEq,

    // --- karşılaştırma ---
    EqEq, BangEq, Lt, Gt, LtEq, GtEq,

    // --- diğer operatörler ---
    Dot, DotDot, DotDotEq, QuestionDot, Arrow, FatArrow,
    Question,   // 'T?' nullable tip (SPEC §4). Geçerliliğine parser karar verir.
    Comma, Colon, Semicolon, At, Tilde,
    Amp, Pipe, Caret, LtLt, GtGt,

    // --- ayraçlar ---
    LParen, RParen, LBracket, RBracket, LBrace, RBrace,
};

struct Token {
    Tok kind = Tok::Eof;
    Span span;

    // Ident / Str / RawStr / FStr için metin.
    // FStr'de tırnaklar arasındaki HAM gövde durur.
    std::string text;

    std::int64_t ival = 0;   // Tok::Int
    double fval = 0.0;       // Tok::Float

    // f-string üç tırnaklı mıydı, ham mıydı — parser alt-lexer'ı kurarken lazım.
    bool tripleQuoted = false;
};

// Hata mesajlarında ve token dökümünde kullanılan okunur ad.
[[nodiscard]] std::string_view tokName(Tok t) noexcept;

// Kaynakta göründüğü hâli ("fn", "+=", "(" ...). Literaller için boş döner.
[[nodiscard]] std::string_view tokLexeme(Tok t) noexcept;

// Bir tanımlayıcı anahtar kelime mi? Değilse nullopt.
[[nodiscard]] std::optional<Tok> keywordLookup(std::string_view s) noexcept;

}  // namespace rs
