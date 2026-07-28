// C API koşumu — Faz 4 / adım 1.
//
// Bu dosya BİLEREK saf C API'sini kullanır: capi.h dışında RaidenScript başlığı
// dahil edilmez. Amaç host'un gördüğü yüzeyi test etmek; iç başlıklar sızarsa
// gömme sözleşmesi test edilmemiş olur.
//
// Koşum:  make test          (hızlı)
//         make DEBUG=1 test  (sanitizer — ömür hatalarını burada yakala)

#include "capi.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <string>

namespace {

int gecen = 0;
int kalan = 0;

void bildir(bool tamam, const std::string& ad, const std::string& ek = {}) {
    if (tamam) {
        ++gecen;
        std::cout << "  ok    " << ad << '\n';
    } else {
        ++kalan;
        std::cout << "  HATA  " << ad;
        if (!ek.empty()) {
            std::cout << "  <- " << ek;
        }
        std::cout << '\n';
    }
}

bool yakin(double a, double b) { return std::fabs(a - b) < 1e-9; }

// --- host geri çağrısı: kaç kez, hangi argümanlarla çağrıldığını kaydeder ---
struct HostKayit {
    int cagriSayisi = 0;
    std::string sonModul;
    std::string sonFn;
    int sonArgc = 0;
    double sonArg0 = 0.0;
};

double hostGeriCagri(const char* modul, const char* fn, const double* args, int argc,
                     void* user) {
    auto* k = static_cast<HostKayit*>(user);
    ++k->cagriSayisi;
    k->sonModul = modul != nullptr ? modul : "";
    k->sonFn = fn != nullptr ? fn : "";
    k->sonArgc = argc;
    k->sonArg0 = (argc > 0 && args != nullptr) ? args[0] : 0.0;
    return 42.0;  // betiğe dönen sonuç
}

// 1 — argümansız çağrı
void t1() {
    rs_vm* vm = rs_new();
    const int e = rs_eval(vm, "fn iki():\n    return 2\n", "t1.rai");
    double out = -1.0;
    const int c = rs_call(vm, "iki", nullptr, 0, &out);
    bildir(e == 0 && c == 0 && yakin(out, 2.0), "argumansiz cagri -> 2",
           e != 0 ? rs_last_error(vm) : (c != 0 ? rs_last_error(vm) : "deger " + std::to_string(out)));
    rs_free(vm);
}

// 2 — argümanlı çağrı
void t2() {
    rs_vm* vm = rs_new();
    const int e = rs_eval(vm, "fn topla(a, b):\n    return a + b\n", "t2.rai");
    const double args[2] = {3.0, 4.0};
    double out = -1.0;
    const int c = rs_call(vm, "topla", args, 2, &out);
    bildir(e == 0 && c == 0 && yakin(out, 7.0), "argumanli cagri 3+4 -> 7",
           c != 0 ? rs_last_error(vm) : "deger " + std::to_string(out));
    rs_free(vm);
}

// 3 — sözdizimi hatası tam tanı metni döndürmeli
void t3() {
    rs_vm* vm = rs_new();
    const int e = rs_eval(vm, "c = 5 ! 3\n", "t3.rai");
    const std::string h = rs_last_error(vm);
    const bool tamam = e != 0 && h.find("hata:") != std::string::npos &&
                       h.find("-->") != std::string::npos && h.find('^') != std::string::npos &&
                       h.find("ipucu:") != std::string::npos;
    bildir(tamam, "sozdizimi hatasi: konum + ok + ipucu", tamam ? "" : h);
    rs_free(vm);
}

// 4 — olmayan fonksiyon
void t4() {
    rs_vm* vm = rs_new();
    rs_eval(vm, "fn var_olan():\n    return 1\n", "t4.rai");
    double out = 0.0;
    const int c = rs_call(vm, "yok_boyle_bir_sey", nullptr, 0, &out);
    const std::string h = rs_last_error(vm);
    bildir(c != 0 && h.find("yok_boyle_bir_sey") != std::string::npos,
           "olmayan fonksiyon anlamli hata veriyor", h);
    rs_free(vm);
}

// 5 — host köprüsü: include game -> game.zar(5) -> geri çağrı
void t5() {
    HostKayit kayit;
    rs_vm* vm = rs_new();
    rs_set_host(vm, hostGeriCagri, &kayit);
    rs_register(vm, "game", "zar");

    const char* kaynak =
        "include game\n"
        "\n"
        "fn at():\n"
        "    return game.zar(5)\n";
    const int e = rs_eval(vm, kaynak, "t5.rai");
    double out = 0.0;
    const int c = rs_call(vm, "at", nullptr, 0, &out);

    const bool tamam = e == 0 && c == 0 && kayit.cagriSayisi == 1 && kayit.sonModul == "game" &&
                       kayit.sonFn == "zar" && kayit.sonArgc == 1 && yakin(kayit.sonArg0, 5.0) &&
                       yakin(out, 42.0);
    bildir(tamam, "host koprusu: include game -> game.zar(5)",
           tamam ? "" : (e != 0 || c != 0 ? rs_last_error(vm)
                                          : "cagri=" + std::to_string(kayit.cagriSayisi) +
                                                " fn=" + kayit.sonFn +
                                                " arg0=" + std::to_string(kayit.sonArg0) +
                                                " out=" + std::to_string(out)));
    rs_free(vm);
}

// 6 — bir VM = bir betik
void t6() {
    rs_vm* vm = rs_new();
    const int e1 = rs_eval(vm, "fn a():\n    return 1\n", "t6a.rai");
    const int e2 = rs_eval(vm, "fn b():\n    return 2\n", "t6b.rai");
    bildir(e1 == 0 && e2 != 0, "ikinci rs_eval reddediliyor", rs_last_error(vm));
    rs_free(vm);
}

// 7 — kayıtsız modül include edilirse eski davranış: uyarı + nil, çökme yok
void t7() {
    rs_vm* vm = rs_new();
    const int e = rs_eval(vm, "include bilinmeyen\n\nfn n():\n    return 9\n", "t7.rai");
    double out = 0.0;
    const int c = rs_call(vm, "n", nullptr, 0, &out);
    bildir(e == 0 && c == 0 && yakin(out, 9.0), "kayitsiz include: uyari + nil, cokme yok",
           rs_last_error(vm));
    rs_free(vm);
}

// 8 — betikten fırlatılan istisna hata olarak dönmeli, çökmemeli
void t8() {
    rs_vm* vm = rs_new();
    const int e = rs_eval(vm, "fn patla():\n    throw Error(\"olmadi\")\n", "t8.rai");
    double out = 0.0;
    const int c = rs_call(vm, "patla", nullptr, 0, &out);
    bildir(e == 0 && c != 0, "betik istisnasi hata olarak donuyor", rs_last_error(vm));
    rs_free(vm);
}

// 9 — aynı VM'de arka arkaya çağrılar (ömür: AST hâlâ yaşıyor mu)
void t9() {
    rs_vm* vm = rs_new();
    rs_eval(vm, "fn kare(x):\n    return x * x\n", "t9.rai");
    bool tamam = true;
    for (int i = 1; i <= 200; ++i) {
        const double a = static_cast<double>(i);
        double out = 0.0;
        if (rs_call(vm, "kare", &a, 1, &out) != 0 || !yakin(out, a * a)) {
            tamam = false;
            break;
        }
    }
    bildir(tamam, "200 ardisik cagri (AST omru)", rs_last_error(vm));
    rs_free(vm);
}

// --- dize kanalı (adım 5) ---

// Betikten gelen dizeyi okur, ters çevirip geri döndürür. Ayrıca sayı
// argümanların args[] yolundan gelmeye devam ettiğini doğrular.
struct DizeKayit {
    std::string gelen;
    bool ikinciSayiMi = false;
    double ikinciSayi = 0.0;
    bool ucuncuNullMu = false;
};

rs_vm* aktifVm = nullptr;

double dizeHost(const char* modul, const char* fn, const double* args, int argc, void* user) {
    (void)modul;
    auto* k = static_cast<DizeKayit*>(user);

    const char* s = rs_arg_str(aktifVm, 0);
    k->gelen = s != nullptr ? s : "<NULL>";

    if (argc > 1) {
        k->ikinciSayiMi = rs_arg_str(aktifVm, 1) == nullptr;
        k->ikinciSayi = args[1];
    }
    k->ucuncuNullMu = rs_arg_str(aktifVm, 7) == nullptr;  // sınır dışı -> NULL

    if (std::strcmp(fn, "yankila") == 0) {
        std::string ters(k->gelen.rbegin(), k->gelen.rend());
        rs_return_str(aktifVm, ters.c_str());
        return 0.0;  // yok sayılmalı
    }
    return 42.0;
}

// 10 — betik -> host dize, host -> betik dize, sayı yolu bozulmadan
void t10() {
    rs_vm* vm = rs_new();
    aktifVm = vm;
    DizeKayit k;
    rs_set_host(vm, dizeHost, &k);
    rs_register(vm, "ui", "yankila");
    const int e = rs_eval(vm,
                          "include ui\n"
                          "fn dene():\n"
                          "    d = ui.yankila(\"RaidenScript\", 7)\n"
                          "    return len(d)\n",
                          "t10.rai");
    double out = 0.0;
    const int c = rs_call(vm, "dene", nullptr, 0, &out);
    bildir(e == 0 && c == 0 && k.gelen == "RaidenScript" && yakin(out, 12.0),
           "dize gidiyor, dize donuyor (uzunluk 12)", rs_last_error(vm));
    bildir(k.ikinciSayiMi && yakin(k.ikinciSayi, 7.0),
           "sayi argumani args[] yolundan geliyor, rs_arg_str NULL");
    bildir(k.ucuncuNullMu, "sinir disi indis NULL donuyor");
    rs_free(vm);
    aktifVm = nullptr;
}

// 11 — rs_return_str çağrılmazsa eski sayı davranışı aynen sürüyor
void t11() {
    rs_vm* vm = rs_new();
    aktifVm = vm;
    DizeKayit k;
    rs_set_host(vm, dizeHost, &k);
    rs_register(vm, "ui", "sayiVer");
    rs_eval(vm,
            "include ui\n"
            "fn dene():\n"
            "    return ui.sayiVer(\"x\") + 1\n",
            "t11.rai");
    double out = 0.0;
    const int c = rs_call(vm, "dene", nullptr, 0, &out);
    bildir(c == 0 && yakin(out, 43.0), "dize donmeyen host cagrisi hala sayi",
           rs_last_error(vm));
    rs_free(vm);
    aktifVm = nullptr;
}

// 12 — geri çağrı dışında rs_arg_str NULL, rs_return_str sessiz
void t12() {
    rs_vm* vm = rs_new();
    const bool a = rs_arg_str(vm, 0) == nullptr;
    rs_return_str(vm, "yoksayilmali");
    const bool b = rs_arg_str(nullptr, 0) == nullptr;
    bildir(a && b, "geri cagri disinda dize kanali kapali");
    rs_free(vm);
}

// 13 — sonsuz özyineleme: yığın taşması yerine yakalanabilir hata
//
// Bu testin asıl işi ÇÖKMEMEK. Sınır yoksa süreç burada sessizce ölüyordu
// (JVM'de tüm Minecraft sunucusu, tek satır günlük bırakmadan).
void t13() {
    rs_vm* vm = rs_new();
    rs_set_max_depth(vm, 100);
    const int e = rs_eval(vm,
                          "fn dip(n):\n"
                          "    return dip(n + 1)\n",
                          "t13.rai");
    const double arg = 0.0;
    double out = 0.0;
    const int c = rs_call(vm, "dip", &arg, 1, &out);
    const std::string hata = rs_last_error(vm);
    bildir(e == 0 && c != 0 && hata.find("derin") != std::string::npos,
           "sonsuz ozyineleme cokme degil hata veriyor", hata);
    rs_free(vm);
}

// 14 — sınır aşımı betikte yakalanabilir ve VM sonrasında kullanılabilir kalıyor
void t14() {
    rs_vm* vm = rs_new();
    rs_set_max_depth(vm, 50);
    const int e = rs_eval(vm,
                          "fn dip(n):\n"
                          "    return dip(n + 1)\n"
                          "fn dene():\n"
                          "    try:\n"
                          "        dip(0)\n"
                          "    catch h:\n"
                          "        return 7\n"
                          "    return 0\n",
                          "t14.rai");
    double out = 0.0;
    const int c = rs_call(vm, "dene", nullptr, 0, &out);
    bildir(e == 0 && c == 0 && yakin(out, 7.0), "derinlik hatasi betikte yakalanabiliyor",
           rs_last_error(vm));

    // Sayaç geri alındı mı? Aynı VM'de sığ bir çağrı hâlâ çalışmalı.
    double out2 = 0.0;
    const int c2 = rs_call(vm, "dene", nullptr, 0, &out2);
    bildir(c2 == 0 && yakin(out2, 7.0), "derinlik sayaci cagri sonrasi sifirlaniyor",
           rs_last_error(vm));
    rs_free(vm);
}

// 15 — yerleşik metoda yanlış tipte argüman: çökme değil hata
void t15() {
    rs_vm* vm = rs_new();
    const int e = rs_eval(vm,
                          "fn dene():\n"
                          "    return \"a,b\".split(5)\n",
                          "t15.rai");
    double out = 0.0;
    const int c = rs_call(vm, "dene", nullptr, 0, &out);
    const std::string hata = rs_last_error(vm);
    bildir(e == 0 && c != 0 && hata.find("split") != std::string::npos,
           "split(sayi) cokmuyor, tip hatasi veriyor", hata);
    rs_free(vm);
}

// 16 — tam sayı sınırları: taşma ve tanımsız kaydırma sessizce geçmiyor
void t16() {
    rs_vm* vm = rs_new();
    const int e = rs_eval(vm,
                          "fn tasma():\n"
                          "    return 2 ** 64\n"
                          "fn kaydir():\n"
                          "    return 1 << 64\n"
                          "fn birUssu():\n"
                          "    return 1 ** 10000000000\n",
                          "t16.rai");
    const std::string evalHata = rs_last_error(vm);
    double out = 0.0;
    const int a = rs_call(vm, "tasma", nullptr, 0, &out);
    const int b = rs_call(vm, "kaydir", nullptr, 0, &out);
    double u = 0.0;
    const int d = rs_call(vm, "birUssu", nullptr, 0, &u);
    bildir(e == 0 && a != 0 && b != 0 && d == 0 && yakin(u, 1.0),
           "tasma ve gecersiz kaydirma hata, 1**buyuk aninda 1", evalHata);
    rs_free(vm);
}

// 17 — host'a giden hata metni: tip adı değil betiğin yazdığı mesaj
void t17() {
    rs_vm* vm = rs_new();
    const int e = rs_eval(vm,
                          "fn dene():\n"
                          "    throw Error(\"tutar girilmedi\")\n",
                          "t17.rai");
    double out = 0.0;
    const int c = rs_call(vm, "dene", nullptr, 0, &out);
    const std::string hata = rs_last_error(vm);
    bildir(e == 0 && c != 0 && hata.find("tutar girilmedi") != std::string::npos,
           "istisna mesaji host'a ulasiyor", hata);
    rs_free(vm);
}

}  // namespace

int main() {
    std::cout << "RaidenScript C API kosumu\n";
    t1();
    t2();
    t3();
    t4();
    t5();
    t6();
    t7();
    t8();
    t9();
    t10();
    t11();
    t12();
    t13();
    t14();
    t15();
    t16();
    t17();
    std::cout << "\ngecen: " << gecen << "  kalan: " << kalan << '\n';
    return kalan == 0 ? 0 : 1;
}
