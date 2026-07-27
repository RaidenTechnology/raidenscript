# Faz 5 — `web` bağlayıcısı: RaidenScript ile animasyonlu arayüzler

**Soru:** RaidenScript, Google Stitch çıktısı gibi ağır animasyonlu üst düzey web siteleri
yazabilecek hale gelir mi?

**Cevap: Evet — ve dile tek bir anahtar kelime eklemeden.** Eksik olan şey dil değil,
**host bağlayıcısı**. Ama önce dört hata kapatılmalı; ikisi ölümcül.

Bu belge ölçümle yazıldı. Aşağıdaki sayıların hepsi `dist/raidenscript.js` (27 Tem yapısı)
üzerinde node ile koşan gerçek ölçümlerdir, tahmin değil.

---

## 1. Ölçüm — sınır animasyonu kaldırıyor mu?

| Ölçüm | Sonuç | 16.7 ms'lik karede |
|---|---|---|
| Saf betik döngüsü (sınır yok) | 0.81 M işlem/s | ~13.500 döngü adımı |
| Host çağrısı, sayı argümanlı | 0.49 M çağrı/s | ~8.100 DOM ilkeli |
| Host çağrısı, dize argüman + dize dönüş | 0.36 M çağrı/s | ~6.000 çağrı |
| JS → betik girişi (olay başına) | 0.28 M çağrı/s | ~4.700 olay |
| **Gerçekçi kare: 200 öğe × 2 stil yazımı** | **1.45 ms/kare** | **bütçenin %9'u** |
| Aynı iş düz JS'te | 0.096 ms/kare | RS ~15× daha yavaş |

**Okunuşu:** WASM↔JS sınırı darboğaz DEĞİL. 200 öğeli bir sahneyi her karede betikten
güncellemek bütçenin onda birini yiyor. Yani "yavaş olur mu" sorusunun cevabı hayır.

**Ama yine de kare başına sürülmemeli** — sebep hız değil, mimari (bkz. §3).

---

## 2. Bulunan hatalar

### 🔴 H1 — WASM yığını 64 KB: ~130 seviyede özyineleme belleği bozuyor

`fn ic(n): return 1 + ic(n-1)` ile ölçüldü:

| Derinlik | native `rs.exe` | wasm (mevcut yapı) |
|---|---|---|
| 100 | ok | ok |
| 150 | ok | 💥 `memory access out of bounds` |
| 1000 | ok | 💥 `null function or function signature mismatch` |
| 5000 | 💥 sessiz ölüm, rc=127, tek satır hata yok | 💥 |

Çökmeden sonra **tüm modül ölüyor**: aynı `Module` üzerinde yeni bir VM bile açılamıyor.
Sebep: emscripten'in varsayılan yığını **64 KB** ve Makefile'da `-sSTACK_SIZE` yok.
Ağaç yürüyen bir yorumlayıcıda her betik çağrısı birkaç C++ karesi demek — 130 seviye
gerçek bir tavan değil, kaza.

Arayüz işi özyinelemelidir (bileşen ağacı gezme, iç içe menü, JSON yürüme). Bu haliyle
web hedefi başlamadan biter.

**Düzeltme (uygulandı, doğrulandı):** Makefile'ın wasm reçetesine `-sSTACK_SIZE=8MB`.

| | önce | sonra |
|---|---|---|
| Güvenli derinlik | ~130 | **1000+** |
| Aşınca ne oluyor | bellek bozulması, modül ölüyor | temiz, yakalanabilir `Maximum call stack size exceeded` |

Maliyeti: `.wasm` boyutu 3 bayt (454.323 → 454.326). Bellek çalışma anında rezerve.

**Kalan iş (C++, kullanıcının):** native tarafta 5000 derinlikte **sessiz ölüm** var —
rc=127, hiçbir tanı çıkmıyor. "Hata mesajları birinci sınıf vatandaş" diyen bir dilde
bu kabul edilemez. Yorumlayıcıya çağrı derinliği sayacı (öneri: 4000) ve aşıldığında
normal bir `Error` gerekiyor. Python'un `RecursionError`'ı budur.

---

### 🔴 H2 — Host'un attığı JS istisnası yığını kalıcı olarak sızdırıyor

Host fonksiyonu `throw` ederse (web'de kaçınılmaz: eksik düğüm, geçersiz seçici,
canvas hatası) istisna WASM karelerinin arasından geçip JS'e kaçıyor. Geçerken
**yığın işaretçisi geri alınmıyor.**

8 MB yığınla ölçüldü — sızıntı düzeltilmiş değil, sadece geç fark ediliyor:

```
başlangıç özyineleme derinliği: 1000
+5.000 istisna sonrası:          937     <- kapasite geri gelmiyor
50.000 istisnada:                MODÜL ÖLDÜ (memory access out of bounds)
```

64 KB yığınla aynı çöküş **1.000 istisnadan önce** geliyordu. Yani bu, uzun yaşayan
bir sayfada saatler içinde ölüm demek — hem de hiçbir uyarı vermeden.

Üstüne, aynı kökten ikinci bir kusur: **betik bu hatayı yakalayamıyor.**

```python
fn korumali():
    try:
        ui.patla()        # host JS istisnası atıyor
        return 1
    catch e:
        return 2          # ← buraya HİÇ girilmiyor
```

Ölçüm: `try/catch` devreye girmiyor, istisna doğrudan `vm.call`'dan JS'e fırlıyor.
Uygulamanın tamamı `.rai` içindeyse (banka demosu tam olarak böyle) betik kendi
hatasını yönetemiyor demektir.

**Düzeltme — iki katman:**

**(a) Köprü (JS, `bindings/js/rs-host.js`):** JS istisnası WASM sınırını ASLA geçmemeli.

```js
let sonuc;
try {
  sonuc = f.apply(null, args);
} catch (err) {
  // İstisnanın wasm karelerinden geçmesine izin verilmez: yığın işaretçisi
  // geri alınmıyor ve kaybedilen alan bir daha dönmüyor (ölçüldü).
  if (hostFail) {                      // yeni C API'si, aşağıda
    const p = strYaz(M, String(err && err.message || err));
    M._rs_host_fail(self.ptr, p);
    M._free(p);
    return 0;                          // yorumlayıcı çağrı yerinde Error yükseltir
  }
  console.error('RaidenScript: host hatası yutuldu -> ' + modul + '.' + fn, err);
  return 0;
}
```

**(b) C API (C++, kullanıcının):** `capi.h`'ye tek fonksiyon, `rs_return_str` ile aynı desen:

```c
/* Host geri çağrısı başarısız oldu. Yorumlayıcı, çağrının yapıldığı yerde
 * yakalanabilir bir Error yükseltir — betiğin try/catch'i çalışır.
 * rs_return_str gibi YALNIZCA geri çağrının içinde geçerlidir. */
void rs_host_fail(rs_vm* vm, const char* mesaj);
```

`interp.cpp`'de host çağrısının döndüğü yer: `rs_return_str`'in bayrağına bakılan
noktanın hemen yanında bir `pendingHostError` bayrağı; doluysa değeri döndürmek yerine
`throw` edilen normal RaidenScript hatası üretilir. Yaklaşık 20 satır.

Bu ikisi birlikte hem sızıntıyı, hem yakalanamayan hatayı kapatıyor.

---

### 🟠 H3 — `s = s + x` karesel; `list.push` + `join` doğrusal

Arayüz metni üretiminin ana kalıbı bu ve şu an tuzak:

| Parça sayısı | `s = s + parca` | `l.push(...)` + `join("")` | fark |
|---|---|---|---|
| 1.500 | 3.9 ms | 5.4 ms | join daha yavaş |
| 3.000 | 13.6 ms | 6.1 ms | 2.2× |
| 6.000 | 25.4 ms | 7.8 ms | 3.3× |
| 12.000 | 119.6 ms | 16.4 ms | **7.3×** |

3.000 birleştirme tek başına bir kareyi (16.7 ms) yiyor. Karesel, çünkü her `+` yeni
bir dize ayırıp tamamını kopyalıyor.

**İki seçenek:**

1. **Belgele (bedava, hemen):** SPEC'e "dize biriktirirken `join` kullan" kuralı.
2. **Yorumlayıcıda düzelt (doğru olan, C++):** `s = s + x` biçiminde, hedef değişken
   solda tekrar ediyorsa ve dizenin başka sahibi yoksa (refcount == 1) yeni ayırma
   yapmadan yerinde `append`. CPython'un yıllardır yaptığı numara. Kazanç: 12.000'de
   ~7×, ve kimse `join` bilmek zorunda kalmaz.

Web hedefi için asıl kural yine de mimari: **HTML'i dize olarak kurma.** Düğümleri
host ilkeliyle üret (banka demosu bunu doğru yapıyor), dize birleştirme sadece
metin/biçimleme için kalsın.

---

### 🟡 H4 — Betik dize döndürünce `vm.call` sessizce 0 veriyor

```js
vm.eval('fn ad():\n    return "RAIDEN"\n');
vm.call('ad', []);        // -> 0        (hata yok, uyarı yok)
```

`rs_call`'ın çıkışı `double*`; dize kanalı yalnızca host→betik yönünde çalışıyor.
Betik→host yönünde dize dönüşü **sessiz sıfıra** düşüyor — deponun en sevmediği hata
sınıfı, `01d9b33` commit'inin tam olarak önlemeye çalıştığı şey.

**Düzeltme (C++, küçük):** ya `rs_call` dize dönüşünde hata kodu + `rs_last_error`
("bu fonksiyon dize döndürüyor, `rs_call_str` kullan") versin, ya da simetriyi kapatan
`int rs_call_str(rs_vm*, const char* fn, const double* args, int argc, const char** out)`
eklensin. En azından **sessiz olmasın**.

Web için kritik değil (itme modeli kullanılıyor) ama `web.metin(...)` gibi bir ilkelin
betikten biçimlenmiş metin çekmesi gerektiği anda çarpılır.

---

## 3. Faz 5 tasarımı — `web` bağlayıcısı

### Temel karar: bildirimsel animasyon, kare döngüsü değil

§1 kare başına sürmenin mümkün olduğunu gösteriyor (bütçenin %9'u). Yine de doğru
mimari bu değil:

- WAAPI/CSS animasyonları **compositor iş parçacığında** koşar. Betik ne yaparsa yapsın
  60 fps'i düşürmez. Kare döngüsü ana iş parçacığındadır; betik bir kare uzarsa
  animasyon takılır.
- Google Stitch çıktısı gibi siteler zaten kare kare çizilmiyor: iş **CSS geçişleri,
  keyframes, scroll-driven animasyon, blur/gradient katmanları**. Motor tarayıcı.
- Deponun kendi mimari kuralıyla da bu örtüşüyor: *"motor JavaScript kalır, sadece
  içerik katmanı RaidenScript'e taşınır."* Betiğin işi **ne olacağını bildirmek**;
  ne zaman piksel değişeceği tarayıcının işi.

Kare döngüsü kapıda kalsın ama tavsiye edilen yol olmasın: `web.kare(fnAdi)` ile
`requestAnimationFrame`'e bağlanabilir — fizik benzeri işler için, ölçülmüş bütçe
kare başına ~4.000 host çağrısı.

### İlkeller (14 tane, dile ekleme yok)

```python
include web

# --- yapı ---
web.dugum(ustId, id, etiket, sinif)      # düğüm oluştur/yerleştir
web.metin(id, deger)
web.oznitelik(id, ad, deger)
web.sinif(id, ad, acik)                  # sınıf ekle/çıkar  (acik: 1/0)
web.sil(id)

# --- görünüm ---
web.stil(id, ozellik, deger)             # "--vurgu" gibi CSS değişkeni de olur
web.olcu(id, "genislik") -> sayı         # okuma (layout tetikler, seyrek kullan)

# --- animasyon (BİLDİRİMSEL: kare başına sınır geçişi YOK) ---
web.zaman(ad)                            # yeni zaman çizelgesi
web.anahtarKare(ad, yuzde, ozellik, deger)
web.oynat(id, ad, sure, gecis, gecikme, tekrar)   # -> element.animate(...)
web.gecis(id, ozellikler, sure, gecis)   # -> CSS transition

# --- olaylar ---
web.dinle(id, olay, fnAdi)               # addEventListener -> vm.call(fnAdi)
web.gorununce(id, fnAdi, esik)           # IntersectionObserver — scroll reveal
web.kaydirmaBagla(id, ad, baslangic, bitis)  # scroll-driven animation
web.girdi(id) -> str                     # form değeri ÇEKME (banka deseni)
```

Host tarafı (`bindings/js/web.js`, ~350-400 satır JS) bunları şuna çeviriyor:

| İlkel | Tarayıcı karşılığı | Nerede koşuyor |
|---|---|---|
| `web.oynat` | `element.animate(keyframes, opts)` | compositor |
| `web.gecis` | `style.transition` | compositor |
| `web.kaydirmaBagla` | `ScrollTimeline` / `animation-timeline: view()` | compositor |
| `web.gorununce` | `IntersectionObserver` | tarayıcı, olay başına 1 çağrı |
| `web.stil(id, "--x", v)` | CSS custom property | betik 1 değer yazar, CSS gerisini yapar |

**Stitch estetiğinin nereden geldiği** (dilin işi değil, ama bağlayıcının bunları
mümkün kılması gerekiyor): katmanlı gradyanlar, `backdrop-filter: blur`, yay (spring)
easing'leri, kademeli (staggered) giriş animasyonları, scroll-driven paralaks.
Hepsi yukarıdaki beş satıra oturuyor — hiçbiri kare döngüsü istemiyor.

### Kademeli giriş — bağlayıcının doğru kullanımı

```python
include web

fn kartlariGoster(adet):
    web.zaman("yukselt")
    web.anahtarKare("yukselt", 0,   "opacity",   "0")
    web.anahtarKare("yukselt", 0,   "transform", "translateY(24px)")
    web.anahtarKare("yukselt", 100, "opacity",   "1")
    web.anahtarKare("yukselt", 100, "transform", "translateY(0)")

    i = 0
    while i < adet:
        web.oynat(f"kart{i}", "yukselt", 520, "cubic-bezier(.2,.8,.2,1)", i * 60, 1)
        i = i + 1
```

`adet = 200` için sınırı **bir kez** 200 çağrıyla geçer (~0.4 ms), sonra animasyon
tamamen tarayıcıda koşar. Kare döngüsü kurulumu aynı işi saniyede 60 kez, 24.000
çağrıyla yapardı.

### `view` blokları — sonra, şeker olarak

SPEC 3.10'daki `view` sözdizimi bu ilkellerin üzerine oturur: `view` bir sanal ağaç
üretir, renderer onu `web.dugum` / `web.stil` çağrılarına çevirir. Ama bu **ayrıştırıcı
işi** (SPEC #8: `a < b` mi etiket açılışı mı belirsizliği, lexer'a mod desteği).

Sıra önemli: **önce bağlayıcı, sonra `view`.** Bağlayıcı olmadan `view`'ın çevireceği
bir hedef yok; bağlayıcı varsa `view` olmadan da site yazılabilir (banka demosu kanıt).

---

## 4. İş listesi

| # | İş | Kim | Not |
|---|---|---|---|
| 1 | `-sSTACK_SIZE=8MB` | ✅ yapıldı | H1, ölçüldü: derinlik 130 → 1000+ |
| 2 | `rs_host_fail` + köprüde try/catch | C++ + JS | H2 — web'e geçmeden önce ŞART |
| 3 | Özyineleme derinliği sayacı + `Error` | C++ | H1'in native ayağı, sessiz ölümü bitirir |
| 4 | `rs_call` dize dönüşünde sessiz 0 | C++ | H4 — en azından hata versin |
| 5 | Dize `+=` yerinde ekleme (refcount==1) | C++ | H3 — 12k'da 7×, isteğe bağlı |
| 6 | `bindings/js/web.js` + 17 ilkel | ✅ yapıldı | `kaydir` ve `sayfaBagla` yolda eklendi |
| 7 | `demo/site` — RAIDEN PARÇA mağazası | ✅ yapıldı | 424 düğüm, 0 host hatası, 83 ms kurulum |
| 8 | Kaçan `Error`'un mesajı kayboluyor (H5) | C++ | aşağıda |
| 9 | `view` blokları | C++ (lexer/parser) | Faz 5b, şeker |

---

## 5. Demo sırasında çıkan iki ek bulgu

### 🟡 H5 — Betikten kaçan `Error`'un mesajı yok oluyor

`sepeteEkle` hatası host'a şöyle ulaştı:

```
hata: betik istisna fırlattı: <Error>
   --> magaza.rai:589:12
```

Konum, kaynak satırı ve ok işareti kusursuz — ama **hatanın kendi mesajı yok**,
yerine tip adı basılıyor. `throw Error("tutar girilmedi")` yazan bir betikte o
metin host'a hiç ulaşmıyor. `interp.cpp`'de istisna dışarı verilirken `message`
alanı tanıya eklenmeli; tanı altyapısı zaten hazır olduğu için küçük bir iş.

### 🔴 H6 — Özyineleme JVM'i SESSİZCE öldürüyor (Faz 6 engelleyicisi)

Aynı yığın sorununun üçüncü yüzü, ve en kötüsü. JNI köprüsünde ölçüldü:

| Ortam | Güvenli derinlik | Aşınca |
|---|---|---|
| native `rs.exe` (8 MB) | ~1.000 | sessiz ölüm, rc=127 |
| wasm (`-sSTACK_SIZE=8MB`) | ~1.000 | temiz `Maximum call stack size exceeded` |
| **JVM, varsayılan (1 MB)** | **~500** | **süreç ölüyor: Java istisnası YOK, `hs_err` YOK, rc=127** |
| JVM, `-Xss16m` | ~5.000 | aynı sessiz ölüm |

Tarayıcıda bir sekme ölür. Minecraft sunucusunda **tüm sunucu** ölür ve
günlükte tek satır iz kalmaz — 20 oyuncu düşer, kimse sebebini bilmez.

Betik karesi başına yaklaşık **1,7 KB yerel yığın** harcanıyor (1 MB ÷ ~600).

**Bu, iş listesindeki 3. maddeyi (özyineleme derinliği sayacı) "iyi olurdu"dan
"Faz 6 için ŞART"a taşıyor.** Yorumlayıcıya bir derinlik sayacı (öneri: 4.000)
ve aşıldığında normal, yakalanabilir bir `Error` gerekiyor. Geçici önlem sunucuyu
`-Xss16m` ile başlatmak — sınırı öteler, kaldırmaz.

### 📐 Sınırda daraltma kuralı (belgelenecek, hata değil)

`rs_host_fn` sayıları `double` taşıyor (capi.h). Olay geri çağrılarında gelen
`0` aslında `0.0` ve **liste indisi olarak kullanılamıyor**:

```python
fn sepeteEkle(ham):
    u = int(ham)          # ← olmazsa URUNLER[u] hata veriyor
```

Bu tasarım gereği doğru (sınırın tek sayı tipi olması bilinçli karar) ama
SPEC'te "host'tan gelen sayılar kayan noktalıdır, indis olarak kullanmadan önce
`int()` ile daralt" diye yazmıyor. Web bağlayıcısıyla birlikte bu, herkesin
çarpacağı ilk duvar oldu — yazılmalı.

Kapsam disiplini: bu listede dile eklenen **hiçbir anahtar kelime yok**. 29/30 sabit
kalıyor, güç bağlayıcıdan geliyor — SPEC'in söz verdiği gibi.
