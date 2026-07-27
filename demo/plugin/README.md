# RaidenEnderChest — kuralları RaidenScript'te olan bir Paper plugin'i

Faz 6'nın (JVM köprüsü) kanıtı. Ender sandığı plugin'i: `/ec`, `/ec <oyuncu>`,
`/ec yenile`. İzinler, bekleme süresi, her mesajın metni ve rengi, ses ve
parçacık seçimi, hedefin bilgilendirilmesi — hepsi `enderchest.rai` içinde.

## Sınır nerede?

| | Ne yapıyor |
|---|---|
| `src/main/resources/enderchest.rai` | Plugin'in tüm kuralları |
| `McBaglayici.java` | `mc.*` ilkelleri (14 tane). Kural yok, sadece "şu oyuncuya şu metni yaz" |
| `EnderChestPlugin.java` | DLL'i kur, VM'i aç, betiği yükle, komutu ilet |
| `bindings/jvm/rs_jni.cpp` | capi.h ↔ JNI. Dil çekirdeğine dokunmaz |
| `bindings/jvm/java/.../RaidenScript.java` | Köprünün Java yüzü — `rs-host.js`'in birebir karşılığı |

## Kurulum

```bash
make jni
```

sonra:

```bash
cd demo/plugin && mvn package
```

`target/RaidenEnderChest.jar` sunucunun `plugins/` klasörüne kopyalanır. DLL
jar'ın içinde gelir; plugin açılışta veri klasörüne çıkarıp yükler, elle kopyalama
yok.

## Asıl fayda: sunucuyu kapatmadan kural değiştirme

`plugins/RaidenEnderChest/enderchest.rai` düzenlenir, oyun içinde `/ec yenile`
yazılır. Canlı sunucuda doğrulandı:

```
[00:13:04] Raiden Ender Sandığı kuralları hazır — bekleme 5 sn      ← açılış
   ... BEKLEME = 5.0 satırı 12.0 yapıldı, /ec yenile ...
[00:13:27] Raiden Ender Sandığı kuralları hazır — bekleme 12 sn     ← yeniden derlendi
[00:13:27] enderchest.rai yeniden yüklendi.
```

Bozuk düzenleme sunucuyu komutsuz bırakmıyor — yeni VM ancak sorunsuz kurulursa
eskisinin yerine geçiyor:

```
[00:13:37] ERROR: enderchest.rai yüklenemedi:
hata: dosya sonunda kapanmamış '('
   --> enderchest.rai:167:1
6 hata
```

…ve eski kurallar çalışmaya devam etti.

## Sınır kuralları (capi.h)

- **Nesne geçmez.** Oyuncu kimliği UUID **dizesi** olarak taşınır.
- **Betiğe dize argümanı geçilemez** (`rs_call` yalnızca `double` alır). Komut
  bağlamını betik ÇEKER: `mc.komutOyuncu()`, `mc.komutArg(0)`. Banka ve mağaza
  demolarındaki desenin aynısı.
- **Host'tan gelen sayılar `double`'dır.** İndis olarak kullanmadan önce `int()`.

`examples/10-minecraft-plugin.rai` bu köprüyle **çalışmaz**: içinde nesne
döndüren `mc.yakindakiVarliklar()` ve metotlu `oyuncu.mesaj()` var. O dosya hedef
taslağı; çalışan desen budur.

## ⚠️ Bilinen sınır: özyineleme derinliği

Betik özyinelemesi JVM'in **yerel yığınını** tüketiyor ve taşma **sunucuyu
sessizce öldürüyor** — Java istisnası yok, `hs_err` günlüğü yok, çıkış kodu 127.

| JVM yığını | Güvenli derinlik |
|---|---|
| varsayılan (1 MB) | ~500 |
| `-Xss16m` | ~5.000 |

Bu plugin'in kuralları özyinelemesiz, ama sunucuyu `-Xss16m` ile başlatmak
tavsiye edilir. Kalıcı çözüm yorumlayıcıda çağrı derinliği sayacı — bkz.
`faz5-web-tasarim.md` (H6).

## Minecraft olmadan sınama

`mc.*` ilkellerinin sahtesini kaydedip kuralları JVM'de doğrudan koşturabilirsin;
sunucu gerekmez. Sekiz senaryo böyle doğrulandı: izin yok, bekleme aktif,
beklemesiz muafiyet, olmayan oyuncu, başkasının sandığı, hedefe haber, kendi
adını yazma, temel izin yok.
