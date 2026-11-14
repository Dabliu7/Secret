#!/usr/bin/env bash
# Injeta libmtasa.so em um GTA:SA que VOCÊ possui (uso em desenvolvimento).
# Os APKs resultantes não devem ser redistribuídos: contêm o jogo da Rockstar.
#
# Aceita um APK único ou uma pasta com split APKs (base.apk + split_*.apk).
# Requisitos: apktool, zip, zipalign, apksigner (Android build-tools), keytool.
#
# Uso: tools/patch_apk.sh <gtasa.apk | pasta_com_apks> <libmtasa.so> [pasta_saida]
set -euo pipefail

SRC="${1:?uso: $0 <gtasa.apk | pasta_com_apks> <libmtasa.so> [pasta_saida]}"
LIB="${2:?faltou o caminho da libmtasa.so}"
OUT="${3:-work/out}"
WORK="work/apk"
KEYSTORE="${KEYSTORE:-work/debug.keystore}"
ABI_DIR="lib/arm64-v8a"

[ -f "$LIB" ] || { echo "ERRO: $LIB não existe" >&2; exit 1; }
rm -rf "$WORK" "$OUT" && mkdir -p "$WORK" "$OUT"

if [ -d "$SRC" ]; then
  BASE="$SRC/base.apk"
  [ -f "$BASE" ] || { echo "ERRO: $SRC não contém base.apk" >&2; exit 1; }
  mapfile -t SPLITS < <(find "$SRC" -maxdepth 1 -name '*.apk' ! -name 'base.apk' | sort)
else
  BASE="$SRC"
  SPLITS=()
fi

# Qual APK carrega as bibliotecas nativas arm64?
has_lib() { unzip -l "$1" "$ABI_DIR/libGTASA.so" >/dev/null 2>&1; }
LIB_APK=""
for apk in "$BASE" "${SPLITS[@]}"; do
  if has_lib "$apk"; then LIB_APK="$apk"; break; fi
done
[ -n "$LIB_APK" ] || { echo "ERRO: nenhum APK contém $ABI_DIR/libGTASA.so (build só armv7?)" >&2; exit 1; }
echo "bibliotecas nativas em: $LIB_APK"

# --- 1. base.apk: patch de smali (-r: não decodifica recursos, rebuild mais robusto)
apktool d -f -r -o "$WORK/base" "$BASE"

SMALI="$(grep -rlE 'const-string [vp][0-9]+, "GTASA"' "$WORK/base"/smali* | head -n1 || true)"
[ -n "$SMALI" ] || { echo "ERRO: loadLibrary(\"GTASA\") não encontrado no smali" >&2; exit 1; }
# Carrega "mtasa" logo depois de "GTASA", reutilizando o mesmo registrador.
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

# APK único: a lib vai dentro do próprio base antes do rebuild.
if [ "$LIB_APK" = "$BASE" ]; then
  cp "$LIB" "$WORK/base/$ABI_DIR/libmtasa.so"
fi
apktool b -o "$WORK/unsigned/base.apk" "$WORK/base"

# --- 2. splits: copiados como estão; o split de ABI recebe a lib
for apk in "${SPLITS[@]}"; do
  name="$(basename "$apk")"
  cp "$apk" "$WORK/unsigned/$name"
  if [ "$apk" = "$LIB_APK" ]; then
    mkdir -p "$WORK/add/$ABI_DIR"
    cp "$LIB" "$WORK/add/$ABI_DIR/libmtasa.so"
    # -0: sem compressão, exigido quando o app usa extractNativeLibs=false
    (cd "$WORK/add" && zip -q -0 -r "../unsigned/$name" lib)
    echo "libmtasa.so adicionada a $name"
  fi
done

# --- 3. alinhar e assinar todos com a MESMA chave
[ -f "$KEYSTORE" ] || keytool -genkeypair -keystore "$KEYSTORE" -storepass android -keypass android \
  -alias debug -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=debug"
for apk in "$WORK"/unsigned/*.apk; do
  name="$(basename "$apk")"
  zipalign -f -p 4 "$apk" "$WORK/aligned-$name"
  apksigner sign --ks "$KEYSTORE" --ks-pass pass:android --out "$OUT/$name" "$WORK/aligned-$name"
done
rm -f "$OUT"/*.idsig

echo
echo "pronto: $OUT/"
if [ ${#SPLITS[@]} -gt 0 ]; then
  echo "instalar: adb install-multiple $OUT/*.apk"
else
  echo "instalar: adb install $OUT/$(basename "$BASE")"
fi
echo "log:      adb logcat -s MTASA"
