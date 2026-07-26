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

#include "diag.hpp"
#include "lexer.hpp"
#include "source.hpp"
#include "token.hpp"

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
           "  rs tani <dosya.rai>    tanılama motorunu dosya üzerinde göster\n";
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

    std::cout << kaynak->name() << "\n"
              << "  " << kaynak->size() << " bayt, "
              << kaynak->lineCount() << " satır\n\n";

    std::cout << "Lexer henüz yazılmadı (Faz 1 / adım 2).\n"
                 "Kaynak ve tanılama katmanı hazır: 'rs tani " << yol << "'\n";
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
    std::cout << "RaidenScript " << SURUM << " REPL\n"
              << "Yorumlayıcı henüz yok (Faz 1 / adım 6). Çıkış: Ctrl+C\n";
    return 0;
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
