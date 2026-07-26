#include "source.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace rs {

Source::Source(std::string name, std::string text)
    : name_(std::move(name)), text_(std::move(text)) {
    lineStarts_.push_back(0);
    for (std::size_t i = 0; i < text_.size(); ++i) {
        if (text_[i] == '\n') {
            lineStarts_.push_back(static_cast<std::uint32_t>(i + 1));
        }
    }
}

std::optional<Source> Source::fromFile(const std::filesystem::path& yol) {
    std::ifstream f(yol, std::ios::binary);
    if (!f) {
        return std::nullopt;
    }
    std::ostringstream ss;
    ss << f.rdbuf();

    std::string icerik = ss.str();

    // UTF-8 BOM'u at. Windows editörleri (Not Defteri, PowerShell Out-File,
    // bazen VS Code) dosya başına EF BB BF koyar. Lexer bunu çok baytlı bir
    // karakter sanıp ilk tanımlayıcıya yapıştırıyordu — görünmez bir hata.
    if (icerik.size() >= 3 && static_cast<unsigned char>(icerik[0]) == 0xEF &&
        static_cast<unsigned char>(icerik[1]) == 0xBB &&
        static_cast<unsigned char>(icerik[2]) == 0xBF) {
        icerik.erase(0, 3);
    }

    // CRLF -> LF. Windows'ta yazılmış dosyalar lexer'a hep temiz gelsin.
    icerik.erase(std::remove(icerik.begin(), icerik.end(), '\r'), icerik.end());

    return Source(yol.string(), std::move(icerik));
}

Source::LineCol Source::lineCol(std::uint32_t offset) const {
    if (offset > size()) {
        offset = size();
    }

    // lineStarts_ sıralı; offset'ten büyük ilk girişin bir öncesi bizim satırımız.
    const auto it = std::upper_bound(lineStarts_.begin(), lineStarts_.end(), offset);
    const auto idx = static_cast<std::uint32_t>(std::distance(lineStarts_.begin(), it) - 1);

    const std::uint32_t satirBasi = lineStarts_[idx];

    // Sütunu UTF-8 karakter cinsinden say: devam baytlarını (10xxxxxx) atla.
    std::uint32_t sutun = 1;
    for (std::uint32_t i = satirBasi; i < offset; ++i) {
        const auto b = static_cast<unsigned char>(text_[i]);
        if ((b & 0xC0) != 0x80) {
            ++sutun;
        }
    }

    return LineCol{idx + 1, sutun};
}

std::string_view Source::lineText(std::uint32_t line) const {
    if (line == 0 || line > lineCount()) {
        return {};
    }
    const std::uint32_t bas = lineStarts_[line - 1];
    const std::uint32_t son = (line < lineCount()) ? lineStarts_[line] : size();

    std::uint32_t uzunluk = son - bas;
    // Sondaki \n'i kes.
    if (uzunluk > 0 && text_[bas + uzunluk - 1] == '\n') {
        --uzunluk;
    }
    return std::string_view(text_).substr(bas, uzunluk);
}

}  // namespace rs
