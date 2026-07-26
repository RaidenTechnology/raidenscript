# RaidenScript — Çalışma Günlüğü

> Devlog'ların ham maddesi. Her dönüm noktasında güncellenir.

---

## 📌 BURADAN DEVAM ET (26 Temmuz 2026'da duraklatıldı)

**Durum:** Faz 1'in 4/7 adımı bitti. Her şey commit'li (`1d2d861`), çalışan ağaç temiz.

```
[x] 1  iskelet + kaynak/konum + tanılama motoru
[x] 2  lexer                    15/15 örnek, 6.889 token
[x] 3  AST                      20 ifade + 16 deyim düğümü
[x] 4  parser                   15/15 örnek ayrışıyor
[x] 5  resolver                 15/15 temiz, 1.044 isim çözüldü
[x] 6  yorumlayıcı              ✅ rs run ÇALIŞIYOR — 10/10 örnek
[ ] 7  REPL              ← SIRADAKİ İŞ (küçük)
```

🎉 **`rs run examples/01-temeller.rai` çalışıyor.** Faz 1'in hedefi tutturuldu.

**Derleme:** `powershell -File build.ps1` (w64devkit'i kendi bulur)
**Sınama:** `build\rs.exe ast examples\12-algoritmalar.rai`

### ✅ Adım 5 BİTTİ (26 Tem) — `src/resolver.hpp/.cpp`

15/15 örnek temiz geçiyor, **1.044 isim çözüldü**. Çözümler AST'ye yazılmıyor,
`unordered_map<const Expr*, Resolution>` yan tablosunda duruyor — ağaç değişmedi.

**Doğrulanan davranışlar:**
- Kapanış yakalama: `02-fonksiyonlar` 3 upvalue, `12-algoritmalar` 7 upvalue
  (notlamalı fibonacci'nin `onbellek` + `fib` yakalaması), `09` 1 upvalue
- Host ayrımı kendiliğinden çalıştı: `06` 13 host (`game.*`), `10` 12 host (`mc.*`),
  `13` 5 host (`serial.*`), `07` 7 host (`sys.*`) — gömme tezi veriye yansıdı
- Hata yolları: `outer` dış kapsamda yoksa, `break`/`continue` döngü dışında,
  `return` fonksiyon dışında, `self`/`super` metot dışında, `super` tabansız sınıfta
- Uyarılar: kullanılmayan yerel değişken, hiçbir yerde tanımlanmamış ad

**Eklenen kural (SPEC'e yansıtılacak): fn/class/trait adları HOIST edilir.**
Her blokta gövdeler çözülmeden önce bu adlar tanımlanır, böylece sıralamadan
bağımsız birbirlerini çağırabilirler. `once()` kendinden sonra tanımlanan
`sonra()`'yı çağırabiliyor. Değişkenlerde hoisting YOK — SPEC §2'deki
"ilk atama tanımlamadır" kuralı aynen duruyor.

**🐛 Yazarken çıkan hata:** atama da değişkeni "kullanıldı" sayıyordu, bu yüzden
hiç okunmayan ama sürekli yazılan değişkenler ölü kod uyarısı almıyordu.
`lookup()` artık `used` bayrağını yalnızca OKUMA'da set ediyor.

---

### Adım 5 — Resolver: ne yapacak (plan, tamamlandı)

Yeni dosya `src/resolver.hpp/.cpp`. `ExprVisitor` + `StmtVisitor` uygulayacak
(AstDumper'ın yaptığı gibi — ağaç hiç değişmeyecek).

1. **Kapsam zinciri.** Blok/fonksiyon/sınıf başına bir kapsam. Bir kapsamdaki
   İLK atama tanımlamadır (SPEC §2); sonrakiler yeniden atamadır.
2. **`outer` semantiği.** `outer x = ...` dış kapsamdaki `x`'i hedefler; dışarıda
   yoksa hata.
3. **Kapanış upvalue'ları.** Lambda'nın kapattığı değişkenleri işaretle —
   yorumlayıcı bunları heap'te yaşatacak.
4. **Tanımsız isim hatası.** Kullanılan ama hiç tanımlanmamış her ad burada
   yakalanır; tanılama motoru zaten hazır (satır/sütun + ok işareti + ipucu).
5. **Bildirilmemiş sınıf alanı UYARISI** (SPEC §4, bulgu #3): `init` içinde
   `self.x = ...` ile yaratılan ve sınıf gövdesinde bildirilmeyen alanlar
   `any` tipini alır ve uyarı üretir.
6. **`self` / `super` bağlamı.** Metot dışında kullanılırsa hata.

### ✅ Adım 6 BİTTİ (26 Tem) — `src/value.*` + `src/interp.*`

**`rs run` çalışıyor. 10/10 çalışabilir örnek geçiyor, 0 gerçek hata.**
Kalan 5 örnek (`06/07/08/10/13`) host bağlayıcısı bekliyor — tasarım gereği,
`game.*`/`mc.*`/`sys.*`/`serial.*` Faz 4'te gelecek.

**Temsil kararı:** değerlerde `std::variant` DOĞRU araç (12 alternatif, özyineleme
shared_ptr ile kırılıyor, `std::visit` tam olarak ihtiyaç). AST'de 36 alternatifle
reddedilmişti — aynı araç, farklı ölçekte farklı sonuç.

**Akış kontrolü C++ istisnasıyla:** `ReturnSignal`/`BreakSignal`/`ContinueSignal`/
`ScriptThrow`. Tree-walk'ta en temiz yol; her `visit()`'in sinyal döndürmesi gerekmiyor.

**Prelude'un bir kısmı RaidenScript'te yazıldı.** `Error` sınıfı `PRELUDE_SRC`
içinde dilin kendisiyle tanımlı — bu sayede `class YetersizKredi(Error)` ve
`super.init(msg)` hiçbir özel durum kodu olmadan çalışıyor.

**Doğrulanan spec davranışları:**
- ✅ Doğruluk: `if sayac:` (sayac=0) bloğa GİRDİ, `if bos:` (nil) girmedi
- ✅ `/` her zaman float (`10 / 4 = 2.5`), `//` tam bölme (negatiflerde aşağı yuvarlama)
- ✅ `**` sağ birleşmeli — `2 ** 3 ** 2 = 512` (64 değil)
- ✅ Kapanış + notlama: `fib(50) = 12586269025` anında
- ✅ Kalıtım, `super.init`, metot geçersiz kılma, `is` ile alt sınıf kontrolü
- ✅ try/catch/finally, özel hata sınıfları
- ✅ UTF-8: `'Yıldırım'.len() == 8`, Türkçe i/İ ve ı/I dönüşümü doğru

**🔴 Çalıştırırken çıkan tasarım kararı: haritalarda nokta erişimi.**
`05` çalışmadı çünkü `oyuncu = {"kredi": 300}` bir harita ama `oyuncu.kredi`
yazmıştım. Önce "örnek hatalı" sandım — ama daha derin bir sorun: **host'tan gelen
olay verileri harita olarak gelir.** `06`'daki `olay.hedef.hp`, `10`'daki
`olay.oyuncu` hep bu kalıba dayanıyor. Nokta erişimi haritalarda çalışmazsa
gömme API'si `olay["hedef"]["hp"]` gibi okunmaz hâle gelir.
**Karar: haritalarda nokta erişimi desteklenir** (JS/Lua gibi), okuma ve yazma
ikisi de. **Yerleşik metotlar öncelikli** — `harita.len` metottur, anahtar değil;
çakışan anahtar için `harita["len"]` yazılır.

**Ek:** `std.math` uygulandı (sqrt/sin/cos/tan/log/exp/floor/ceil/abs/atan2/pow/
min/max + PI/E). Tam modül sistemi Faz 4'te ama bu 30 satır `15-mini-hesaplayici`'yi
tam çalışır hâle getirdi.

**Bilinen sınırlama:** döngüsel referanslar (`a.b = a`) sızdırıyor — `shared_ptr`
kullanıldığı için. Çöp toplayıcı Faz 3'ün işi, bilinen ve kabul edilen.

---

### Adım 6 — Yorumlayıcı: ne yapacak (plan, tamamlandı)

Yeni dosya `src/value.hpp` (değer temsili) + `src/interp.hpp/.cpp`.

- Değer: `int64`, `double`, `bool`, `nil`, `string`, `list`, `map`, fonksiyon,
  sınıf, örnek. Faz 1'de `shared_ptr` yeterli — GC Faz 3'te gelecek.
- **Doğruluk kuralı: SADECE `nil` ve `false` yanlıştır** (SPEC §2). `0` ve `""` doğru.
- **`/` her zaman float, `//` tam bölme** (SPEC §2).
- `and`/`or` kısa devre (bu yüzden AST'de `Logical` ayrı düğüm).
- `return`/`break`/`continue` C++ istisnasıyla taşınır (tree-walk'ta en temiz yol).
- `throw`/`try`/`catch`/`finally` → RaidenScript istisnaları.
- Prelude (SPEC §7.2): `print len type int float str bool range assert Error`.

**Bitiş çizgisi:** `rs run examples/01-temeller.rai` çalışsın.

### Aklında tut

- 15 örnek artık **regresyon takımı**. Her değişiklikten sonra hepsini koştur —
  lexer'daki `T?` hatası ve parser'daki brace-block hatası ikisi de bu sayede çıktı.
- Faz 0'da park edilen iki karar Faz 2'ye ait, şimdi uğraşma: tipli `catch` (#12),
  `sys.bekle` async kısıtı (#13).

---

---

## 26 Temmuz 2026 — Faz 0 başladı

**Dönüm noktası:** Projenin kuruluşu ve dil tanımının ilk taslağı.

### Yapılanlar

- **`SPEC.md` yazıldı** — dil tanımının v0 taslağı:
  - Hedefler ve *hedef olmayanlar* (kapsam patlamasına karşı ilk savunma)
  - 14 temel tasarım kararı, her biri gerekçesiyle
  - Sözdizimi turu: değişkenler, string'ler, koleksiyonlar, akış kontrolü, fonksiyonlar, sınıflar, trait'ler, hatalar, async, modüller, `view`, dekoratörler
  - Kademeli tip sistemi ve `T?` nil-güvenliği kuralı
  - Bellek modeli + host sınırında handle tablosu kararı
  - Gömme modeli: C API taslağı ve 5 host bağlayıcısı
  - 28 anahtar kelime (v1.0'a kadar tavan 30)
  - Operatör öncelik tablosu (Pratt parser bunu birebir uygular)
  - Tam EBNF grameri
  - Faz 1 MVP sınırı: neyin içeride, neyin hangi fazda olduğu

- **`examples/` ilk parti — 7 program** (hedef 30):
  - `01-temeller` — değişkenler, doğruluk kuralı, döngüler, aralıklar
  - `02-fonksiyonlar` — kapanışlar, özyineleme, `outer`, yüksek mertebeli fonksiyonlar
  - `03-siniflar` — kalıtım, trait'ler, geçersiz kılma
  - `04-koleksiyonlar` — liste/harita, dilimleme, iç içe yapılar
  - `05-hatalar` — try/catch/finally, özel hata sınıfı
  - `06-oyun-modu` — STAR BREAKER modu (gömme tezinin doğrulaması)
  - `07-otomasyon` — RaidenAI/Crimson betiği

### Bulgular

Örnekleri yazmak spec'te **7 kusur** ortaya çıkardı — Faz 0'ın tüm amacı buydu.
İkisi kırmızı (gramer dili yazamıyor), beşi sarı (karar bekliyor).
Ayrıntılar `SPEC.md` §11a'da.

En kritik ikisi:
1. Koşullu ifade (ternary) gramerde hiç yok, ama refleksle yazıldı.
2. Çok satırlı ok fonksiyonu (`() => { ... }`) gramerde tanımsız — ve gömme
   API'sinin en sık kalıbı tam olarak bu.

### Kararlar

- Dil çekirdeği küçük kalacak; "mod + plugin + uygulama + otomasyon + oyun"
  yeteneği dilden değil **host bağlayıcılarından** gelecek.
- Paket sistemi repo bazlı (Go modeli) — registry, sunucu, hesap, domain yok.
- Yol: tree-walking yorumlayıcı → bytecode VM. Native derleme sonraya.
- Uygulama dili C++ (WASM + native + gömülü tek kaynaktan çıkabilen tek seçenek).

### Sırada

- `examples/` 30'a tamamlanacak.
- §11a'daki 7 bulgu karara bağlanacak, gramer güncellenecek.
- Sonra Faz 1: lexer → Pratt parser → AST → resolver → tree-walking yorumlayıcı.

---

## 26 Temmuz 2026 — İsim kesinleşti: **RaidenScript**

Kullanıcı 8 aday önerdi, hepsi denetlendi. Kazanan **RaidenScript** — aynı adda dil
veya kayda değer proje olmadığı doğrulandı.

**Şema:** `RaidenScript` / kısaltma **RS** / komut `rs` / uzantı `.rai`

### Elenenler ve sebepleri (bir daha tartışılmasın)

| Aday | Neden elendi |
|---|---|
| RaidenScriptus | Uydurma Latince ek, yazım hatası gibi okunuyor |
| RaidenScripter | "Scripter" = betik yazan *kişi*; aracı değil kullanıcıyı adlandırıyor |
| RaidenScripturam | 17 karakter, *scriptura* = kutsal metin çağrışımı |
| RaidenScraptus | **"Scrap" = hurda/çöp.** Bir dil için felaket çağrışım |
| RaidenOS | İşletim sistemi adı — yanlış kategori |
| Raiden++ | `+` paket adı/URL'de geçersiz; arama motorları `++`'ı yutar → aranamaz |
| Raiden# | Yukarıdakiler + **`#` dilin kendi yorum karakteri**; URL'de fragment ayracı |

### Uzantı kararı

`.rai` seçildi. "**Rai**den"in ilk üç harfi *ve* 雷 (rai = gök gürültüsü) kökü —
çift anlam. Örnekler zaten bu uzantıdaydı, sıfır yeniden iş.

**Alınamayan uzantılar (doğrulandı, bir daha araştırılmasın):**
`.rs` Rust'ın · `.rds` R'ın serileştirme formatı + AWS RDS markası ·
`.rsc` MikroTik RouterOS betikleri · `.ra` RealAudio ·
`.raisc`/`.rasc`/`.rdsc`/`.rdnsc` telaffuz/uzunluk sorunları

### Uygulanan değişiklikler

- `SPEC.md` başlık + adlandırma tablosu, C API `rai_*` → `rs_*`,
  modül önbelleği `~/.raidenscript/pkg/`, örnek paket `rs-http`, `rs run dosya.rai`
- `README.md` yeniden yazıldı (adlandırma bölümü eklendi)
- Proje klasörü `rai/` → `raidenscript/`

---

## 26 Temmuz 2026 — 7 kusur çözüldü, örnekler 10/30

**Dönüm noktası:** Faz 0'ın ilk bulgu turu kapandı, gramer güncellendi.

### Çözülen 7 kusur ve spec'e işlenişi

| # | Karar | Nereye |
|---|---|---|
| 1 | `a if kosul else b` eklendi — öncelik 0, sağ birleşmeli | §2, §8, §9 |
| 2 | `=> { ... }` girinti kuralına resmi istisna | §2, §9 lexer kuralı 2 |
| 3 | Bildirilmemiş alan serbest ama **uyarı** üretir, tipi `any` | §4 |
| 4 | `/` her zaman float, `//` tam bölme | §2 |
| 5 | Prelude sabitlendi (dar); matematik `use std.math` ister | §7.2 |
| 6 | f-string `{...}` alt-lexer'la yeniden taranır | §9 lexer kuralı 3 |
| 7 | `Error` prelude'da sınıf, kalıtılabilir | §7.2 |

Ayrıca **§9'a sözcüksel katman kuralları** eklendi (3 istisna: parantez derinliği
girintiyi bastırır, brace_block'ta INDENT/DEDENT yok, f-string alt-lexer'ı).

### Yeni örnekler (10/30)

- `08-tipler` — kademeli tip, `T?` nil-güvenliği, daraltma, `?.`, `any` sınırı
- `09-kosullu-ve-lambda` — yeni eklenen iki gramer yapısının egzersizi
- `10-minecraft-plugin` — **ikinci host bağlayıcısı** (`mc.*`, JVM köprüsü)

`10` özellikle önemli: aynı dilin hem tarayıcı oyununa (`06`) hem Minecraft
sunucusuna girebildiğini gösteriyor. Gömme tezinin asıl kanıtı bu — dil çekirdeği
hiç değişmedi, sadece bağlayıcı farklı.

### Yeni bulgu (#8)

`09`'u yazarken `(olay) => olay.hedef.y = ...` yazdım — **atama bir ifade değil.**
Karar: öyle kalsın (`if (x = 5)` hatasının kapısını açmamak için). Süslü parantezli
gövde bu durumun da çözümü — kural 2'nin ikinci gerekçesi.

### Düzeltme

`02-fonksiyonlar` çıplak `sqrt()` kullanıyordu; prelude kararı gereği
`use std.math` + `math.sqrt()` oldu.

### Sırada

- Örnekler 30'a — özellikle async, string işleme, algoritma, İHA telemetrisi
- Sonra **Faz 1**: lexer → Pratt parser → AST → resolver → tree-walking yorumlayıcı

---

## 26 Temmuz 2026 — Faz 0 TAMAM (15/15 örnek, 13 kusur)

**Dönüm noktası:** Tasarım fazı kapandı. Kod yazmaya hazırız.

### Kapsam kararı

30 örnek hedefi **15'e indirildi**. Gerekçe: 10 örnek zaten 8 kusur bulmuştu ve
getiri azalıyordu; kalan bütçe *tekrar* yerine **test edilmemiş alana** harcandı.
15 hedefli örnek, 30 doldurma örnekten daha çok kusur buldu — nitekim son 5 örnek
tek başına 5 kusur çıkardı.

### Son 5 örnek

- `11-metin` — string API'si, dilimleme, UTF-8/Türkçe, biçimlendirme
- `12-algoritmalar` — kabarcık/hızlı sıralama, ikili arama, notlamalı fibonacci, asal eleği
- `13-iha-telemetri` — **üçüncü host bağlayıcısı** (`serial.*`), SUAS2026 yer istasyonu
- `14-hata-kurtarma` — hata hiyerarşisi, yeniden deneme, `finally` ile kaynak temizliği
- `15-mini-hesaplayici` — **RaidenScript'te yazılmış lexer + Pratt parser + değerlendirici**

`15` kasıtlı olarak meta seçildi: Faz 1'de C++'ta yazacağımız şeyin RaidenScript'teki
provası. En çok kusuru o buldu — çünkü ilk kez 200 satırlık *gerçek* bir program yazıldı.

### Yeni 5 kusur (9-13)

| # | Kusur | Karar |
|---|---|---|
| 9 | **Bileşik atama yoktu** — `self.i = self.i + 1` onlarca kez | ✅ `+= -= *= /= //= %= **=` eklendi |
| 10 | **Açık uçlu dilim gramerde yoktu** — `x[6..]` yazılamıyordu | ✅ İki uç da atlanabilir oldu |
| 11 | **🔴 `=>` sonrası `{` gerçek belirsizlik** — blok mu map mi? | ✅ JS çözümü: `{` her zaman blok, map için `({...})` |
| 12 | Tipli `catch` yok, `is` zinciri + elle rethrow gerekiyor | 🅿 Faz 2'ye park |
| 13 | Bloklayan `sys.bekle()` olay döngüsünü kilitler | ✅ Sadece `async fn` içinde `await` ile |

**11 numara en değerlisi:** `{` hem map literal'i hem `brace_block` başlatıyordu.
Faz 1'de parser yazılırken bulunsaydı yeniden yazım gerekirdi. Faz 0'ın var olma
sebebi tam olarak bu.

### Faz 0 bilançosu

- 3 belge (SPEC / README / WORKLOG), 15 örnek program
- **13 kusur bulundu ve 11'i çözüldü** (2'si bilinçli olarak sonraki faza park)
- Tek satır derleyici kodu yazılmadı — ve yazılmamalıydı

### Sırada: FAZ 1

```
1  Proje iskeleti   CMake + C++20, klasör yapısı, test koşucusu
2  Lexer            token tipleri, satır/sütun, INDENT/DEDENT,
                    f-string alt-lexer'ı, brace-block modu
3  AST              düğüm tipleri + ziyaretçi altyapısı
4  Parser           recursive descent (deyim) + Pratt (ifade)
5  Resolver         kapsam zinciri, isim çözümleme, kapanış upvalue'ları
6  Yorumlayıcı      tree-walking, değer temsili, ortamlar
7  CLI + REPL       `rs run dosya.rai` + etkileşimli kabuk
8  Hata mesajları   kaynak satırı + ok işareti
```

**İlk dönüm noktası:** `rs run examples/01-temeller.rai` çalışsın.

---

## 26 Temmuz 2026 — Faz 1 / adım 1: iskelet + tanılama motoru

**Dönüm noktası:** İlk C++ kodu. `rs.exe` derlenip çalışıyor.

### Araç zinciri kuruldu

Makinede hiçbir C++ derleyicisi yoktu (cmake/g++/clang/MSVC/WinSDK — hepsi yok).
**w64devkit v2.8.0** kuruldu: `C:\Users\imrai\tools\w64devkit`
(tek 7z-SFX, 57.3 MB, kurulum yok, yönetici yetkisi yok, hesap yok).

```
gcc/g++ 16.1.0   make 4.4.1   gdb 17.1   __cplusplus = 202002
```

⚠️ **Tuzak:** `w64devkit\bin` PATH'te olmazsa g++ kendi assembler'ını (`as`)
bulamaz ve "cannot execute 'as'" der. Sistem PATH'i kirletmemek için
`build.ps1` araç zincirini kendisi bulup PATH'i sadece o oturum için ayarlıyor.

### Yazılanlar

| Dosya | İş |
|---|---|
| `Makefile` | `make` / `make run FILE=…` / `make test` / `make clean`, `DEBUG=1` ile sanitizer |
| `build.ps1` | Windows sarmalayıcı; w64devkit'i kendisi bulur, sistem PATH'ine dokunmaz |
| `src/source.hpp/.cpp` | `Span` (bayt ofseti) + `Source` (satır indeksi, UTF-8 farkındalıklı satır/sütun) |
| `src/diag.hpp/.cpp` | Rust tarzı tanılama: `hata: …`, `--> dosya:satır:sütun`, kaynak satırı, ok işareti, ipucu |
| `src/main.cpp` | CLI: `run` / `tani` / `--version` / `--help` / REPL iskeleti |

**Neden önce tanılama motoru?** SPEC §1 hedef 4: "hata mesajları birinci sınıf
vatandaş". Sonradan eklenen hata altyapısı hep yamalı kalır — lexer'dan bile önce
yazıldı ki bundan sonraki her modül ilk günden düzgün hata versin.

### Doğrulama

- `-Wall -Wextra -Wpedantic -Wshadow -Wconversion` ile **sıfır uyarı**
- `rs --version`, `rs run <dosya>` çalışıyor
- **UTF-8 sütun testi geçti:** `öğüşçı = 1   # fn burada` satırında ok işareti
  sütun **16**'ya (doğru) düştü; bayt ofseti kullanılsaydı 22 olup 6 karakter
  kayacaktı. Türkçe kaynakta hata mesajları hizalı.

### Sırada

Faz 1 / adım 2: **lexer** — token tipleri, INDENT/DEDENT üretimi, f-string
alt-lexer'ı, `brace_block` modu, `=>` sonrası `{` ayrımı (SPEC §9 lexer kuralları).

---

## 26 Temmuz 2026 — Faz 1 / adım 2: LEXER çalışıyor

**Dönüm noktası:** 15 örneğin tamamı temiz taranıyor — 6.889 token, sıfır hata.

### Yazılanlar

| Dosya | İş |
|---|---|
| `src/token.hpp/.cpp` | `Tok` enum'u, `Token`, 32 girişlik anahtar kelime tablosu, okunur adlar |
| `src/lexer.hpp/.cpp` | Tarayıcının kendisi (~470 satır) |
| `src/main.cpp` | `rs tokens <dosya>` — girintiye göre hizalanmış token dökümü |

### SPEC §9'daki dört kural da uygulandı ve doğrulandı

| Kural | Sınama | Sonuç |
|---|---|---|
| 1. Parantez derinliği girintiyi bastırır | Çok satırlı map literal'i | ✅ içinde NEWLINE üretilmiyor |
| 2. `brace_block` içinde NEWLINE var, INDENT/DEDENT yok | `(olay) => { ... }` | ✅ deyimler ayrılıyor |
| 3. f-string gövdesi ham tutulur | `f"{olay.hedef.ad} vuruldu"` | ✅ parser'a bırakıldı |
| 4. `=>` sonrası `{` her zaman blok | Aynı dosyada map + blok | ✅ `lastSignificant_` ile ayrılıyor |

### Lexer yazarken netleşen kararlar (SPEC §9'a işlendi)

- **Tanımlayıcılar UTF-8** → `ateş`, `sayaç`, `yıldırım` geçerli isimler.
  Spec söylememişti; Türkçe yazan biri için en somut kolaylık olduğu için evet.
- **Bilimsel gösterim** (`1e10`, `2.5e-3`) eklendi — sayısal kodda kaçınılmaz.
- **`.` ancak ardından RAKAM gelirse ondalık noktadır.** Bu kural olmasa `1..10`
  aralığı `1.` + `.10` diye taranıp aralık sözdizimi çökerdi. Token dökümünde
  doğrulandı: `1` `..` `10`.
- **Girintide sekme yasak** — Python'un en bilinen yarası baştan kapatıldı.

### 🐛 Bütünleşme testinin yakaladığı hata

15 örneğin hepsini taramak **2 dosyada hata** verdi: `08-tipler` ve `13-iha-telemetri`,
ikisi de "beklenmeyen '?'". Sebep: lexer çıplak `?`'i hata sayıyordu, ama SPEC §4'teki
`T?` nullable tip eki tam olarak çıplak `?` kullanıyor (`-> Oyuncu?`).

**Düzeltme:** `Tok::Question` eklendi. Doğrusu lexer'ın karar vermemesi —
`?.` mı `T?` mi olduğunu bağlam belirler, o da parser'ın işi.

Ders: tek dosyalık sınamalar bunu bulamazdı. Örnek koleksiyonunu **regresyon
takımı** olarak koşmak Faz 0'ın beklenmedik ikinci getirisi oldu.

### Doğrulama

- `-Wall -Wextra -Wpedantic -Wshadow -Wconversion` → sıfır uyarı
- 15/15 örnek temiz, 6.889 token
- Hata yolu: kapanmamış metin, bilinmeyen kaçış, `!`, `?`, kapanmamış `(`
  → beşi de konum + ok işareti + ipucu ile raporlandı, **tarama durmadı**
  (kullanıcı hepsini tek seferde görüyor)
- UTF-8 sütunlar doğru: `sayaç = ` satırında `=` sütun 7 (bayt sayılsaydı 8)

### Sırada

Faz 1 / adım 3-4: **AST + parser**. Recursive descent (deyimler) + Pratt (ifadeler),
SPEC §8 öncelik tablosu birebir uygulanacak.

---

## 26 Temmuz 2026 — Faz 1 / adım 3-4: AST + PARSER çalışıyor

**Dönüm noktası:** 15 örneğin tamamı ayrıştırılıyor. Elimizde artık sözdizimi ağacı var.

### Yazılanlar

| Dosya | İş |
|---|---|
| `src/ast.hpp` | 20 ifade + 16 deyim düğümü, `TypeNode`, `Param`, `Program` |
| `src/parser.hpp/.cpp` | Özyinelemeli iniş (deyim) + öncelik tırmanışı (ifade) |
| `src/astdump.hpp/.cpp` | Ağacı yazdıran ziyaretçi |
| `src/main.cpp` | `rs ast <dosya> [--sessiz]` |

### C++ kalıp kararları

- **Temsil: sanal hiyerarşi + Ziyaretçi.** `std::variant` + `std::visit` düşünüldü
  ama 36 alternatifle özyinelemeli variant hem derlemeyi şişiriyor hem hata
  mesajlarını okunmaz kılıyor.
- **`accept()` gövdeleri için CRTP.** `ExprNode<D>` / `StmtNode<D>` sayesinde
  36 düğümde aynı tek satır elle yazılmadı.
- **Öncelik tırmanışı, tablo sürücülü Pratt değil.** Gramer sabit olduğu için
  tablo esneklik kazandırmıyor; 11 kademe = 11 fonksiyon, okunması çok daha kolay.
  `**` ve koşullu ifade sağ birleşmeli — `2**3**2` ağacı doğrulandı (`2**(3**2)`).

### 🔴 En önemli bulgu: SPEC §9 KURAL 2 YANLIŞTI

`brace_block` içinde INDENT/DEDENT bastırılınca şu **ayrıştırılamıyor**:

```
kaydet("vurulma", (olay) => {
    if olay.kritik:
        hasar = hasar * 2      ← bu girinti lexer'a hiç ulaşmıyordu
})
```

Bastırma kararı `(` ve `[` için doğru (içleri tek bir ifade), ama `brace_block`
**deyim** içeriyor ve deyimler blok yapısına muhtaç.

**Düzeltme:** brace_block kendi girinti bağlamını açar — NEWLINE de INDENT/DEDENT
de üretilir. Deyim sonu ayrıca `;` ve `}` önü olarak kabul edilir (tek satırlık
`=> { x = 1 }` için). Lexer + parser + SPEC üçü birden güncellendi.

### Diğer iki bulgu

- **Sınıf alanında tip artık isteğe bağlı** (#15). Gramer `IDENT ":" type` zorunlu
  kılıyordu ama `06`/`10`'da `ad = "PLASMA LANCE"` yazmıştım — kademeli tip
  felsefesiyle çelişiyordu.
- **UTF-8 BOM atlanıyor** (#16). Windows editörleri dosya başına `EF BB BF` koyuyor;
  lexer bunu çok baytlı karakter sanıp **ilk tanımlayıcıya yapıştırıyordu**
  (`kademe` yerine `﻿kademe`). Görünmez bir hataydı, AST dökümünde yakalandı.

### Doğrulama

- Sıfır derleyici uyarısı (`-Wall -Wextra -Wpedantic -Wshadow -Wconversion`)
- **15/15 örnek ayrıştırılıyor**
- Sağ birleşme doğrulandı: `2 ** 3 ** 2` → `2 ** (3 ** 2)`
- Zincirli koşullu ifade doğru iç içe geçiyor
- f-string alt-lexer'ı çalışıyor: `f"{o.ad} → {o.hp:.1f} kaldı"` → 4 parça,
  biçim eki `.1f` ayrı yakalandı
- `a.b?.c[1..]` → indeks(alan?.(alan.(ad))) + açık uçlu aralık

### Sırada

Faz 1 / adım 5-6: **resolver + yorumlayıcı**. Kapsam zinciri, isim çözümleme,
kapanış upvalue'ları; sonra ağaç yürüyen değerlendirici.
Ondan sonra `rs run examples/01-temeller.rai` gerçekten çalışacak.
