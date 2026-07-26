# Rai — Dil Tanımı (v0, taslak)

> **Durum:** Faz 0 — tasarım. Henüz tek satır derleyici kodu yok ve olmamalı.
> **Kod adı:** `rai` (geçici — isim kesinleşince tek seferde değişir)
> **Son güncelleme:** 26 Temmuz 2026

---

## 1. Bu dil ne?

Rai, **gömülebilir bir betik dili**. Tek başına çalışan programlar yazılabilir, ama asıl varlık sebebi başka bir uygulamanın içine girip ona programlanabilirlik kazandırmak: oyuna mod, sunucuya plugin, masaüstü uygulamasına otomasyon.

Dört dilden beslenir ve her birinden farklı bir katman alır:

| Kaynak | Ne alındı |
|---|---|
| **Python** | Okunabilirlik: girinti bazlı bloklar, f-string, az noktalama |
| **JavaScript** | Çalışma modeli: kapanışlar, `async`/`await`, olay döngüsü, ok fonksiyonları |
| **C++** | İsteğe bağlı statik tipler, performans bilinci, native'e gömülebilirlik |
| **HTML** | `view` blokları — arayüzü ayrı şablon dosyasında değil, dilin içinde bildirimsel olarak tanımlama |

### Hedefler

1. **Öğrenmesi 1 saat, ustalaşması aylar.** Python bilen biri ilk 10 dakikada üretken olmalı.
2. **Çekirdek küçük kalır.** Anahtar kelime sayısı 30'u geçmez. Güç kütüphanede ve host bağlayıcılarında.
3. **Her yere gömülebilir.** Tarayıcı (WASM), masaüstü (native), Node (N-API), JVM (köprü), gömülü (ESP32).
4. **Hata mesajları birinci sınıf vatandaş.** Bir dilin kalitesi büyük ölçüde hata mesajlarının kalitesidir.
5. **Kademeli tip.** Tip yazmadan başla, kritik yerlerde tip ekle, tip denetleyici seni korusun.

### Hedef OLMAYANLAR

Bunları yazmak, sonradan "şunu da ekleyelim" baskısına karşı tek savunma:

- ❌ **C++ yerine geçmek.** Sistem dili değil. İşletim sistemi yazılmaz.
- ❌ **Maksimum performans.** Amaç "yeterince hızlı", C değil.
- ❌ **Akademik tip sistemi.** Bağımlı tipler, HKT, tam çıkarım yok. Pratik olan kadarı.
- ❌ **Kendi paket registry'si.** Modüller repo adresinden çözülür (Go modeli). Sunucu işletmiyoruz.
- ❌ **Geriye dönük uyumluluk sözü — v1.0'a kadar.** v1.0 öncesi her şey değişebilir.

---

## 2. Temel kararlar ve gerekçeleri

Bu tablo dilin karakterini belirler. Her satırın bir gerekçesi var; gerekçe çürütülmeden karar değişmez.

| Konu | Karar | Gerekçe |
|---|---|---|
| Blok sözdizimi | **Girinti** (`:` ile açılır) | Python okunabilirliği ana hedef. Süslü parantez gürültü. |
| Değişken tanımı | `x = 1` bir kapsamdaki **ilk** atama tanımlar | `let` gürültüsü yok; resolver statik olarak bildiği için Python'daki çalışma zamanı sürprizleri yok |
| Dış kapsama yazma | `outer x = 1` | Python'un `nonlocal`/`global` ikilisi kafa karıştırıcı, tek kelimeye indirdik |
| Doğruluk (truthiness) | **Sadece `nil` ve `false` yanlıştır** | `0` ve `""` doğrudur. `if count:` klasik hatasını kökten siler (Lua/Ruby modeli) |
| Tipler | Kademeli — yazmazsan dinamik, yazarsan **zorlanır** | TypeScript'in aksine anotasyon süs değil, denetlenir |
| Bellek | **Çöp toplayıcı** (önce mark-sweep, sonra nesil bazlı) | Betik dilinde ownership öğrenme maliyeti kabul edilemez |
| Hata modeli | **İstisna** (`try`/`catch`/`throw`) | Python/JS bilen biri için sıfır öğrenme maliyeti. Host sınırında yakalanır, mod hatası uygulamayı düşürmez |
| Eşzamanlılık | Tek iş parçacığı + olay döngüsü, `async`/`await` | JS modeli. Veri yarışı sınıfını tamamen ortadan kaldırır |
| Sayılar | `int` (64-bit işaretli) ve `float` (f64) | İki tip yeter. `i8..u64`, `f32` sonraki fazlarda tipli alanlar için |
| String | `str`, UTF-8, **değişmez** | Değişmezlik eşzamanlılık ve hash'lenebilirlik için bedava kazanç |
| Aralık | `0..10` (hariç), `0..=10` (dahil) | Rust'ın açıklığı; Python'un `range()` fonksiyon çağrısından daha okunur |
| Modüller | `use "github.com/kullanici/repo"` | Go modeli — registry yok, sunucu yok, hesap yok, domain yok |
| Yorum | `#` satır sonuna kadar | Python |
| Dosya uzantısı | `.rai` | Çakışan canlı format yok (doğrulandı: sadece ölü/niş formatlar) |

---

## 3. Sözdizimi turu

### 3.1 Değişkenler ve temel tipler

```python
ad = "Raiden"              # str  — çıkarım
sayi = 42                  # int
oran = 3.14                # float
acik = true                # bool
yok = nil                  # nil

hp: int = 100              # açık tip — denetlenir
isim: str = 42             # ✗ TİP HATASI: str bekleniyordu, int verildi

buyuk = 1_000_000          # okunabilirlik için alt çizgi
hex   = 0xFF
ikili = 0b1010
```

### 3.2 String'ler

```python
a = "çift tırnak"
b = 'tek tırnak da olur'
c = f"Merhaba {ad}, canın {hp}"          # f-string
d = f"{hp / 2:.1f} kaldı"                 # biçimlendirme
e = r"C:\yol\kacissiz"                    # ham string
f = """
    çok satırlı
    string
"""
```

### 3.3 Koleksiyonlar

```python
liste = [1, 2, 3]
liste.push(4)
ilk = liste[0]
son = liste[-1]                # negatif indeks sondan sayar
dilim = liste[1..3]            # [2, 3]

harita = {"ad": "Raiden", "hp": 100}
harita["mp"] = 50
if "hp" in harita:
    print(harita["hp"])

# Tipli koleksiyonlar
skorlar: list[int] = [10, 20, 30]
envanter: map[str, int] = {"altin": 500}
```

### 3.4 Akış kontrolü

```python
if hp <= 0:
    print("öldün")
elif hp < 30:
    print("dikkat")
else:
    print("iyisin")

while hp > 0:
    hp = hp - 1

for i in 0..10:                # 0,1,...,9
    print(i)

for item in envanter:          # koleksiyon gezme
    print(item)

for anahtar, deger in harita.items():
    print(f"{anahtar} = {deger}")

for i in 0..100:
    if i % 2 == 0:
        continue
    if i > 50:
        break
```

### 3.5 Fonksiyonlar

```python
fn selamla(ad):
    return f"Merhaba {ad}"

fn mesafe(ax: f64, ay: f64, bx: f64, by: f64) -> f64:
    dx = ax - bx
    dy = ay - by
    return sqrt(dx*dx + dy*dy)

fn vur(hedef, hasar = 10):             # varsayılan parametre
    hedef.hp = hedef.hp - hasar

# Ok fonksiyonu (kapanış)
kare = (x) => x * x
liste.map((x) => x * 2)

# Kapanış gerçekten kapatır
fn sayac():
    n = 0
    return () => {
        outer n = n + 1
        return n
    }
```

### 3.6 Sınıflar ve trait'ler

```python
trait Cizilebilir:
    fn ciz(self, yzc)

class Varlik:
    hp: int = 100
    fn init(self, x, y):
        self.x = x
        self.y = y

    fn hasarAl(self, miktar: int):
        self.hp = self.hp - miktar
        if self.hp <= 0:
            self.oldu()

    fn oldu(self):
        print("varlık öldü")

class Gemi(Varlik, Cizilebilir):        # kalıtım + trait
    fn init(self, x, y):
        super.init(x, y)
        self.kredi = 0

    fn ciz(self, yzc):
        yzc.sprite("ship", self.x, self.y)

    fn oldu(self):                       # geçersiz kılma
        print("gemi patladı")

g = Gemi(100, 200)
g.hasarAl(150)
```

### 3.7 Hatalar

```python
fn bol(a, b):
    if b == 0:
        throw Error("sıfıra bölme")
    return a / b

try:
    sonuc = bol(10, 0)
catch e:
    print(f"hata yakalandı: {e.message}")
finally:
    print("her durumda çalışır")
```

### 3.8 Async (Faz 3+)

```python
async fn veriCek(url: str) -> map:
    cevap = await http.get(url)
    return cevap.json()

async fn main():
    veri = await veriCek("https://ornek/api")
    print(veri["ad"])

    # Paralel
    sonuclar = await all([
        veriCek("https://a"),
        veriCek("https://b"),
    ])
```

### 3.9 Modüller

```python
use std.math                              # yerleşik kütüphane
use std.json as j                         # yeniden adlandırma
use "github.com/RaidenTechnology/rai-http"          # repo bazlı
use "github.com/RaidenTechnology/rai-http" @ "v0.3.1"   # sürüm sabitleme

print(math.sqrt(16))
```

**Registry yok.** `use` bir git reposunu işaret eder, sürümler git tag'leridir. Bu kararın bedeli merkezi arama olmaması; kazancı hiçbir altyapı, hesap veya domain gerektirmemesi.

### 3.10 `view` — bildirimsel arayüz (Faz 5)

```python
view Hud(oyuncu):
    <column gap=4 pad=8>
        <bar value={oyuncu.hp} max=100 color="#e33"/>
        <text size=14>{oyuncu.ad} — {oyuncu.kredi} CR</text>
        <button on_click={() => magazaAc()}>MAĞAZA</button>
    </column>
```

Bir `view` **sanal ağaç** döndürür. Onu piksele çeviren şey host'un renderer'ıdır:
tarayıcıda DOM, Minecraft'ta envanter GUI'si, masaüstünde native pencere.
Dil hiçbir çizim yapmaz — sadece "ne çizilmeli"yi tarif eder.

### 3.11 Dekoratörler

```python
@fast                       # sıcak yol — VM agresif optimize eder, ileride native derlenir
fn carpim(a: f64, b: f64) -> f64:
    return a * b

@deprecated("v0.4'te kalkacak, yerine yeniHesapla kullan")
fn hesapla():
    pass
```

---

## 4. Tip sistemi

**Kademeli (gradual).** Anotasyon yoksa tip `any`'dir ve denetim çalışma zamanına ertelenir. Anotasyon varsa derleme zamanında zorlanır.

```
int  float  str  bool  nil  any
list[T]  map[K,V]  fn(T...) -> R
<sınıf adı>  <trait adı>
T?          # nullable: T veya nil
```

Kurallar:

1. **Anotasyon bir sözleşmedir.** `x: int` yazdıysan oraya `str` koyamazsın — derleme hatası.
2. **Çıkarım yereldir.** `x = 5` → `int`. Fonksiyonlar arası global çıkarım yok (karmaşıklık/fayda oranı kötü).
3. **`any` bulaşıcı değildir.** `any` bir değeri tipli bir yere koyarken çalışma zamanı kontrolü eklenir.
4. **`T?` açmadan kullanılamaz.** `nil` olabilecek bir değere doğrudan erişmek hatadır:

```python
fn bul(ad: str) -> Oyuncu?:
    ...

o = bul("raiden")
print(o.hp)              # ✗ HATA: o, nil olabilir
if o != nil:
    print(o.hp)          # ✓ daraltma (narrowing) sonrası güvenli
```

Bu tek kural, gerçek yazılımdaki hataların büyük bir dilimini kapatır — ve Faz 2'nin en değerli işi budur.

---

## 5. Bellek modeli

- Her şey referans, `int`/`float`/`bool`/`nil` değer semantiğiyle davranır.
- **Çöp toplayıcı:** Faz 3'te mark-and-sweep. Nesil bazlı iyileştirme sonra.
- Kapanışlar kapattıkları değişkenleri **upvalue** olarak yaşatır.
- Host sınırında **handle tablosu**: Rai nesnesi host'a verilirken doğrudan pointer değil, tabloya indeks geçer. Bu, iki GC'nin (Rai'nin ve host'un) birbirini kilitlemesini engeller — gömme işinin en klasik sızıntı kaynağı burasıdır.

---

## 6. Gömme modeli

Dilin varlık sebebi. Host tarafı C API'si üzerinden konuşur:

```c
rai_vm*  rai_new(void);
void     rai_free(rai_vm*);
int      rai_eval(rai_vm*, const char* src, rai_value* out);
void     rai_bind(rai_vm*, const char* ad, rai_fn fn);   /* host fonksiyonu tanıt */
void     rai_set_limit(rai_vm*, uint64_t adim, uint64_t bellek);  /* sonsuz döngü koruması */
```

Kritik güvenlik kuralı: **bir mod host'u kilitleyemez.** `rai_set_limit` ile adım ve bellek tavanı konur; aşan betik istisna alır, host çalışmaya devam eder.

Host bağlayıcıları (dil çekirdeğinin dışında, her proje için ayrı):

| Bağlayıcı | Hedef proje | Sağladığı API |
|---|---|---|
| `game.*` | STAR BREAKER | sprite, ses, giriş, varlık |
| `mc.*` | Raiden RPG | oyuncu, dünya, event, envanter |
| `sys.*` | RaidenAI / Crimson | dosya, süreç, ağ |
| `serial.*` | SUAS2026 İHA | telemetri, uçuş verisi |
| `ui.*` | hepsi | `view` renderer'ı |

---

## 7. Anahtar kelimeler

Toplam **28**. Bu sayı v1.0'a kadar 30'u geçmeyecek.

```
fn      class   trait   view    use     as      outer
if      elif    else    while   for     in      break
continue return try     catch   finally throw   async
await   self    super   true    false   nil     pass
```

Operatör olarak da: `and` `or` `not` `is`

---

## 8. Operatör öncelikleri

Pratt parser bu tabloyu birebir uygular. Düşükten yükseğe:

| Öncelik | Operatörler | Birleşme |
|---|---|---|
| 1 | `or` | sol |
| 2 | `and` | sol |
| 3 | `not` (tekli) | sağ |
| 4 | `==` `!=` `<` `>` `<=` `>=` `is` `in` | sol |
| 5 | `..` `..=` | sol |
| 6 | `+` `-` | sol |
| 7 | `*` `/` `//` `%` | sol |
| 8 | `-` `+` `~` (tekli) | sağ |
| 9 | `**` | **sağ** |
| 10 | `()` `[]` `.` `?.` (sonek) | sol |

---

## 9. Gramer (EBNF)

Sözcüksel katman `INDENT` / `DEDENT` / `NEWLINE` token'ları üretir (Python modeli).

```ebnf
program        = { statement } EOF ;

statement      = simple_stmt NEWLINE | compound_stmt ;

simple_stmt    = assign | expr | return_stmt | "break" | "continue"
               | "pass" | throw_stmt | use_stmt ;

assign         = [ "outer" ] target [ ":" type ] "=" expr ;
target         = IDENT | postfix "." IDENT | postfix "[" expr "]" ;

return_stmt    = "return" [ expr ] ;
throw_stmt     = "throw" expr ;
use_stmt       = "use" ( dotted_name | STRING [ "@" STRING ] ) [ "as" IDENT ] ;

compound_stmt  = if_stmt | while_stmt | for_stmt | fn_decl
               | class_decl | trait_decl | try_stmt | view_decl ;

block          = ":" NEWLINE INDENT { statement } DEDENT
               | ":" simple_stmt NEWLINE ;

if_stmt        = "if" expr block { "elif" expr block } [ "else" block ] ;
while_stmt     = "while" expr block ;
for_stmt       = "for" IDENT { "," IDENT } "in" expr block ;

fn_decl        = { decorator } [ "async" ] "fn" IDENT "(" [ params ] ")"
                 [ "->" type ] block ;
params         = param { "," param } ;
param          = IDENT [ ":" type ] [ "=" expr ] ;
decorator      = "@" IDENT [ "(" [ args ] ")" ] NEWLINE ;

class_decl     = "class" IDENT [ "(" IDENT { "," IDENT } ")" ] ":" NEWLINE
                 INDENT { field_decl | fn_decl } DEDENT ;
field_decl     = IDENT ":" type [ "=" expr ] NEWLINE ;

trait_decl     = "trait" IDENT ":" NEWLINE
                 INDENT { "fn" IDENT "(" [ params ] ")" [ "->" type ] NEWLINE } DEDENT ;

try_stmt       = "try" block "catch" IDENT block [ "finally" block ] ;

view_decl      = "view" IDENT "(" [ params ] ")" ":" NEWLINE
                 INDENT element DEDENT ;
element        = "<" IDENT { attr } ( "/>" | ">" { element | text | "{" expr "}" } "</" IDENT ">" ) ;
attr           = IDENT [ "=" ( STRING | NUMBER | "{" expr "}" ) ] ;

(* ifadeler — öncelik tablosuna göre *)
expr           = or_expr ;
or_expr        = and_expr { "or" and_expr } ;
and_expr       = not_expr { "and" not_expr } ;
not_expr       = "not" not_expr | compare ;
compare        = range_expr { ( "==" | "!=" | "<" | ">" | "<=" | ">=" | "is" | "in" ) range_expr } ;
range_expr     = sum [ ( ".." | "..=" ) sum ] ;
sum            = product { ( "+" | "-" ) product } ;
product        = unary { ( "*" | "/" | "//" | "%" ) unary } ;
unary          = ( "-" | "+" | "~" | "await" ) unary | power ;
power          = postfix [ "**" unary ] ;
postfix        = primary { "(" [ args ] ")" | "[" expr "]" | "." IDENT | "?." IDENT } ;

primary        = NUMBER | STRING | FSTRING | "true" | "false" | "nil"
               | IDENT | "self" | "super"
               | "(" expr ")"
               | "[" [ expr { "," expr } ] "]"
               | "{" [ map_entry { "," map_entry } ] "}"
               | lambda ;
lambda         = "(" [ params ] ")" "=>" ( expr | block ) ;
map_entry      = expr ":" expr ;

type           = IDENT [ "[" type { "," type } "]" ] [ "?" ]
               | "fn" "(" [ type { "," type } ] ")" [ "->" type ] ;
```

---

## 10. Faz 1 kapsamı (MVP sınırı)

**Bu listeye girmeyen hiçbir şey Faz 1'de yazılmaz.** Kapsam patlaması bu tür projelerin bir numaralı ölüm sebebi.

### İÇERİDE ✅

- `int`, `float`, `str`, `bool`, `nil`, `list`, `map`
- Değişkenler, aritmetik, karşılaştırma, mantık
- `if` / `elif` / `else`, `while`, `for..in`, `break`, `continue`
- `fn`, kapanışlar, özyineleme, varsayılan parametreler, ok fonksiyonları
- `class` — alanlar, metotlar, tekli kalıtım, `self`, `super`
- `try` / `catch` / `throw`
- `use std.*` — sadece yerleşik modüller
- f-string
- REPL + `rai run dosya.rai`
- **Satır/sütun bilgili hata mesajları** (baştan, sonradan eklemek acı verir)

### DIŞARIDA ❌ (fazı belirtilmiş)

| Özellik | Faz |
|---|---|
| Tip anotasyonlarının **zorlanması** (Faz 1'de ayrıştırılır, yok sayılır) | 2 |
| `trait` | 2 |
| `T?` ve nil-güvenliği | 2 |
| Bytecode VM + GC | 3 |
| `async` / `await` | 3 |
| `@fast` | 3 |
| Repo bazlı `use` | 4 |
| C API / WASM / gömme | 4 |
| `view` | 5 |
| JVM köprüsü | 6 |
| LSP, formatter, paket çözücü | 7 |

---

## 11. Açık sorular

### 11a. Örnek yazarken ORTAYA ÇIKAN kusurlar (26 Tem, `examples/01-07`)

Bunlar spec'i okurken değil, dili *kullanırken* çıktı. Faz 1 başlamadan karara bağlanmalı.

1. **🔴 Koşullu ifade (ternary) gramerde YOK.** `04-koleksiyonlar.rai`'de refleksle `a if a > b else b` yazdım — ayrıştırılamaz. Karar gerek: ya `expr if expr else expr` eklenir (Python biçimi, yeni anahtar kelime gerekmez, `if`/`else` zaten var) ya da yasaklanıp `math.max()` gibi fonksiyonlara zorlanır. **Öneri: eklensin** — ok fonksiyonlarının içinde tek satırlık dallanma sık lazım oluyor ve alternatifi çirkin.

2. **🔴 Çok satırlı ok fonksiyonu grameri tutarsız.** `02` ve `06`'da `() => { ... }` yazdım ama gramer `=> ( expr | block )` diyor, `block` ise `:` + INDENT demek. Girinti bazlı bir dilde, fonksiyon çağrısının parantezi *içinde* çok satırlı gövde girintiyle çözülemez — Python bu yüzden lambda'yı tek ifadeyle sınırlar. **Öneri: `=> { ... }` süslü parantezli gövde girinti kuralına resmi istisna olarak tanınsın.** Gramere eklenmeli, yoksa `game.on("x", () => {...})` gibi en sık kullanılacak kalıp yazılamaz.

3. **🟡 `init` içinde tanımlanmamış alan yaratmak serbest mi?** `03`'te `self.ad = ad`, `06`'da `self.sonAtis = 0.0` — ikisi de sınıf gövdesinde bildirilmemişti. Python serbest bırakır, tip denetleyicisi için kâbustur. **Öneri: serbest ama uyarı ver**; `class` gövdesinde bildirilen alanlar tipli, bildirilmeyenler `any`.

4. **🟡 `/` int'ler üzerinde ne döndürür?** `07`'de `(n * 100) / toplam` yazdım. Python 3 modeli (`/` her zaman float, `//` tam bölme) mi, C modeli mi? **Öneri: Python modeli** — `5 / 2 == 2.5`, `5 // 2 == 2`. Sessiz tam sayı kırpması hata kaynağı.

5. **🟡 Prelude (global yerleşikler) tanımsız.** `02`'de `sqrt()` çıplak yazdım, spec ise `math.sqrt()` diyor. Hangi isimler `use` gerektirmeden hazır? **Öneri: dar bir prelude** — `print`, `len`, `type`, `int`, `float`, `str`, `bool`, `Error`, `range`. Matematik `use std.math` ister.

6. **🟡 f-string içinde tırnak çakışması.** `04`'te `f"kredi: {oyuncu['kredi']}"` yazdım. Lexer, f-string'in `{...}` bölümünü ayrı bir alt-lexer'la mı tarayacak? **Öneri: evet** — `{}` içi tam ifade olarak yeniden taranır, iç tırnak serbest.

7. **🟡 Yerleşik `Error` gerçek bir sınıf olmalı.** `05`'te `class YetersizKredi(Error)` yazdım. Yani `Error` prelude'da bir sınıf ve kalıtılabilir; `message` alanı var.

### 11b. Baştan bilinen açık sorular

8. **Girinti + `view` çakışması.** JSX'in klasik belirsizliği: `a < b` karşılaştırma mı, etiket açılışı mı? Çözüm adayı: lexer'a mod desteği (Faz 1'de kullanılmasa da altyapısı konur).
9. **`for` üzerinde çoklu değişken.** `for k, v in harita.items()` mi, yoksa demet (tuple) tipi mi eklensin? Demet eklemek çekirdeği büyütür.
10. **String biçimlendirme dili.** `{x:.2f}` Python'ın mini dili — ne kadarı desteklenecek?
11. **Modül önbelleği nerede yaşayacak?** `~/.rai/pkg/<host>/<kullanıcı>/<repo>@<tag>/` öneri.
12. **`switch`/`match` gerekli mi?** Anahtar kelime bütçesi 28/30. `match` güçlü ama pahalı — v1.0 sonrasına bırakılabilir.
13. **Operatör aşırı yükleme?** Muhtemelen hayır — çekirdeği küçük tutma kuralına aykırı.

---

## 12. Sıradaki adım

Faz 0'ın kalan işi: **`examples/` altında 30 örnek program.** Spec'i okuyarak değil, dili *kullanarak* test etmenin tek yolu bu. Örnekler yazılırken sözdizimindeki rahatsız edici noktalar ortaya çıkar ve spec düzeltilir — kod yazılmadan önce.

İlk parti `examples/` altında. Hedef: 30'a tamamlamak, sonra Faz 1'e geçmek.
