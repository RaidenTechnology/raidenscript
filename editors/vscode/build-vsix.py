#!/usr/bin/env python3
"""VSIX paketleyici - vsce/npm gerektirmez.

VSIX bir OPC (Open Packaging Conventions) zip'idir: eklenti dosyalari
'extension/' altina girer, yanina bir vsixmanifest ve icerik tipi tablosu
konur. vsce'nin yaptigi ekstra isler (bagimlilik toplama, README isleme)
bu eklentide gerekmiyor - saf bir dilbilgisi eklentisi.

    python build-vsix.py            -> raidenscript-<surum>.vsix

Kurulum:  code --install-extension raidenscript-<surum>.vsix --force
"""

import json
import pathlib
import sys
import zipfile

HERE = pathlib.Path(__file__).resolve().parent

# Pakete girecek dosyalar. Yeni bir dilbilgisi/varlik eklenirse buraya yazilir.
FILES = [
    "package.json",
    "language-configuration.json",
    "syntaxes/raidenscript.tmLanguage.json",
    "icons/rai.png",
    "README.md",
]

CONTENT_TYPES = """<?xml version="1.0" encoding="utf-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension="json" ContentType="application/json" />
  <Default Extension="vsixmanifest" ContentType="text/xml" />
  <Default Extension="png" ContentType="image/png" />
  <Default Extension="md" ContentType="text/markdown" />
  <Default Extension="txt" ContentType="text/plain" />
</Types>
"""

MANIFEST = """<?xml version="1.0" encoding="utf-8"?>
<PackageManifest Version="2.0.0" xmlns="http://schemas.microsoft.com/developer/vsx-schema/2011">
  <Metadata>
    <Identity Language="en-US" Id="{name}" Version="{version}" Publisher="{publisher}" />
    <DisplayName>{display}</DisplayName>
    <Description xml:space="preserve">{description}</Description>
    <Tags>{tags}</Tags>
    <Categories>{categories}</Categories>
    <GalleryFlags>Public</GalleryFlags>
    <Properties>
      <Property Id="Microsoft.VisualStudio.Code.Engine" Value="{engine}" />
      <Property Id="Microsoft.VisualStudio.Code.ExtensionKind" Value="ui,workspace" />
    </Properties>
  </Metadata>
  <Installation>
    <InstallationTarget Id="Microsoft.VisualStudio.Code" />
  </Installation>
  <Dependencies />
  <Assets>
    <Asset Type="Microsoft.VisualStudio.Code.Manifest" Path="extension/package.json" Addressable="true" />
  </Assets>
</PackageManifest>
"""


def xml_escape(s):
    return (s.replace("&", "&amp;").replace("<", "&lt;")
             .replace(">", "&gt;").replace('"', "&quot;"))


def main():
    pkg = json.loads((HERE / "package.json").read_text(encoding="utf-8"))
    out = HERE / "raidenscript-{}.vsix".format(pkg["version"])

    manifest = MANIFEST.format(
        name=pkg["name"],
        version=pkg["version"],
        publisher=pkg["publisher"],
        display=xml_escape(pkg["displayName"]),
        description=xml_escape(pkg["description"]),
        tags=",".join(pkg.get("keywords", [])),
        categories=",".join(pkg.get("categories", [])),
        engine=pkg["engines"]["vscode"],
    )

    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("extension.vsixmanifest", manifest)
        z.writestr("[Content_Types].xml", CONTENT_TYPES)
        for rel in FILES:
            src = HERE / rel
            if not src.exists():
                print("  atlandi (yok): {}".format(rel))
                continue
            z.write(src, "extension/" + rel)
            print("  + extension/{}".format(rel))

    print("\n{} ({} bayt)".format(out.name, out.stat().st_size))
    print("kur: code --install-extension \"{}\" --force".format(out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
