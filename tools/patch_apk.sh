#!/usr/bin/env bash
# Injeta libmtasa.so em um APK do GTA:SA que VOCÊ possui (uso em desenvolvimento).
# O APK resultante não deve ser redistribuído: ele contém o jogo da Rockstar.
#
# Requisitos: apktool, zipalign, apksigner (Android build-tools), keytool.
# Uso: tools/patch_apk.sh gtasa.apk build/android-arm64/libmtasa.so [saida.apk]
set -euo pipefail

APK="${1:?uso: $0 <gtasa.apk> <libmtasa.so> [saida.apk]}"
LIB="${2:?faltou o caminho da libmtasa.so}"
OUT="${3:-work/gtasa-mtasa.apk}"
WORK="work/apk"
KEYSTORE="${KEYSTORE:-work/debug.keystore}"

rm -rf "$WORK" && mkdir -p "$WORK" "$(dirname "$OUT")"
apktool d -f -o "$WORK/src" "$APK"

[ -f "$WORK/src/lib/arm64-v8a/libGTASA.so" ] || {
  echo "ERRO: este APK não tem lib/arm64-v8a/libGTASA.so (build só armv7?)" >&2; exit 1; }
cp "$LIB" "$WORK/src/lib/arm64-v8a/libmtasa.so"

# Acha o smali que faz System.loadLibrary("GTASA") e carrega "mtasa" logo depois,
# reutilizando o mesmo registrador.
SMALI="$(grep -rlE 'const-string [vp][0-9]+, "GTASA"' "$WORK/src"/smali* | head -n1)"
[ -n "$SMALI" ] || { echo "ERRO: loadLibrary(\"GTASA\") não encontrado no smali" >&2; exit 1; }
awk '
  /const-string [vp][0-9]+, "GTASA"/ { reg = $2; sub(/,$/, "", reg) }
  { print }
  reg != "" && /Ljava\/lang\/System;->loadLibrary\(Ljava\/lang\/String;\)V/ {
    print "    const-string " reg ", \"mtasa\""
    print "    invoke-static {" reg "}, Ljava/lang/System;->loadLibrary(Ljava/lang/String;)V"
    reg = ""; patched = 1
  }
  END { exit patched ? 0 : 1 }
' "$SMALI" > "$SMALI.new" || { echo "ERRO: falha ao patchear $SMALI" >&2; exit 1; }
mv "$SMALI.new" "$SMALI"
echo "smali patcheado: $SMALI"

# Ponteiros de heap com tag no byte alto quebram VMs que empacotam ponteiros.
sed -i 's/<application /<application android:allowNativeHeapPointerTagging="false" /' \
  "$WORK/src/AndroidManifest.xml"

apktool b -o "$WORK/unsigned.apk" "$WORK/src"
zipalign -f -p 4 "$WORK/unsigned.apk" "$WORK/aligned.apk"
[ -f "$KEYSTORE" ] || keytool -genkeypair -keystore "$KEYSTORE" -storepass android -keypass android \
  -alias debug -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=debug"
apksigner sign --ks "$KEYSTORE" --ks-pass pass:android --out "$OUT" "$WORK/aligned.apk"
echo "pronto: $OUT  (adb install -r $OUT && adb logcat -s MTASA)"
