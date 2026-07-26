#include "value.hpp"

#include <cmath>
#include <sstream>

namespace rs {

std::string typeName(const Value& v) {
    return std::visit(
        [](auto&& x) -> std::string {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, Nil>) return "nil";
            else if constexpr (std::is_same_v<T, bool>) return "bool";
            else if constexpr (std::is_same_v<T, std::int64_t>) return "int";
            else if constexpr (std::is_same_v<T, double>) return "float";
            else if constexpr (std::is_same_v<T, Str>) return "str";
            else if constexpr (std::is_same_v<T, std::shared_ptr<ListObj>>) return "list";
            else if constexpr (std::is_same_v<T, std::shared_ptr<MapObj>>) return "map";
            else if constexpr (std::is_same_v<T, std::shared_ptr<FnObj>>) return "fn";
            else if constexpr (std::is_same_v<T, std::shared_ptr<NativeObj>>) return "fn";
            else if constexpr (std::is_same_v<T, std::shared_ptr<ClassObj>>) return "class";
            else if constexpr (std::is_same_v<T, std::shared_ptr<InstanceObj>>)
                return x ? x->cls->name : "instance";
            else return "fn";
        },
        v);
}

namespace {

// Ondalık sayıyı gereksiz sıfırlar olmadan yazar: 3.0 -> "3.0", 3.14 -> "3.14"
std::string floatToStr(double d) {
    if (std::isnan(d)) return "nan";
    if (std::isinf(d)) return d > 0 ? "inf" : "-inf";
    std::ostringstream ss;
    ss.precision(15);
    ss << d;
    std::string s = ss.str();
    if (s.find('.') == std::string::npos && s.find('e') == std::string::npos &&
        s.find("inf") == std::string::npos && s.find("nan") == std::string::npos) {
        s += ".0";
    }
    return s;
}

std::string stringify(const Value& v, bool repr) {
    return std::visit(
        [&](auto&& x) -> std::string {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, Nil>) {
                return "nil";
            } else if constexpr (std::is_same_v<T, bool>) {
                return x ? "true" : "false";
            } else if constexpr (std::is_same_v<T, std::int64_t>) {
                return std::to_string(x);
            } else if constexpr (std::is_same_v<T, double>) {
                return floatToStr(x);
            } else if constexpr (std::is_same_v<T, Str>) {
                // print("a") -> a   ama  print(["a"]) -> ["a"]
                return repr ? ("\"" + (x ? *x : std::string{}) + "\"") : (x ? *x : std::string{});
            } else if constexpr (std::is_same_v<T, std::shared_ptr<ListObj>>) {
                if (!x) return "[]";
                std::string s = "[";
                for (std::size_t i = 0; i < x->items.size(); ++i) {
                    if (i > 0) s += ", ";
                    s += stringify(x->items[i], true);
                }
                return s + "]";
            } else if constexpr (std::is_same_v<T, std::shared_ptr<MapObj>>) {
                if (!x) return "{}";
                std::string s = "{";
                for (std::size_t i = 0; i < x->entries.size(); ++i) {
                    if (i > 0) s += ", ";
                    s += stringify(x->entries[i].first, true);
                    s += ": ";
                    s += stringify(x->entries[i].second, true);
                }
                return s + "}";
            } else if constexpr (std::is_same_v<T, std::shared_ptr<FnObj>>) {
                return "<fn " + (x ? x->name : std::string("?")) + ">";
            } else if constexpr (std::is_same_v<T, std::shared_ptr<NativeObj>>) {
                return "<yerleşik " + (x ? x->name : std::string("?")) + ">";
            } else if constexpr (std::is_same_v<T, std::shared_ptr<ClassObj>>) {
                return "<class " + (x ? x->name : std::string("?")) + ">";
            } else if constexpr (std::is_same_v<T, std::shared_ptr<InstanceObj>>) {
                return "<" + (x ? x->cls->name : std::string("?")) + ">";
            } else {
                return "<bağlı metot>";
            }
        },
        v);
}

}  // namespace

std::string toDisplay(const Value& v) { return stringify(v, false); }
std::string toRepr(const Value& v) { return stringify(v, true); }

bool valueEquals(const Value& a, const Value& b) {
    // Sayılarda int/float karışık karşılaştırmaya izin ver: 1 == 1.0
    const auto sayi = [](const Value& v, double& out) -> bool {
        if (const auto* i = std::get_if<std::int64_t>(&v)) { out = static_cast<double>(*i); return true; }
        if (const auto* d = std::get_if<double>(&v)) { out = *d; return true; }
        return false;
    };
    double x = 0, y = 0;
    if (sayi(a, x) && sayi(b, y)) {
        return x == y;
    }

    if (a.index() != b.index()) {
        return false;
    }

    if (const auto* sa = std::get_if<Str>(&a)) {
        const auto* sb = std::get_if<Str>(&b);
        return *sa && *sb ? **sa == **sb : sa->get() == sb->get();
    }
    if (const auto* la = std::get_if<std::shared_ptr<ListObj>>(&a)) {
        const auto* lb = std::get_if<std::shared_ptr<ListObj>>(&b);
        if (!*la || !*lb) return la->get() == lb->get();
        if ((*la)->items.size() != (*lb)->items.size()) return false;
        for (std::size_t i = 0; i < (*la)->items.size(); ++i) {
            if (!valueEquals((*la)->items[i], (*lb)->items[i])) return false;
        }
        return true;
    }
    if (std::holds_alternative<Nil>(a)) return true;
    if (const auto* ba = std::get_if<bool>(&a)) return *ba == std::get<bool>(b);

    // Nesneler kimlik ile karşılaştırılır.
    return std::visit(
        [&](auto&& sol) -> bool {
            using T = std::decay_t<decltype(sol)>;
            if constexpr (std::is_same_v<T, Nil> || std::is_same_v<T, bool> ||
                          std::is_same_v<T, std::int64_t> || std::is_same_v<T, double> ||
                          std::is_same_v<T, Str>) {
                return false;
            } else {
                return sol == std::get<T>(b);
            }
        },
        a);
}

}  // namespace rs
