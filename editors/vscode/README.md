# RaidenScript for VS Code

`.rai` dosyaları için sözdizimi renklendirmesi, girinti kuralları ve dosya ikonu.

## Kurulum

```
python build-vsix.py
code --install-extension raidenscript-0.1.0.vsix --force
```

Sonra VS Code'da **Developer: Reload Window** (Ctrl+Shift+P).

Klasörü elle `~/.vscode/extensions/` içine kopyalamak **çalışmaz** — VS Code 1.74'ten
beri kurulu eklentileri `extensions.json` kaydından okuyor, klasörü taramıyor.

## İçerik

| | |
|---|---|
| Dilbilgisi | `syntaxes/raidenscript.tmLanguage.json` — 29 anahtar kelime, f-string içi ifadeler, ham dizeler, `view` etiketleri |
| Girinti | `language-configuration.json` — `:` sonrası artır, `return`/`break` sonrası azalt, offside katlama |
| Tanımlayıcılar | UTF-8 harf kabul edilir (SPEC 2.2) — `değer`, `çarpan`, `ölçüm` doğru renklenir |
| İkon | `icons/rai.png` — Seti gibi dil ikonu destekleyen temalar dosya ağacında gösterir |

## Sözdizimi renkleri neye bağlanır

Renkler doğrudan kodlanmaz; temanın kendi renklerine bağlanan TextMate kapsamları kullanılır.

| Kapsam | Ne renklenir |
|---|---|
| `keyword.control` | `if` `while` `return` `try` `throw` |
| `keyword.control.import` | `import` `include` |
| `storage.type` | `fn` `class` `trait` `view` |
| `entity.name.function` | tanım ve çağrı adları |
| `entity.name.type` | `class` adları ve BüyükHarfle başlayan tanımlayıcılar |
| `support.function.builtin` | prelude: `print` `len` `type` `range` `assert` `Error` |
| `constant.language` | `true` `false` `nil` |
| `variable.language` | `self` `super` |
| `string.quoted.interpolated` | `f"..."` — içindeki `{ }` ifadeleri ayrıca renklenir |
