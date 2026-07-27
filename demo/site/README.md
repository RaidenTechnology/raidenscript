# RAIDEN PARÇA — RaidenScript ile animasyonlu mağaza

Faz 5 `web` bağlayıcısının kanıtı: bir bilgisayar bileşenleri mağazası. Katalog,
fiyat biçimleme, indirim, stok, filtre, sıralama, sepet matematiği, kargo eşiği,
sistem kurucusunun uyumluluk kuralları **ve hangi animasyonun ne zaman oynayacağı**
tek bir `.rai` dosyasında.

## Çalıştırma

```bash
python -m http.server 8801 --directory .
```

Sonra `http://localhost:8801/demo/site/` adresini aç. (`.wasm` için doğru MIME
gerekiyorsa depo kökündeki sunucu betiğini kullan.) Yükleme bitince konsola ölçüm
düşüyor: derleme süresi, kurulum süresi, düğüm sayısı, host hatası sayısı.

Önce `make wasm` çalıştırılmış olmalı — sayfa `dist/raidenscript.js` dosyasını
doğrudan yüklüyor.

## Sınır nerede?

| | Ne yapıyor |
|---|---|
| `magaza.rai` | Mağazanın tamamı. Ürünler, kurallar, hesaplar, metinler, animasyon zamanlaması. |
| `bindings/js/web.js` | 17 ilkel: düğüm aç, metin yaz, sınıf tak, zaman çizelgesini tarayıcıya devret. Ürün/fiyat/sepet kelimesi geçmez. |
| `app.js` | WASM'ı kaldır, bağlayıcıyı VM'e tak, `baslat()` çağır. 60 satır. |
| `style.css` | Görünüm. Hangi sınıfın nereye takılacağına betik karar veriyor. |

Sınama, banka demosundakiyle aynı: `web.js`'in gövdelerini terminale yazan
sürümlerle değiştir, `magaza.rai` tek satır değişmeden çalışsın.

## Animasyon nasıl çalışıyor

Betik **kare başına stil yazmıyor**. Zaman çizelgesini bir kez bildiriyor:

```python
web.zaman("yukselt")
web.anahtarKare("yukselt", 0,   "opacity",   "0")
web.anahtarKare("yukselt", 0,   "transform", "translateY(30px) scale(.985)")
web.anahtarKare("yukselt", 100, "opacity",   "1")
web.anahtarKare("yukselt", 100, "transform", "translateY(0) scale(1)")
```

sonra oynatmayı devrediyor:

```python
web.oynat(id, "yukselt", 620, "cubic-bezier(.16,1,.3,1)", sira * 55, 1)
```

20 kart için sınır **20 kez** geçiliyor (~0.04 ms); animasyonun kendisi
tarayıcıda, compositor iş parçacığında koşuyor. Kare döngüsü kurulsaydı aynı iş
saniyede 60 kez, 1200 çağrıyla yapılırdı ve betik yavaşladığında animasyon
takılırdı.

Kaydırmaya bağlı olanlar (`web.kaydirmaBagla`, `web.sayfaBagla`) `ViewTimeline` /
`ScrollTimeline` kullanıyor — orada da betik hiç çalışmıyor.

## Ölçüm (yerel, Chrome)

```
betik derleme     22.0 ms      (1.000 satırlık magaza.rai)
sayfa kurulumu    83.2 ms      (424 düğüm, ~1.400 host çağrısı)
host hatası        0
```

## Bu demoda yakalanan hatalar

1. **Sınırda daraltma:** host'tan gelen her sayı `double`. `URUNLER[u]` kayan
   noktalı indisi kabul etmiyor — "Sepete ekle" düğmesi sessizce değil, açık bir
   tanı basarak çalışmıyordu. Olay girişlerinde `int(...)` zorunlu.
2. **`inherits: false` + `::after`:** sayaç animasyonu `--sayi`'yi elemana
   yazıyor ama `counter()` `::after`'da okunuyor; kalıtım kapalıyken pseudo-eleman
   değeri göremiyor ve 0'da kalıyor. `inherits: true` şart.
3. **Türkçe büyük harf tuzağı:** sayfa `lang="tr"` iken CSS `text-transform:
   uppercase` "Raiden"i "RAİDEN" yapıyor. Marka adlarına `lang="en"` verildi.
4. **Türkçe yüzde eki:** "%66'sı", "%88'i", "%78'i" — son rakamın okunuşuna göre
   değişiyor. Tek biçimli ek yanlış yazım üretiyor; tablo betikte.
