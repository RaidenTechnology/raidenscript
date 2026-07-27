# RAIDEN BANK — tarayıcıda RaidenScript

Bir internet bankacılığı arayüzü. Bankacılığın tamamı [`banka.rai`](banka.rai)
içinde çalışıyor; JavaScript yalnızca DOM'a çiziyor.

## Çalıştırma

`dist/` git'te yok, önce WASM üretilmeli:

```bash
make wasm                       # depo kökünde, emsdk gerekir
python -m http.server 8801      # yine depo kökünde
```

Sonra <http://localhost:8801/demo/banka/> — demo PIN `8419`.

`file://` ile açılmaz: `banka.rai` fetch ile okunuyor ve `.wasm` bir HTTP
sunucusu ister.

## İş bölümü

| | |
|---|---|
| **banka.rai** | Hesaplar, bakiyeler, IBAN mod-97 doğrulaması, para aritmetiği ve biçimlendirme, transfer kuralları (günlük limit, EFT ücreti, para birimi, aynı hesap), hesap hareketleri, bileşik faiz projeksiyonu, PIN denemesi ve blokaj, **ekrandaki her metin** |
| **app.js** | `document.createElement`. Tek bir bankacılık kuralı yok. |

Sınama şu: `app.js` yerine terminale yazan bir sürüm koyarsan `banka.rai`
değişmeden çalışır — nitekim [`examples/16-banka-arayuzu.rai`](../../examples/16-banka-arayuzu.rai)
aynı çekirdeği çizgi karakterleriyle çiziyor ve **aynı sayıları** üretiyor.

## Host arayüzü (9 ilkel)

Betik `include ui` der, host bunları kaydeder:

```
ui.ekran(ad)                                   görünür ekranı değiştir
ui.temizle(bolge)                              bir bölgeyi boşalt
ui.metin(alan, deger)                          tek bir metin alanını doldur
ui.hesapKarti(no, ad, bakiye, birim, iban, secili)
ui.ekstreSatiri(tarih, tur, tutar, bakiye, aciklama, artiMi)
ui.projeksiyonSatiri(donem, bakiye, kazanc)
ui.durum(tur, baslik, satir)                   bildirim
ui.girdi(alan) -> str                          form değerini OKU
```

Hepsi çizim ilkelidir. `ui.transferYap` diye bir şey yok — o kural betikte.

## Dize kanalı

Bu demo `capi.h`'daki dize kanalını kullanır. Sınır hâlâ skalerdir: sayılar
`args[]` dizisinden geçer. Dizeler sayının içine sıkıştırılmaz, yanında ayrı
bir kanalda taşınır (`rs_arg_str` / `rs_return_str`).

Metin host'tan betiğe **çekilir**, itilmez:

```
betik:  iban = ui.girdi("iban")
host :  girdi: (alan) => document.getElementById('alan_' + alan).value
```

JS tarafında bu tamamen görünmez — host fonksiyonu sıradan JS dizesi alır ve
döndürür; dönüş `string` ise köprü `rs_return_str`'e çevirir.

## Denenecekler

| Senaryo | Ne olur |
|---|---|
| `TR91 0006 2001 1978 6457 8414 26` | Aynı banka, ücretsiz havale |
| `TR56 0011 1000 0000 0102 0304 05` | Başka banka, 15,50 EFT ücreti kesilir |
| Yukarıdakinin son hanesini değiştir | mod-97 tutmaz, alan kırmızıya döner |
| Bakiyeden büyük tutar | Yetersiz bakiye, bakiye değişmez |
| Birikim hesabından 60.000 | Günlük limit 50.000'i aşar |
| `TR26 0006 1005 1978 6457 8413 55` | TRY hesaptan USD hesaba — reddedilir |
| Tutara `abc` yaz | Hangi karakterin bozuk olduğu söylenir |
| PIN'i 3 kez yanlış gir | Hesap bloke olur |
