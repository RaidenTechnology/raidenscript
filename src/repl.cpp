#include "repl.hpp"

#include <iostream>

#include "lexer.hpp"
#include "parser.hpp"
#include "resolver.hpp"

namespace rs {

Repl::Repl() {
    oturumKaynak_ = std::make_unique<Source>("<repl>", "");
    oturumTani_ = std::make_unique<Diagnostics>(*oturumKaynak_);
    yorumlayici_ = std::make_unique<Interpreter>(*oturumKaynak_, *oturumTani_);
}

// Satir bir blok aciyorsa (':' ile bitiyorsa) devam satiri isteyecegiz.
bool Repl::blokAciyorMu(const std::string& satir) {
    for (auto it = satir.rbegin(); it != satir.rend(); ++it) {
        if (*it == ' ' || *it == '\t' || *it == '\r') continue;
        if (*it == '#') break;          // yorum satiri
        return *it == ':';
    }
    return false;
}
void Repl::degerlendir(const std::string& kaynak) {
    kaynaklar_.push_back(std::make_unique<Source>("<repl>", kaynak));
    Source& src = *kaynaklar_.back();

    tanilar_.push_back(std::make_unique<Diagnostics>(src));
    Diagnostics& tani = *tanilar_.back();

    Lexer lexer(src, tani);
    Parser parser(src, lexer.tokenize(), tani);
    programlar_.push_back(std::make_unique<Program>(parser.parseProgram()));
    Program& prog = *programlar_.back();

    if (tani.hasErrors()) {
        tani.print(std::cout);
        return;
    }

    // Resolver'i REPL'de UYARISIZ kullaniyoruz: tek satirlik girdide
    // "kullanilmadi" uyarisi surekli tetiklenir ve gurultu yapar.
    Diagnostics cozTani(src);
    Resolver resolver(src, cozTani);
    resolver.resolve(prog);
    if (cozTani.hasErrors()) {
        cozTani.print(std::cout);
        return;
    }

    yorumlayici_->setDiagnostics(tani);
    if (!yorumlayici_->run(prog)) {
        tani.print(std::cout);
        return;
    }

    // Girdi tek bir ifade deyimiyse degerini yazdir.
    if (prog.stmts.size() == 1) {
        if (dynamic_cast<const ExprStmt*>(prog.stmts[0].get()) != nullptr) {
            const Value v = yorumlayici_->lastValue();
            if (!std::holds_alternative<Nil>(v)) {
                std::cout << toRepr(v) << '\n';
            }
        }
    }
}
int Repl::run() {
    std::cout << "RaidenScript 0.0.1-faz1\n"
              << "Cikis: .cik   (veya Ctrl+Z sonra Enter)\n\n";

    std::string tampon;
    std::string satir;

    while (true) {
        std::cout << (tampon.empty() ? ">>> " : "... ") << std::flush;
        if (!std::getline(std::cin, satir)) break;

        if (tampon.empty()) {
            if (satir == ".cik" || satir == ".çık") break;
            if (satir.empty()) continue;
        }

        tampon += satir;
        tampon += '\n';

        // Blok icindeysek bos satir gorene kadar okumaya devam et.
        const bool blokta = blokAciyorMu(satir) || (tampon.find('\n') != tampon.rfind('\n'));
        if (blokta && !satir.empty()) continue;

        degerlendir(tampon);
        tampon.clear();
    }

    std::cout << "\ngorusuruz\n";
    return 0;
}

}  // namespace rs