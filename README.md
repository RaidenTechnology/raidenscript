# RaidenScript

> ⚠️ **Faz 0 — tasarım aşaması.** Henüz çalışan bir derleyici yok, olması da beklenmiyor.
> Bu depo şu an bir dil *tanımından* ve örnek programlardan ibaret.

**RaidenScript** (kısaca **RS**), gömülebilir bir betik dili. Python'un okunabilirliğini,
JavaScript'in çalışma modelini, C++'ın isteğe bağlı tiplerini ve HTML'in bildirimsel
arayüz fikrini tek dilde birleştirmeyi hedefliyor.

```python
fn selamla(ad: str) -> str:
    return f"Merhaba {ad}"

class Gemi(Varlik):
    hp: int = 100
    fn ciz(self, yzc):
        yzc.sprite("ship", self.x, self.y)

view Hud(oyuncu):
    <column gap=4 pad=8>
        <bar value={oyuncu.hp} max=100 color="#e33"/>
        <button on_click={() => magazaAc()}>MAĞAZA</button>
    </column>
```

## Neden?

Tek başına çalışan programlar yazılabilir, ama asıl amaç **başka bir uygulamanın
içine girip ona programlanabilirlik kazandırmak**: oyuna mod, sunucuya plugin,
masaüstü uygulamasına otomasyon.

Dil çekirdeği kasıtlı olarak küçük (28 anahtar kelime). Güç, host'un sağladığı
bağlayıcılardan gelir.

## Adlandırma

| | |
|---|---|
| Tam ad | RaidenScript |
| Kısaltma | RS |
| Komut | `rs` |
| Uzantı | `.rai` — "**Rai**den"in ilk 3 harfi, aynı zamanda 雷 (gök gürültüsü) kökü |

`.rs` kullanılamaz (Rust'ın), `.rds` R'ın, `.rsc` MikroTik'in, `.ra` RealAudio'nun.

## Yol haritası

| Faz | İş | Durum |
|---|---|---|
| 0 | Dil tanımı + örnek programlar | 🟡 sürüyor (7/30 örnek) |
| 1 | Lexer, parser, AST, tree-walking yorumlayıcı | ⬜ |
| 2 | Kademeli tipler, nil-güvenliği, stdlib | ⬜ |
| 3 | Bytecode VM + çöp toplayıcı | ⬜ |
| 4 | C API, WASM, gömme | ⬜ |
| 5 | `view` — bildirimsel arayüz | ⬜ |
| 6 | JVM köprüsü | ⬜ |
| 7 | LSP, formatter, paket çözücü | ⬜ |
| 8 | Self-hosting | ⬜ |

## Dosyalar

- [`SPEC.md`](SPEC.md) — dil tanımı, gramer, tasarım kararları ve gerekçeleri
- [`examples/`](examples/) — örnek programlar (spec'i test etmenin tek gerçek yolu)
- [`WORKLOG.md`](WORKLOG.md) — çalışma günlüğü

---

Raiden Technology
