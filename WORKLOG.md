# RaidenScript — Çalışma Günlüğü

> Devlog'ların ham maddesi. Her dönüm noktasında güncellenir.

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
