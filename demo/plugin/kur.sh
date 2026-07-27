#!/bin/sh
# RaidenEnderChest — derle ve test sunucusuna kur.
#
#   sh demo/plugin/kur.sh
#
# DLL'i de yeniler: rs_jni.cpp ya da src/ altında bir şey değiştiyse gerekiyor.
# Sunucu KAPALIYKEN çalıştır — açıkken jar ve DLL kilitli olur.

set -e
KOK=$(cd "$(dirname "$0")/../.." && pwd)
SUNUCU="$HOME/Plugins/mc-sword/server"
ARACLAR="$HOME/Plugins/mc-sword/.tools"

export JAVA_HOME="$ARACLAR/jdk-21.0.11+10"
export PATH="$JAVA_HOME/bin:$ARACLAR/apache-maven-3.9.9/bin:$HOME/tools/w64devkit/bin:$PATH"

echo "--> yerel kütüphane (make jni)"
cd "$KOK" && make jni

echo "--> plugin (mvn package)"
cd "$KOK/demo/plugin" && mvn -q package

echo "--> kurulum"
cp target/RaidenEnderChest.jar "$SUNUCU/plugins/"

# enderchest.rai veri klasöründe VARSA dokunulmuyor: oradaki kullanıcının
# düzenlediği sürüm ve plugin onu okuyor. Jar'daki kopya sadece ilk kurulumda
# açılıyor. Yeni sürüme geçmek isteniyorsa dosya elle silinir.
if [ -f "$SUNUCU/plugins/RaidenEnderChest/enderchest.rai" ]; then
  echo "    not: mevcut enderchest.rai korundu (yenisi için o dosyayı sil)"
fi

echo "--> hazır. Sunucuyu start.bat ile aç, oyunda /ec yaz."
