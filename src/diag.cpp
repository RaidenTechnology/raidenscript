#include "diag.hpp"

#include <cstdlib>
#include <ostream>
#include <string_view>

namespace rs {
namespace {

constexpr std::string_view KIRMIZI = "\033[1;31m";
constexpr std::string_view SARI = "\033[1;33m";
constexpr std::string_view MAVI = "\033[1;34m";
constexpr std::string_view CAMGOBEGI = "\033[1;36m";
constexpr std::string_view KALIN = "\033[1m";
constexpr std::string_view SIFIRLA = "\033[0m";

bool renkAcikMi() {
    // NO_COLOR sözleşmesi: değişken TANIMLIYSA renk kapalı.
    return std::getenv("NO_COLOR") == nullptr;
}

std::string_view etiket(Severity s) {
    switch (s) {
        case Severity::Error:   return "hata";
        case Severity::Warning: return "uyarı";
        case Severity::Note:    return "not";
    }
    return "hata";
}

std::string_view etiketRengi(Severity s) {
    switch (s) {
        case Severity::Error:   return KIRMIZI;
        case Severity::Warning: return SARI;
        case Severity::Note:    return CAMGOBEGI;
    }
    return KIRMIZI;
}

// UTF-8 farkındalıklı sütun -> ekran boşluğu. Sekmeleri sekme olarak korur ki
// ok işareti kaynaktaki gerçek hizayla örtüşsün.
std::string bosluklar(std::string_view satir, std::uint32_t sutun) {
    std::string out;
    std::uint32_t sayac = 1;
    for (std::size_t i = 0; i < satir.size() && sayac < sutun; ++i) {
        const auto b = static_cast<unsigned char>(satir[i]);
        if ((b & 0xC0) == 0x80) {
            continue;  // UTF-8 devam baytı, sütun saymaz
        }
        out += (satir[i] == '\t') ? '\t' : ' ';
        ++sayac;
    }
    return out;
}

}  // namespace

void Diagnostics::add(Severity sev, Span span, std::string mesaj, std::string ipucu) {
    items_.push_back(Diagnostic{sev, span, std::move(mesaj), std::move(ipucu)});
    if (sev == Severity::Error) {
        ++errorCount_;
    } else if (sev == Severity::Warning) {
        ++warningCount_;
    }
}

void Diagnostics::error(Span span, std::string mesaj, std::string ipucu) {
    add(Severity::Error, span, std::move(mesaj), std::move(ipucu));
}

void Diagnostics::warning(Span span, std::string mesaj, std::string ipucu) {
    add(Severity::Warning, span, std::move(mesaj), std::move(ipucu));
}

void Diagnostics::note(Span span, std::string mesaj, std::string ipucu) {
    add(Severity::Note, span, std::move(mesaj), std::move(ipucu));
}

void Diagnostics::clear() noexcept {
    items_.clear();
    errorCount_ = 0;
    warningCount_ = 0;
}

void Diagnostics::printOne(std::ostream& os, const Diagnostic& d, bool renk) const {
    const auto lc = src_->lineCol(d.span.offset);
    const std::string_view satir = src_->lineText(lc.line);

    const std::string satirNo = std::to_string(lc.line);
    const std::string oluk(satirNo.size(), ' ');  // "  |" hizası

    const auto R = [&](std::string_view kod) { return renk ? kod : std::string_view{}; };

    // hata: mesaj
    os << R(etiketRengi(d.severity)) << etiket(d.severity) << R(SIFIRLA)
       << R(KALIN) << ": " << d.message << R(SIFIRLA) << '\n';

    //   --> dosya:satir:sutun
    os << oluk << R(MAVI) << "--> " << R(SIFIRLA)
       << src_->name() << ':' << lc.line << ':' << lc.col << '\n';

    //    |
    os << oluk << R(MAVI) << " |" << R(SIFIRLA) << '\n';

    //  3 |     x = $5
    os << R(MAVI) << satirNo << " | " << R(SIFIRLA) << satir << '\n';

    //    |         ^^^
    const std::uint32_t isaretUzunluk = d.span.length > 0 ? d.span.length : 1;
    os << oluk << R(MAVI) << " | " << R(SIFIRLA) << bosluklar(satir, lc.col)
       << R(etiketRengi(d.severity));
    for (std::uint32_t i = 0; i < isaretUzunluk; ++i) {
        os << '^';
    }
    os << R(SIFIRLA) << '\n';

    if (!d.hint.empty()) {
        os << oluk << R(MAVI) << " |" << R(SIFIRLA) << '\n';
        os << oluk << R(MAVI) << " = " << R(SIFIRLA)
           << R(CAMGOBEGI) << "ipucu" << R(SIFIRLA) << ": " << d.hint << '\n';
    }
    os << '\n';
}

void Diagnostics::print(std::ostream& os) const {
    const bool renk = renkAcikMi();
    for (const auto& d : items_) {
        printOne(os, d, renk);
    }
}

void Diagnostics::printSummary(std::ostream& os) const {
    if (items_.empty()) {
        return;
    }
    const bool renk = renkAcikMi();
    const auto R = [&](std::string_view kod) { return renk ? kod : std::string_view{}; };

    if (errorCount_ > 0) {
        os << R(KIRMIZI) << errorCount_ << " hata" << R(SIFIRLA);
    }
    if (warningCount_ > 0) {
        if (errorCount_ > 0) {
            os << ", ";
        }
        os << R(SARI) << warningCount_ << " uyarı" << R(SIFIRLA);
    }
    os << '\n';
}

}  // namespace rs
