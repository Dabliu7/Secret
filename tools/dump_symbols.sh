#!/usr/bin/env bash
# Fase 0: extrai a tabela de símbolos exportados da libGTASA.so e confere os
# símbolos que o payload precisa. Uso: tools/dump_symbols.sh caminho/libGTASA.so
set -euo pipefail

SO="${1:?uso: $0 <libGTASA.so> [dir_saida]}"
OUT="${2:-work/symbols}"
READELF="${READELF:-$(command -v llvm-readelf || command -v readelf)}"
FILT="$(command -v llvm-cxxfilt || command -v c++filt || echo cat)"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

mkdir -p "$OUT"
"$READELF" -h "$SO" | grep -E 'Class|Machine' | tee "$OUT/header.txt"
grep -q 'AArch64' "$OUT/header.txt" || { echo "ERRO: não é um ELF arm64" >&2; exit 1; }
"$READELF" -n "$SO" | grep -i 'build id' | tee "$OUT/build_id.txt" || true

# colunas: offset tamanho tipo nome_mangled
"$READELF" --dyn-syms -W "$SO" \
  | awk '$4=="FUNC" || $4=="OBJECT" { if ($7!="UND") print $2, $3, $4, $8 }' \
  | sort -k4 > "$OUT/dynsym.txt"
awk '{print $4}' "$OUT/dynsym.txt" | "$FILT" | paste -d' ' "$OUT/dynsym.txt" - > "$OUT/dynsym_demangled.txt"
echo "símbolos exportados: $(wc -l < "$OUT/dynsym.txt") -> $OUT/dynsym_demangled.txt"

# Confere os nomes referenciados em src/game/Symbols.cpp
missing=0
for sym in $(grep -oE '"_Z[A-Za-z0-9_]+"' "$ROOT/src/game/Symbols.cpp" | tr -d '"' | sort -u); do
  if grep -q " $sym\$" "$OUT/dynsym.txt"; then
    echo "  ok       $sym"
  else
    echo "  AUSENTE  $sym"; missing=$((missing + 1))
  fi
done
exit $((missing > 0))
