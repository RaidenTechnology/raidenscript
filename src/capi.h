/* RaidenScript gömme API'si — saf C.
 *
 * Bu başlık host tarafından okunur (C, JS/WASM glue, JVM köprüsü). İçinde
 * hiçbir C++ tipi geçmez; öyle olsun ki emscripten ve diğer köprüler bunu
 * doğrudan kullanabilsin.
 *
 * Kapsam (Faz 4 / adım 1): bir VM = bir betik. rs_eval bir kez çağrılır.
 * Sınırda skaler (double) taşınır; metin ve yapı gerekirse sonra eklenir.
 *
 * Tipik akış:
 *     rs_vm* vm = rs_new();
 *     rs_set_host(vm, geriCagri, kullanici);
 *     rs_register(vm, "game", "spawnBullet");
 *     rs_eval(vm, kaynak, "silah.rai");
 *     double sonuc;
 *     rs_call(vm, "atesEt", args, 2, &sonuc);
 *     rs_free(vm);
 *
 * SIRA ÖNEMLİ: rs_register çağrıları rs_eval'den ÖNCE yapılmalıdır, çünkü
 * 'include game' satırı eval sırasında çözülür.
 */
#ifndef RAIDENSCRIPT_CAPI_H
#define RAIDENSCRIPT_CAPI_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rs_vm rs_vm;

/* Betikten host'a geri çağrı. 'include game' + 'game.spawnBullet(x, y)' buraya düşer.
 * Dönen değer betiğe sonuç olarak verilir. */
typedef double (*rs_host_fn)(const char* modul, const char* fn,
                             const double* args, int argc, void* user);

/* Yaşam döngüsü. rs_new bellek yetmezse NULL döner. */
rs_vm* rs_new(void);
void   rs_free(rs_vm* vm);

/* Host bağlayıcısı — eval'den önce. */
void rs_set_host(rs_vm* vm, rs_host_fn cb, void* user);
int  rs_register(rs_vm* vm, const char* modul, const char* fn);

/* Betiği yükler ve üst düzey deyimlerini çalıştırır (fonksiyon tanımları kurulur).
 * 0 = tamam, !0 = hata -> rs_last_error(). Bir VM'de yalnızca bir kez. */
int rs_eval(rs_vm* vm, const char* kaynak, const char* ad);

/* Global bir fonksiyonu çağırır. args NULL olabilir (argc 0 ise).
 * out NULL olabilir — sonuç istenmiyorsa. 0 = tamam. */
int rs_call(rs_vm* vm, const char* fn, const double* args, int argc, double* out);

/* Son hatanın tam tanı metni (konum, kaynak satırı, ok işareti, ipucu dahil).
 * Sahibi VM'dir; bir sonraki rs_* çağrısına kadar geçerlidir. Hata yoksa "". */
const char* rs_last_error(rs_vm* vm);

/* --- Dize kanalı (Faz 4 / adım 5) ---------------------------------------
 *
 * rs_host_fn imzası DEĞİŞMEDİ: args[] hâlâ yalnızca double taşır ve mevcut
 * host'lar (STAR BREAKER dahil) olduğu gibi çalışır. Dizeler sayının içine
 * sıkıştırılmaz — yanlarında ayrı bir kanalda gider ve bu iki fonksiyonla
 * okunur/yazılır. Her ikisi de YALNIZCA geri çağrının içinde geçerlidir.
 *
 * Metin host'tan betiğe nasıl gider? Betik ÇEKER, host itmez:
 *
 *     betik:  iban = ui.girdi("iban")
 *     host :  const char* alan = rs_arg_str(vm, 0);   // "iban"
 *             rs_return_str(vm, formdakiDeger);       // betik dize alır
 *
 * Böylece rs_call'ın imzasına dokunmadan iki yön de açılıyor.
 */

/* i. argüman betikte bir DİZE ise metni, değilse NULL döner. Sayı argümanlar
 * args[] dizisinden okunmaya devam eder. İşaretçi geri çağrı bitene kadar
 * geçerlidir; saklanacaksa kopyalanmalı. */
const char* rs_arg_str(rs_vm* vm, int i);

/* Betiğe sayı yerine DİZE döndürür. Bu çağrıldıysa geri çağrının kendi dönüş
 * değeri yok sayılır. Metin kopyalanır, sahiplik host'ta kalır. */
void rs_return_str(rs_vm* vm, const char* s);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* RAIDENSCRIPT_CAPI_H */
