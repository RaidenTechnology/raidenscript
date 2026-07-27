#include "token.hpp"

#include <array>
#include <utility>

namespace rs {
namespace {

// Anahtar kelime tablosu. SPEC §7.1'deki 28 + 4 operatör anahtar kelimesi.
// Sıralı tutuluyor ki gözle denetlenebilsin; arama doğrusal ama tablo 32 girişlik.
constexpr std::array<std::pair<std::string_view, Tok>, 32> KEYWORDS{{
    {"and", Tok::KwAnd},
    {"as", Tok::KwAs},
    {"async", Tok::KwAsync},
    {"await", Tok::KwAwait},
    {"break", Tok::KwBreak},
    {"catch", Tok::KwCatch},
    {"class", Tok::KwClass},
    {"continue", Tok::KwContinue},
    {"elif", Tok::KwElif},
    {"else", Tok::KwElse},
    {"false", Tok::KwFalse},
    {"finally", Tok::KwFinally},
    {"fn", Tok::KwFn},
    {"for", Tok::KwFor},
    {"if", Tok::KwIf},
    {"import", Tok::KwImport},
    {"in", Tok::KwIn},
    {"is", Tok::KwIs},
    {"nil", Tok::KwNil},
    {"not", Tok::KwNot},
    {"or", Tok::KwOr},
    {"outer", Tok::KwOuter},
    {"pass", Tok::KwPass},
    {"return", Tok::KwReturn},
    {"self", Tok::KwSelf},
    {"super", Tok::KwSuper},
    {"throw", Tok::KwThrow},
    {"trait", Tok::KwTrait},
    {"true", Tok::KwTrue},
    {"try", Tok::KwTry},
    {"view", Tok::KwView},
    {"while", Tok::KwWhile},
}};

}  // namespace

std::optional<Tok> keywordLookup(std::string_view s) noexcept {
    for (const auto& [ad, tok] : KEYWORDS) {
        if (ad == s) {
            return tok;
        }
    }
    return std::nullopt;
}

std::string_view tokName(Tok t) noexcept {
    switch (t) {
        case Tok::Newline:      return "satır sonu";
        case Tok::Indent:       return "girinti";
        case Tok::Dedent:       return "girinti çıkışı";
        case Tok::Eof:          return "dosya sonu";
        case Tok::Ident:        return "tanımlayıcı";
        case Tok::Int:          return "tam sayı";
        case Tok::Float:        return "ondalık sayı";
        case Tok::Str:          return "metin";
        case Tok::RawStr:       return "ham metin";
        case Tok::FStr:         return "biçimli metin";
        default:                break;
    }
    const std::string_view lx = tokLexeme(t);
    return lx.empty() ? "bilinmeyen" : lx;
}

std::string_view tokLexeme(Tok t) noexcept {
    switch (t) {
        case Tok::KwFn:         return "fn";
        case Tok::KwClass:      return "class";
        case Tok::KwTrait:      return "trait";
        case Tok::KwView:       return "view";
        case Tok::KwImport:     return "import";
        case Tok::KwAs:         return "as";
        case Tok::KwOuter:      return "outer";
        case Tok::KwIf:         return "if";
        case Tok::KwElif:       return "elif";
        case Tok::KwElse:       return "else";
        case Tok::KwWhile:      return "while";
        case Tok::KwFor:        return "for";
        case Tok::KwIn:         return "in";
        case Tok::KwBreak:      return "break";
        case Tok::KwContinue:   return "continue";
        case Tok::KwReturn:     return "return";
        case Tok::KwTry:        return "try";
        case Tok::KwCatch:      return "catch";
        case Tok::KwFinally:    return "finally";
        case Tok::KwThrow:      return "throw";
        case Tok::KwAsync:      return "async";
        case Tok::KwAwait:      return "await";
        case Tok::KwSelf:       return "self";
        case Tok::KwSuper:      return "super";
        case Tok::KwTrue:       return "true";
        case Tok::KwFalse:      return "false";
        case Tok::KwNil:        return "nil";
        case Tok::KwPass:       return "pass";
        case Tok::KwAnd:        return "and";
        case Tok::KwOr:         return "or";
        case Tok::KwNot:        return "not";
        case Tok::KwIs:         return "is";

        case Tok::Plus:         return "+";
        case Tok::Minus:        return "-";
        case Tok::Star:         return "*";
        case Tok::Slash:        return "/";
        case Tok::SlashSlash:   return "//";
        case Tok::Percent:      return "%";
        case Tok::StarStar:     return "**";

        case Tok::Assign:       return "=";
        case Tok::PlusEq:       return "+=";
        case Tok::MinusEq:      return "-=";
        case Tok::StarEq:       return "*=";
        case Tok::SlashEq:      return "/=";
        case Tok::SlashSlashEq: return "//=";
        case Tok::PercentEq:    return "%=";
        case Tok::StarStarEq:   return "**=";

        case Tok::EqEq:         return "==";
        case Tok::BangEq:       return "!=";
        case Tok::Lt:           return "<";
        case Tok::Gt:           return ">";
        case Tok::LtEq:         return "<=";
        case Tok::GtEq:         return ">=";

        case Tok::Dot:          return ".";
        case Tok::DotDot:       return "..";
        case Tok::DotDotEq:     return "..=";
        case Tok::QuestionDot:  return "?.";
        case Tok::Question:     return "?";
        case Tok::Arrow:        return "->";
        case Tok::FatArrow:     return "=>";
        case Tok::Comma:        return ",";
        case Tok::Colon:        return ":";
        case Tok::Semicolon:    return ";";
        case Tok::At:           return "@";
        case Tok::Tilde:        return "~";
        case Tok::Amp:          return "&";
        case Tok::Pipe:         return "|";
        case Tok::Caret:        return "^";
        case Tok::LtLt:         return "<<";
        case Tok::GtGt:         return ">>";

        case Tok::LParen:       return "(";
        case Tok::RParen:       return ")";
        case Tok::LBracket:     return "[";
        case Tok::RBracket:     return "]";
        case Tok::LBrace:       return "{";
        case Tok::RBrace:       return "}";

        default:                return {};
    }
}

}  // namespace rs
