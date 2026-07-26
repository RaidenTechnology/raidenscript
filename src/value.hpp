// Çalışma zamanı değer temsili.
//
// Burada AST'nin aksine std::variant DOĞRU araç: alternatifler az, özyineleme
// shared_ptr ile kırılıyor ve tip üzerinde switch yapmak (std::visit) tam olarak
// yorumlayıcının ihtiyacı. AST'de 36 alternatif vardı, burada 12.
//
// Bellek: Faz 1'de shared_ptr. Çöp toplayıcı Faz 3'te gelecek — döngüsel
// referanslar (a.b = a) şimdilik sızdırır, bilinen ve kabul edilen sınırlama.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "ast.hpp"

namespace rs {

struct Nil {
    bool operator==(const Nil&) const noexcept { return true; }
};

struct ListObj;
struct MapObj;
struct FnObj;
struct NativeObj;
struct ClassObj;
struct InstanceObj;
struct BoundObj;

using Str = std::shared_ptr<const std::string>;  // string değişmez (SPEC §2)

using Value = std::variant<Nil,
                           bool,
                           std::int64_t,
                           double,
                           Str,
                           std::shared_ptr<ListObj>,
                           std::shared_ptr<MapObj>,
                           std::shared_ptr<FnObj>,
                           std::shared_ptr<NativeObj>,
                           std::shared_ptr<ClassObj>,
                           std::shared_ptr<InstanceObj>,
                           std::shared_ptr<BoundObj>>;

struct ListObj {
    std::vector<Value> items;
};

// Ekleme sırasını korur — 'for k, v in harita.items()' öngörülebilir olsun diye.
// Arama doğrusal; Faz 3'te hash tablosuna geçilecek.
struct MapObj {
    std::vector<std::pair<Value, Value>> entries;
};

class Environment;

struct FnObj {
    std::string name = "<anonim>";
    const std::vector<Param>* params = nullptr;
    const Stmt* body = nullptr;      // blok gövde
    const Expr* bodyExpr = nullptr;  // '(x) => x*2' tek ifade gövdesi
    std::shared_ptr<Environment> closure;
    bool isMethod = false;
    std::shared_ptr<ClassObj> owner;  // 'super' çözümü için
};

// std::function, ham işaretçi değil: yerleşik metotların alıcıyı ('merhaba'.upper()
// içindeki string'i) ve yorumlayıcıyı yakalaması gerekiyor.
using NativeFn = std::function<Value(std::vector<Value>&)>;

struct NativeObj {
    std::string name;
    int arityMin = 0;
    int arityMax = -1;  // -1 = sınırsız
    NativeFn fn;
};

struct ClassObj {
    std::string name;
    std::shared_ptr<ClassObj> base;
    std::unordered_map<std::string, std::shared_ptr<FnObj>> methods;
    const ClassDecl* decl = nullptr;
    std::shared_ptr<Environment> closure;

    [[nodiscard]] std::shared_ptr<FnObj> findMethod(const std::string& ad) const {
        const ClassObj* c = this;
        while (c != nullptr) {
            const auto it = c->methods.find(ad);
            if (it != c->methods.end()) {
                return it->second;
            }
            c = c->base.get();
        }
        return nullptr;
    }

    [[nodiscard]] bool isSubclassOf(const ClassObj* other) const {
        const ClassObj* c = this;
        while (c != nullptr) {
            if (c == other) {
                return true;
            }
            c = c->base.get();
        }
        return false;
    }
};

struct InstanceObj {
    std::shared_ptr<ClassObj> cls;
    std::vector<std::pair<std::string, Value>> fields;  // sıra korunur

    [[nodiscard]] Value* find(const std::string& ad) {
        for (auto& [k, v] : fields) {
            if (k == ad) {
                return &v;
            }
        }
        return nullptr;
    }
    void set(const std::string& ad, Value v) {
        if (auto* p = find(ad)) {
            *p = std::move(v);
            return;
        }
        fields.emplace_back(ad, std::move(v));
    }
};

// 'gemi.ateş' — metot, ait olduğu örnekle bağlanmış hâlde taşınabilir.
struct BoundObj {
    Value self;
    std::shared_ptr<FnObj> fn;
};

// --- kapsam ---

class Environment : public std::enable_shared_from_this<Environment> {
public:
    explicit Environment(std::shared_ptr<Environment> parent = nullptr)
        : parent_(std::move(parent)) {}

    void define(const std::string& ad, Value v) { vars_[ad] = std::move(v); }

    [[nodiscard]] Value* find(const std::string& ad) {
        Environment* e = this;
        while (e != nullptr) {
            const auto it = e->vars_.find(ad);
            if (it != e->vars_.end()) {
                return &it->second;
            }
            e = e->parent_.get();
        }
        return nullptr;
    }

    // 'outer x = ...' — mevcut kapsamı ATLAYIP dışarıda arar.
    [[nodiscard]] Value* findOuter(const std::string& ad) {
        return parent_ ? parent_->find(ad) : nullptr;
    }

    [[nodiscard]] const std::shared_ptr<Environment>& parent() const noexcept { return parent_; }

private:
    std::unordered_map<std::string, Value> vars_;
    std::shared_ptr<Environment> parent_;
};

// --- yardımcılar ---

inline Str makeStr(std::string s) {
    return std::make_shared<const std::string>(std::move(s));
}

// SPEC §2: SADECE nil ve false yanlıştır. 0 ve "" DOĞRUdur.
// Bu, 'if count:' klasik hatasını kökten siler.
inline bool truthy(const Value& v) noexcept {
    if (std::holds_alternative<Nil>(v)) {
        return false;
    }
    if (const auto* b = std::get_if<bool>(&v)) {
        return *b;
    }
    return true;
}

[[nodiscard]] std::string typeName(const Value& v);
[[nodiscard]] std::string toDisplay(const Value& v);  // print için
[[nodiscard]] std::string toRepr(const Value& v);     // iç içe gösterim için
[[nodiscard]] bool valueEquals(const Value& a, const Value& b);

}  // namespace rs
