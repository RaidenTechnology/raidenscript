// Tanılama (hata/uyarı) motoru.
//
// SPEC §1 hedef 4: "Hata mesajları birinci sınıf vatandaş."
// Bu yüzden lexer'dan bile ÖNCE yazılıyor — sonradan eklenen hata altyapısı
// hep yamalı kalır.
#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

#include "source.hpp"

namespace rs {

enum class Severity : std::uint8_t { Error, Warning, Note };

struct Diagnostic {
    Severity severity = Severity::Error;
    Span span;
    std::string message;
    std::string hint;  // isteğe bağlı "= ipucu:" satırı
};

// Bir kaynak dosyaya ait tanılamaları toplar ve Rust tarzı basar:
//
//   hata: beklenmeyen karakter '$'
//     --> ornek.rai:3:9
//      |
//    3 |     x = $5
//      |         ^
//      |
//      = ipucu: sayı mı demek istedin?
//
class Diagnostics {
public:
    explicit Diagnostics(const Source& src) : src_(&src) {}

    void error(Span span, std::string mesaj, std::string ipucu = {});
    void warning(Span span, std::string mesaj, std::string ipucu = {});
    void note(Span span, std::string mesaj, std::string ipucu = {});

    [[nodiscard]] bool hasErrors() const noexcept { return errorCount_ > 0; }
    [[nodiscard]] std::size_t errorCount() const noexcept { return errorCount_; }
    [[nodiscard]] std::size_t warningCount() const noexcept { return warningCount_; }
    [[nodiscard]] const std::vector<Diagnostic>& all() const noexcept { return items_; }

    // Hepsini basar. Renk NO_COLOR ortam değişkeniyle kapatılabilir.
    void print(std::ostream& os) const;

    // "3 hata, 1 uyarı" gibi bir özet satırı.
    void printSummary(std::ostream& os) const;

    void clear() noexcept;

private:
    void add(Severity sev, Span span, std::string mesaj, std::string ipucu);
    void printOne(std::ostream& os, const Diagnostic& d, bool renk) const;

    const Source* src_;
    std::vector<Diagnostic> items_;
    std::size_t errorCount_ = 0;
    std::size_t warningCount_ = 0;
};

}  // namespace rs
