// Sözdizimsel çözümleyici.
//
// Deyimler: özyinelemeli iniş (recursive descent).
// İfadeler: öncelik tırmanışı — SPEC §8 tablosundaki 11 kademe, her biri bir
// fonksiyon. (Pratt'ın tablo sürücülü hâli yerine kademe zinciri seçildi:
// gramer sabit olduğu için tablo bir esneklik kazandırmıyor, okunabilirlik ise
// belirgin şekilde artıyor.)
//
// Hata kurtarma: hatadan sonra bir sonraki satır sonuna/blok çıkışına atlanır,
// böylece tek koşuda birden çok hata raporlanabilir.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ast.hpp"
#include "diag.hpp"
#include "source.hpp"
#include "token.hpp"

namespace rs {

class Parser {
public:
    Parser(const Source& src, std::vector<Token> tokens, Diagnostics& diag)
        : src_(&src), toks_(std::move(tokens)), diag_(&diag) {}

    [[nodiscard]] Program parseProgram();

private:
    // --- imleç ---
    [[nodiscard]] const Token& peek(std::size_t ileri = 0) const;
    [[nodiscard]] const Token& previous() const;
    [[nodiscard]] bool check(Tok k) const { return peek().kind == k; }
    [[nodiscard]] bool atEnd() const { return peek().kind == Tok::Eof; }
    const Token& advance();
    bool match(Tok k);
    bool matchAny(std::initializer_list<Tok> ks);
    const Token& expect(Tok k, std::string_view ne);

    void skipNewlines();
    void endOfStatement();   // NEWLINE | ';' | '}' önü | DEDENT önü | dosya sonu
    void synchronize();
    void err(const Token& t, std::string mesaj, std::string ipucu = {});

    // --- deyimler ---
    StmtPtr statement();
    StmtPtr simpleStatement();
    StmtPtr compoundStatement();
    StmtPtr block();                 // ':' NEWLINE INDENT ... DEDENT | ':' simple
    StmtPtr braceBlock();            // '{' ... '}'  (lambda gövdesi)
    StmtPtr ifStatement();
    StmtPtr whileStatement();
    StmtPtr forStatement();
    StmtPtr tryStatement();
    StmtPtr ImportStatement();  
    StmtPtr functionDecl(std::vector<Decorator> dekoratorler, bool isAsync);
    StmtPtr classDecl();
    StmtPtr traitDecl();

    std::vector<Param> paramList();
    std::vector<Decorator> decorators();

    // --- ifadeler: SPEC §8 öncelik kademeleri ---
    ExprPtr expression();      // 0  ternary
    ExprPtr orExpr();          // 1  or
    ExprPtr andExpr();         // 2  and
    ExprPtr notExpr();         // 3  not
    ExprPtr comparison();      // 4  == != < > <= >= is in
    ExprPtr rangeExpr();       // 5  .. ..=
    ExprPtr sum();             // 6  + -
    ExprPtr product();         // 7  * / // %
    ExprPtr unary();           // 8  - + ~ await
    ExprPtr power();           // 9  **  (sağ birleşmeli)
    ExprPtr postfix();         // 10 () [] . ?.
    ExprPtr primary();

    ExprPtr lambdaOrGrouped();       // '(' sonrası: lambda mı parantezli ifade mi
    ExprPtr listLiteral();
    ExprPtr mapLiteral();
    ExprPtr fstring(const Token& t);  // gövdeyi alt-lexer'la çözer

    // --- tipler ---
    TypePtr typeNode();

    // --- yardımcı ---
    [[nodiscard]] static bool assignable(const Expr& e) noexcept;
    template <class T> std::unique_ptr<T> make(Span span);

    const Source* src_;
    std::vector<Token> toks_;
    Diagnostics* diag_;
    std::size_t i_ = 0;
    int hataSayisi_ = 0;
    static constexpr int MAKS_HATA = 40;  // kaskad hataları sonsuza gitmesin
};

}  // namespace rs
