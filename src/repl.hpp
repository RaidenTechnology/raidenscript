// Etkilesimli kabuk.
//
// Kritik tasarim noktasi: her turun Source/Diagnostics/Program nesneleri
// YASAMAYA DEVAM ETMELI. Cunku 1. turda tanimlanan bir fonksiyon (FnObj)
// govdesine `const Stmt*` isaretcisi tutuyor -- o AST silinirse 2. turda
// fonksiyonu cagirmak coker.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "diag.hpp"
#include "interp.hpp"
#include "source.hpp"

namespace rs {

class Repl {
public:
    Repl();
    int run();

private:
    void degerlendir(const std::string& kaynak);
    static bool blokAciyorMu(const std::string& satir);

    // Omur boyu tutulanlar -- yukaridaki nota bak.
    std::vector<std::unique_ptr<Source>> kaynaklar_;
    std::vector<std::unique_ptr<Diagnostics>> tanilar_;
    std::vector<std::unique_ptr<Program>> programlar_;

    std::unique_ptr<Source> oturumKaynak_;
    std::unique_ptr<Diagnostics> oturumTani_;
    std::unique_ptr<Interpreter> yorumlayici_;
};

}  // namespace rs