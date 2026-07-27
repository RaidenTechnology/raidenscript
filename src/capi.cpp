// C API gövdesi — capi.h'daki sözleşmenin uygulaması.
//
// Buradaki asıl iş ömür yönetimidir. Native CLI'da (main.cpp komutRun) Source,
// AST ve Interpreter yığında yaşar ve fonksiyon bitince yok olur. Gömme'de bu
// çalışmaz: FnObj, AST düğümlerine HAM işaretçi tutuyor (value.hpp FnObj::body).
// rs_call, rs_eval'den çok sonra çağrılacağı için AST'nin o ana kadar yaşaması
// şart. Bu yüzden rs_vm hepsinin sahibidir ve üye SIRASI önemlidir.

#include "capi.h"

#include <cstdlib>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "diag.hpp"
#include "interp.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "resolver.hpp"
#include "source.hpp"

// Üyeler bildirim sırasının TERSİNE yıkılır. Aşağıdaki sıra şunu garanti eder:
// interp -> prog -> diag -> src. Değiştirme; interp'in ortamları AST'ye,
// Diagnostics de Source'a bakıyor.
struct rs_vm {
    std::unique_ptr<rs::Source> src;
    std::unique_ptr<rs::Diagnostics> diag;
    rs::Program prog;
    std::unique_ptr<rs::Interpreter> interp;

    std::vector<std::pair<std::string, std::string>> kayitli;
    rs_host_fn host = nullptr;
    void* user = nullptr;

    std::string sonHata;
    bool yuklendi = false;

    // Dize kanalı — yalnızca host geri çağrısı sürerken anlamlı.
    // aktifArg, yorumlayıcının yığınındaki argüman vektörünü gösterir;
    // sahibi biz değiliz, çağrı bitince mutlaka nullptr olmalı.
    const std::vector<rs::Interpreter::HostArg>* aktifArg = nullptr;
    std::string donenDize;
    bool donenDizeVar = false;
};

namespace {

// Tanı çıktısını metne alır. Renk kodları kapatılır — bu tampon bir terminale
// değil host'a gidiyor.
std::string taniMetni(const rs::Diagnostics& d) {
    const bool renkVardi = std::getenv("NO_COLOR") == nullptr;
#ifdef _WIN32
    if (renkVardi) { _putenv_s("NO_COLOR", "1"); }
#else
    if (renkVardi) { setenv("NO_COLOR", "1", 1); }
#endif
    std::ostringstream oss;
    d.print(oss);
    d.printSummary(oss);
#ifdef _WIN32
    if (renkVardi) { _putenv_s("NO_COLOR", ""); }
#else
    if (renkVardi) { unsetenv("NO_COLOR"); }
#endif
    return oss.str();
}

// Geri çağrı boyunca aktif argümanları yayınlar, çıkışta MUTLAKA temizler.
// Host geri çağrısı (özellikle JS/WASM tarafı) istisna fırlatabilir; elle
// sıfırlamak o yolda dangling bir işaretçi bırakırdı.
struct ArgKapsami {
    rs_vm* vm;
    ArgKapsami(rs_vm* v, const std::vector<rs::Interpreter::HostArg>& args) : vm(v) {
        vm->aktifArg = &args;
        vm->donenDize.clear();
        vm->donenDizeVar = false;
    }
    ~ArgKapsami() { vm->aktifArg = nullptr; }
    ArgKapsami(const ArgKapsami&) = delete;
    ArgKapsami& operator=(const ArgKapsami&) = delete;
};

}  // namespace

extern "C" {

rs_vm* rs_new(void) {
    return new (std::nothrow) rs_vm{};
}

void rs_free(rs_vm* vm) {
    delete vm;
}

void rs_set_host(rs_vm* vm, rs_host_fn cb, void* user) {
    if (vm == nullptr) {
        return;
    }
    vm->host = cb;
    vm->user = user;
}

int rs_register(rs_vm* vm, const char* modul, const char* fn) {
    if (vm == nullptr || modul == nullptr || fn == nullptr) {
        return 1;
    }
    if (vm->yuklendi) {
        vm->sonHata = "rs_register, rs_eval'den SONRA çağrıldı; "
                      "'include' satırları eval sırasında çözülüyor";
        return 1;
    }
    vm->kayitli.emplace_back(modul, fn);
    return 0;
}

int rs_eval(rs_vm* vm, const char* kaynak, const char* ad) {
    if (vm == nullptr || kaynak == nullptr) {
        return 1;
    }
    if (vm->yuklendi) {
        vm->sonHata = "bu VM'de zaten bir betik yüklü — bir VM = bir betik "
                      "(birden fazla betik için rs_new ile ayrı VM aç)";
        return 1;
    }
    vm->sonHata.clear();

    vm->src = std::make_unique<rs::Source>(ad != nullptr ? ad : "<gomulu>", kaynak);
    vm->diag = std::make_unique<rs::Diagnostics>(*vm->src);

    rs::Lexer lexer(*vm->src, *vm->diag);
    rs::Parser parser(*vm->src, lexer.tokenize(), *vm->diag);
    vm->prog = parser.parseProgram();
    if (vm->diag->hasErrors()) {
        vm->sonHata = taniMetni(*vm->diag);
        return 1;
    }

    // Resolver ayrı bir Diagnostics kullanır: uyarıları (host isimleri) betiğin
    // kendi çıktısına karıştırmayalım, ama HATALARI yine de bildirelim.
    rs::Diagnostics cozTani(*vm->src);
    rs::Resolver resolver(*vm->src, cozTani);
    resolver.resolve(vm->prog);
    if (cozTani.hasErrors()) {
        vm->sonHata = taniMetni(cozTani);
        return 1;
    }

    vm->interp = std::make_unique<rs::Interpreter>(*vm->src, *vm->diag);

    // Köprü, run'dan ÖNCE kurulmalı: 'include game' üst düzey bir deyim ve
    // run sırasında çözülüyor.
    if (vm->host != nullptr) {
        rs_host_fn cb = vm->host;
        void* user = vm->user;
        // vm'i yakalamak güvenli: lambda interp'in içinde yaşıyor, interp ise
        // rs_vm'in SON üyesi — yani vm'den önce yıkılıyor.
        vm->interp->setHostBridge(
            vm->kayitli,
            [vm, cb, user](const std::string& modul, const std::string& fn,
                           const std::vector<rs::Interpreter::HostArg>& args)
                -> rs::Interpreter::HostSonuc {
                std::vector<double> sayilar;
                sayilar.reserve(args.size());
                for (const auto& a : args) {
                    sayilar.push_back(a.sayi);
                }

                const ArgKapsami kapsam(vm, args);
                const double d = cb(modul.c_str(), fn.c_str(),
                                    sayilar.empty() ? nullptr : sayilar.data(),
                                    static_cast<int>(sayilar.size()), user);

                rs::Interpreter::HostSonuc s;
                if (vm->donenDizeVar) {
                    s.dizeMi = true;
                    s.dize = std::move(vm->donenDize);
                } else {
                    s.sayi = d;
                }
                return s;
            });
    }

    vm->yuklendi = true;
    if (!vm->interp->run(vm->prog)) {
        vm->sonHata = taniMetni(*vm->diag);
        return 1;
    }
    return 0;
}

int rs_call(rs_vm* vm, const char* fn, const double* args, int argc, double* out) {
    if (vm == nullptr || fn == nullptr) {
        return 1;
    }
    if (!vm->yuklendi || vm->interp == nullptr) {
        vm->sonHata = "önce rs_eval ile bir betik yükle";
        return 1;
    }
    if (argc < 0 || (argc > 0 && args == nullptr)) {
        vm->sonHata = "argüman sayısı ile dizi uyuşmuyor";
        return 1;
    }
    vm->sonHata.clear();

    std::vector<rs::Value> degerler;
    degerler.reserve(static_cast<std::size_t>(argc));
    for (int i = 0; i < argc; ++i) {
        degerler.emplace_back(args[i]);
    }

    // Çağrının hataları çağrı başına temiz bir Diagnostics'e yazılsın; aksi hâlde
    // eval'den kalan uyarılar her rs_call'da tekrar basılır.
    rs::Diagnostics cagriTani(*vm->src);
    vm->interp->setDiagnostics(cagriTani);

    rs::Value sonuc{rs::Nil{}};
    const bool tamam = vm->interp->callGlobal(fn, degerler, sonuc);

    vm->interp->setDiagnostics(*vm->diag);

    if (!tamam) {
        vm->sonHata = taniMetni(cagriTani);
        return 1;
    }
    if (out != nullptr) {
        *out = rs::toDouble(sonuc);
    }
    return 0;
}

const char* rs_last_error(rs_vm* vm) {
    if (vm == nullptr) {
        return "";
    }
    return vm->sonHata.c_str();
}

const char* rs_arg_str(rs_vm* vm, int i) {
    if (vm == nullptr || vm->aktifArg == nullptr || i < 0) {
        return nullptr;
    }
    if (static_cast<std::size_t>(i) >= vm->aktifArg->size()) {
        return nullptr;
    }
    const std::string* p = (*vm->aktifArg)[static_cast<std::size_t>(i)].dize;
    return p != nullptr ? p->c_str() : nullptr;
}

void rs_return_str(rs_vm* vm, const char* s) {
    if (vm == nullptr || vm->aktifArg == nullptr || s == nullptr) {
        return;
    }
    vm->donenDize = s;
    vm->donenDizeVar = true;
}

}  // extern "C"
