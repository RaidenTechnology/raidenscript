// Kaynak dosya ve konum takibi.
//
// Her token, AST düğümü ve hata bir Span taşır. Span'i sonradan eklemek acı
// verdiği için en baştan, en alt katmana konuyor.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rs {

// Kaynaktaki bir aralık. Satır/sütun DEĞİL — bayt ofseti; çeviri Source'ta yapılır.
struct Span {
    std::uint32_t offset = 0;
    std::uint32_t length = 0;

    [[nodiscard]] constexpr std::uint32_t end() const noexcept { return offset + length; }

    // İki span'i kapsayan en küçük span (a'nın başından b'nin sonuna).
    [[nodiscard]] static constexpr Span merge(Span a, Span b) noexcept {
        const std::uint32_t bas = a.offset < b.offset ? a.offset : b.offset;
        const std::uint32_t son = a.end() > b.end() ? a.end() : b.end();
        return Span{bas, son - bas};
    }
};

class Source {
public:
    struct LineCol {
        std::uint32_t line = 1;  // 1 tabanlı
        std::uint32_t col = 1;   // 1 tabanlı, UTF-8 KARAKTER cinsinden (bayt değil)
    };

    Source(std::string name, std::string text);

    // Dosyayı okur. Okunamazsa nullopt.
    static std::optional<Source> fromFile(const std::filesystem::path& yol);

    [[nodiscard]] std::string_view text() const noexcept { return text_; }
    [[nodiscard]] std::string_view name() const noexcept { return name_; }
    [[nodiscard]] std::uint32_t size() const noexcept {
        return static_cast<std::uint32_t>(text_.size());
    }
    [[nodiscard]] std::uint32_t lineCount() const noexcept {
        return static_cast<std::uint32_t>(lineStarts_.size());
    }

    // Bayt ofsetini satır/sütuna çevirir. Sütun UTF-8 karakter sayar, çünkü
    // "Yıldırım" gibi bir satırda bayt sütunu yanlış yeri gösterir.
    [[nodiscard]] LineCol lineCol(std::uint32_t offset) const;

    // 1 tabanlı satır numarasının metni (sondaki \n hariç).
    [[nodiscard]] std::string_view lineText(std::uint32_t line) const;

private:
    std::string name_;
    std::string text_;
    std::vector<std::uint32_t> lineStarts_;  // her satırın başlangıç ofseti
};

}  // namespace rs
