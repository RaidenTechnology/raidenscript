// RaidenScript CLI
//
// Faz 1 ilerleyişi:
//   [x] 1. iskelet + kaynak/konum katmanı + tanılama motoru
//   [x] 2. lexer
//   [ ] 3. AST
//   [ ] 4. parser
//   [ ] 5. resolver
//   [ ] 6. yorumlayıcı
//   [ ] 7. REPL
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "astdump.hpp"
#include "diag.hpp"
#include "interp.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "resolver.hpp"
#include "source.hpp"
#include "token.hpp"
#include "repl.hpp" 

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {

constexpr std::string_view SURUM = "0.0.1-faz1";

// Windows konsolunu UTF-8 ve ANSI renklerine hazırlar.
// Bu olmadan "Yıldırım" bozuk çıkar ve renk kodları ham metin olarak görünür.
void konsoluHazirla() {
#ifdef _WIN32
    ::SetConsoleOutputCP(CP_UTF8);
    HANDLE h = ::GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mod = 0;
    if (h != INVALID_HANDLE_VALUE && ::GetConsoleMode(h, &mod)) {
        ::SetConsoleMode(h, mod | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif
}

void yardim() {
    std::cout
        << "RaidenScript " << SURUM << "\n"
           "\n"
           "KULLANIM:\n"
           "  rs run <dosya.rai>     bir dosyayı çalıştır\n"
           "  rs                     REPL başlat\n"
           "  rs --version           sürüm\n"
           "  rs --help              bu yardım\n"
           "\n"
           "GELİŞTİRME:\n"
           "  rs tokens <dosya.rai>  token dökümü\n"
           "  rs ast <dosya.rai>     sözdizimi ağacı dökümü\n"
           "  rs coz <dosya.rai>     isim çözümleme (resolver)\n"
           "  rs tani <dosya.rai>    tanılama motorunu dosya üzerinde göster\n";
}

int komutAst(const std::string& yol, bool sessiz) {
    auto kaynak = rs::Source::fromFile(yol);
    if (!kaynak) {
        std::cerr << "hata: dosya okunamadı: " << yol << '\n';
        return 1;
    }

    rs::Diagnostics tani(*kaynak);
    rs::Lexer lexer(*kaynak, tani);
    rs::Parser parser(*kaynak, lexer.tokenize(), tani);
    const rs::Program prog = parser.parseProgram();

    if (!sessiz) {
        rs::AstDumper dumper(std::cout, *kaynak);
        dumper.dump(prog);
        std::cout << '\n';
    }
    tani.print(std::cout);
    tani.printSummary(std::cout);
    return tani.hasErrors() ? 1 : 0;
}

int komutCoz(const std::string& yol) {
    auto kaynak = rs::Source::fromFile(yol);
    if (!kaynak) {
        std::cerr << "hata: dosya okunamadı: " << yol << '\n';
        return 1;
    }

    rs::Diagnostics tani(*kaynak);
    rs::Lexer lexer(*kaynak, tani);
    rs::Parser parser(*kaynak, lexer.tokenize(), tani);
    const rs::Program prog = parser.parseProgram();

    if (tani.hasErrors()) {
        tani.print(std::cout);
        tani.printSummary(std::cout);
        return 1;
    }

    rs::Resolver resolver(*kaynak, tani);
    resolver.resolve(prog);

    std::size_t yerel = 0, upvalue = 0, global = 0, host = 0;
    for (const auto& [dugum, r] : resolver.resolutions()) {
        (void)dugum;
        switch (r.kind) {
            case rs::Resolution::Kind::Local:   ++yerel;   break;
            case rs::Resolution::Kind::Upvalue: ++upvalue; break;
            case rs::Resolution::Kind::Global:  ++global;  break;
            case rs::Resolution::Kind::Host:    ++host;    break;
        }
    }

    tani.print(std::cout);
    std::cout << resolver.resolutions().size() << " isim çözüldü — "
              << yerel << " yerel, " << upvalue << " kapanış (upvalue), "
              << global << " global, " << host << " host\n";
    tani.printSummary(std::cout);
    return tani.hasErrors() ? 1 : 0;
}

int komutTokens(const std::string& yol) {
    auto kaynak = rs::Source::fromFile(yol);
    if (!kaynak) {
        std::cerr << "hata: dosya okunamadı: " << yol << '\n';
        return 1;
    }

    rs::Diagnostics tani(*kaynak);
    rs::Lexer lexer(*kaynak, tani);
    const auto tokenlar = lexer.tokenize();

    int girinti = 0;
    for (const auto& t : tokenlar) {
        if (t.kind == rs::Tok::Dedent) {
            --girinti;
        }

        const auto lc = kaynak->lineCol(t.span.offset);
        std::cout << std::setw(4) << lc.line << ':' << std::left << std::setw(4) << lc.col
                  << std::right << std::string(static_cast<std::size_t>(girinti > 0 ? girinti * 2 : 0), ' ')
                  << rs::tokName(t.kind);

        switch (t.kind) {
            case rs::Tok::Ident:
            case rs::Tok::Str:
            case rs::Tok::RawStr:
            case rs::Tok::FStr:
                std::cout << "  " << t.text;
                break;
            case rs::Tok::Int:
                std::cout << "  " << t.ival;
                break;
            case rs::Tok::Float:
                std::cout << "  " << t.fval;
                break;
            default:
                break;
        }
        std::cout << '\n';

        if (t.kind == rs::Tok::Indent) {
            ++girinti;
        }
    }

    std::cout << '\n' << tokenlar.size() << " token\n";
    tani.print(std::cout);
    tani.printSummary(std::cout);
    return tani.hasErrors() ? 1 : 0;
}

int komutRun(const std::string& yol) {
    auto kaynak = rs::Source::fromFile(yol);
    if (!kaynak) {
        std::cerr << "hata: dosya okunamadı: " << yol << '\n';
        return 1;
    }

    rs::Diagnostics tani(*kaynak);
    rs::Lexer lexer(*kaynak, tani);
    rs::Parser parser(*kaynak, lexer.tokenize(), tani);
    const rs::Program prog = parser.parseProgram();

    if (tani.hasErrors()) {
        tani.print(std::cerr);
        tani.printSummary(std::cerr);
        return 1;
    }

    // Resolver yalnızca DENETLER; uyarıları bastırıyoruz ki program çıktısı
    // temiz kalsın. Denetim için 'rs coz' var.
    rs::Diagnostics cozTani(*kaynak);
    rs::Resolver resolver(*kaynak, cozTani);
    resolver.resolve(prog);
    if (cozTani.hasErrors()) {
        cozTani.print(std::cerr);
        cozTani.printSummary(std::cerr);
        return 1;
    }

    rs::Interpreter yorumlayici(*kaynak, tani);
    const bool tamam = yorumlayici.run(prog);
    if (!tamam) {
        tani.print(std::cerr);
        tani.printSummary(std::cerr);
        return 1;
    }
    return 0;
}

// Tanılama motorunun gerçek bir dosya üzerinde nasıl göründüğünü gösterir.
// Lexer gelince bu komut kalkacak.
int komutTani(const std::string& yol) {
    auto kaynak = rs::Source::fromFile(yol);
    if (!kaynak) {
        std::cerr << "hata: dosya okunamadı: " << yol << '\n';
        return 1;
    }

    rs::Diagnostics tani(*kaynak);

    // Dosyadaki ilk 'fn' geçişini örnek bir konum olarak kullan.
    const std::string_view metin = kaynak->text();
    const std::size_t konum = metin.find("fn ");

    if (konum != std::string_view::npos) {
        const rs::Span span{static_cast<std::uint32_t>(konum), 2};
        tani.error(span, "örnek hata — tanılama motoru sınaması",
                   "ok işareti UTF-8 karakter sütununu takip ediyor, bayt ofsetini değil");
        tani.warning(span, "örnek uyarı, aynı konumda");
    } else {
        tani.error(rs::Span{0, 1}, "dosyada 'fn' bulunamadı");
    }

    tani.print(std::cout);
    tani.printSummary(std::cout);
    return 0;
}

int komutRepl() {
    rs::Repl repl;
    return repl.run();
}

}  // namespace

int main(int argc, char** argv) {
    konsoluHazirla();

    const std::vector<std::string> arg(argv + 1, argv + argc);

    if (arg.empty()) {
        return komutRepl();
    }

    const std::string& ilk = arg[0];

    if (ilk == "--help" || ilk == "-h") {
        yardim();
        return 0;
    }
    if (ilk == "--version" || ilk == "-v") {
        std::cout << "RaidenScript " << SURUM << '\n';
        return 0;
    }
    if (ilk == "run") {
        if (arg.size() < 2) {
            std::cerr << "hata: 'run' bir dosya adı bekliyor\n";
            return 1;
        }
        return komutRun(arg[1]);
    }
    if (ilk == "tokens") {
        if (arg.size() < 2) {
            std::cerr << "hata: 'tokens' bir dosya adı bekliyor\n";
            return 1;
        }
        return komutTokens(arg[1]);
    }
    if (ilk == "ast") {
        if (arg.size() < 2) {
            std::cerr << "hata: 'ast' bir dosya adı bekliyor\n";
            return 1;
        }
        const bool sessiz = arg.size() > 2 && arg[2] == "--sessiz";
        return komutAst(arg[1], sessiz);
    }
    if (ilk == "coz") {
        if (arg.size() < 2) {
            std::cerr << "hata: 'coz' bir dosya adı bekliyor\n";
            return 1;
        }
        return komutCoz(arg[1]);
    }
    if (ilk == "tani") {
        if (arg.size() < 2) {
            std::cerr << "hata: 'tani' bir dosya adı bekliyor\n";
            return 1;
        }
        return komutTani(arg[1]);
    }

    std::cerr << "hata: bilinmeyen komut '" << ilk << "'\n";
    yardim();
    return 1;
}
