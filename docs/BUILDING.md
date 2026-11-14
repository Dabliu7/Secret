# Tutorial: compilar e testar o port

Este guia leva do zero até a `libmtasa.so` carregada dentro do GTA:SA no aparelho.

**O que você terá no final:** o jogo abrindo normalmente, com o payload carregado
e três hooks ativos (game loop, render 2D, toque), confirmados pelo `logcat`.
Ainda **não** há multiplayer, HUD nem Lua — isso é o roadmap em
[`FEASIBILITY.md`](FEASIBILITY.md). As partes 1 e 2 são validadas pelo CI; as
partes 3 a 5 (APK e aparelho) ainda não foram testadas em um dispositivo real.

| Parte | O que faz | Precisa do jogo? |
|---|---|---|
| 1 | Preparar o ambiente | não |
| 2 | Compilar a `libmtasa.so` | não |
| 3 | Conferir os símbolos da `libGTASA.so` | sim |
| 4 | Injetar no APK e instalar | sim |
| 5 | Verificar no aparelho | sim |

---

## Parte 1 — Ambiente

Use Linux (Ubuntu 22.04+). No Windows, use **WSL2** com Ubuntu; no macOS os
passos são os mesmos trocando `linux` por `darwin` no nome do NDK.

### 1.1 Ferramentas de build

```sh
sudo apt-get update
sudo apt-get install -y git cmake ninja-build unzip curl
cmake --version    # precisa ser 3.22 ou mais novo
```

### 1.2 Android NDK r27c

```sh
mkdir -p ~/android && cd ~/android
curl -LO https://dl.google.com/android/repository/android-ndk-r27c-linux.zip
unzip -q android-ndk-r27c-linux.zip
echo 'export ANDROID_NDK_HOME=$HOME/android/android-ndk-r27c' >> ~/.bashrc
source ~/.bashrc
```

Confira: `ls $ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake` deve existir.

### 1.3 Ferramentas de APK (só para as partes 3 a 5)

```sh
sudo apt-get install -y adb apktool zipalign apksigner openjdk-17-jdk-headless binutils
```

Se a sua distribuição não tiver `zipalign`/`apksigner` como pacotes, eles vêm no
*build-tools* do Android SDK (`sdkmanager "build-tools;35.0.0"`); adicione a pasta
ao `PATH`.

---

## Parte 2 — Compilar

```sh
git clone https://github.com/Dabliu7/Secret.git
cd Secret
cmake --preset android-arm64
cmake --build --preset android-arm64
```

Resultado: `build/android-arm64/libmtasa.so`. Na primeira vez o CMake baixa o
And64InlineHook do GitHub (precisa de internet).

Confira que é um ELF arm64 e que exporta o ponto de entrada:

```sh
READELF=$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf
$READELF -h build/android-arm64/libmtasa.so | grep Machine      # AArch64
$READELF --dyn-syms build/android-arm64/libmtasa.so | grep JNI_OnLoad
```

### Variações

| Quero | Comando |
|---|---|
| Incluir o transporte RakNet | `cmake --preset android-arm64 -DMTASA_WITH_RAKNET=ON` |
| Build de depuração | `cmake --preset android-arm64 -DCMAKE_BUILD_TYPE=Debug` |
| Recompilar do zero | `rm -rf build/android-arm64` e repetir |
| Rodar os testes (sem NDK) | `cmake --preset host && cmake --build --preset host && ctest --preset host` |

### Sem instalar nada: usar o CI

Todo push roda o workflow `build`. Em **Actions → build → (último run) →
Artifacts** baixe `libmtasa-arm64-v8a-and64-raknetOFF` e pule para a Parte 3.

---

## Parte 3 — Conferir os símbolos do jogo (Fase 0)

Você precisa de uma cópia do GTA:SA para Android **que você comprou**, em uma
versão que tenha biblioteca **arm64**.

### 3.1 Extrair o APK do seu aparelho

Ative *Opções do desenvolvedor → Depuração USB*, conecte o cabo e:

```sh
adb devices                                   # o aparelho deve aparecer como "device"
adb shell pm path com.rockstargames.gtasa
```

- **Uma linha** (`.../base.apk`): `adb pull <caminho> gtasa.apk`.
- **Várias linhas** (`base.apk` + `split_config.arm64_v8a.apk` ...): a instalação
  é em *split APKs* e as bibliotecas ficam no split. O `patch_apk.sh` não trata
  esse caso; una os splits em um APK único antes (por exemplo com o APKEditor,
  comando `merge`) e use o resultado como `gtasa.apk`.

### 3.2 Extrair e conferir a `libGTASA.so`

```sh
mkdir -p work
unzip -o gtasa.apk 'lib/arm64-v8a/libGTASA.so' -d work/
tools/dump_symbols.sh work/lib/arm64-v8a/libGTASA.so
```

Se o `unzip` não achar o arquivo, o APK é só armv7 e **não serve** — é preciso
uma versão mais nova do jogo.

Saída esperada:

```
  ok       _ZN5CGame7ProcessEv
  ok       _Z13Render2dStuffv
  ok       _Z14AND_TouchEventiiii
```

### 3.3 Se algum aparecer como `AUSENTE`

Os nomes no código são candidatos; a sua build pode usar outros.

1. Procure o nome real em `work/symbols/dynsym_demangled.txt`:
   ```sh
   grep -i 'CGame::Process\|Render2dStuff\|TouchEvent' work/symbols/dynsym_demangled.txt
   ```
2. **Achou com outro nome/assinatura:** troque a string correspondente em
   `kWanted`, em `src/game/Symbols.cpp`. Se a assinatura de `AND_TouchEvent` for
   diferente de `(int,int,int,int)`, ajuste também `hook_AND_TouchEvent` em
   `src/game/GameHooks.cpp` — assinatura errada causa crash no primeiro toque.
3. **Não está exportado:** abra a `libGTASA.so` no Ghidra, localize a função e
   coloque o offset em `kOffsets` (`src/game/Symbols.cpp`):
   ```cpp
   {"_Z13Render2dStuffv", 0x123456},   // offset relativo à base da imagem
   ```
4. Recompile (Parte 2) e rode o `dump_symbols.sh` de novo.

Anote o conteúdo de `work/symbols/build_id.txt`: offsets só valem para essa build.

---

## Parte 4 — Injetar no APK e instalar

```sh
tools/patch_apk.sh gtasa.apk build/android-arm64/libmtasa.so
```

O script descompacta o APK, copia a `libmtasa.so`, adiciona
`System.loadLibrary("mtasa")` logo após o carregamento da `libGTASA.so`,
reempacota e assina com uma chave de debug. Saída: `work/gtasa-mtasa.apk`.

### Instalar

A assinatura mudou, então o Android recusa atualizar por cima do original. É
preciso desinstalar antes — **isso apaga saves e pode apagar os dados do jogo**.

```sh
# 1. backup dos dados (OBB) e dos saves
adb pull /sdcard/Android/obb/com.rockstargames.gtasa work/obb-backup
adb pull /sdcard/Android/data/com.rockstargames.gtasa work/data-backup

# 2. trocar o APK
adb uninstall com.rockstargames.gtasa
adb install work/gtasa-mtasa.apk

# 3. devolver os dados
adb push work/obb-backup /sdcard/Android/obb/com.rockstargames.gtasa
```

Em Android 11+ o acesso a `Android/obb` e `Android/data` via `adb` pode ser
negado; nesse caso copie as pastas com um gerenciador de arquivos que tenha essa
permissão, ou deixe o jogo baixar os dados de novo.

Não redistribua `work/gtasa-mtasa.apk`: ele contém o jogo da Rockstar.

---

## Parte 5 — Verificar no aparelho

Em um terminal, antes de abrir o jogo:

```sh
adb logcat -c && adb logcat -s MTASA
```

Abra o jogo. Sequência esperada:

```
I MTASA: MTA:SA Android 0.1.0 (hook=and64, net=none)
I MTASA: libGTASA.so em 0x7a........ (/data/app/.../lib/arm64/libGTASA.so)
I MTASA: hooks instalados: ok (backend and64)
I MTASA: primeiro CGame::Process interceptado
I MTASA: primeiro frame de overlay (backend=0x0)
```

As duas últimas linhas só aparecem depois de entrar em uma partida (o loop do
jogo e o desenho 2D precisam estar rodando). `backend=0x0` é normal: o backend de
render ainda não existe.

### Diagnóstico

| Sintoma | Causa provável | O que fazer |
|---|---|---|
| Nenhuma linha `MTASA` | A lib não foi carregada | Confira `unzip -l work/gtasa-mtasa.apk \| grep libmtasa` e a mensagem "smali patcheado" do script |
| `sym ... NÃO encontrado` | Símbolo diferente nessa build | Parte 3.3 |
| `libGTASA.so não foi carregada` | Nome da lib diferente | Ajuste `kGameLib` em `src/core/main.cpp` |
| `hooks instalados: FALHA` | O backend não conseguiu patchear | Veja as linhas `A64_HOOK` no `adb logcat` completo |
| Crash ao tocar na tela | Assinatura de `AND_TouchEvent` errada | Parte 3.3, item 2 |
| Crash ao iniciar | Hook em função errada (offset de outra build) | Confira o build-id; `adb logcat -b crash` mostra o backtrace |
| `INSTALL_FAILED_UPDATE_INCOMPATIBLE` | Original ainda instalado | `adb uninstall com.rockstargames.gtasa` |
| `INSTALL_FAILED_NO_MATCHING_ABIS` | Aparelho não é arm64 | Use um aparelho arm64 |
| Jogo fecha pedindo licença/dados | Verificação da loja ou OBB ausente | Restaure o OBB; a verificação de licença pode recusar APKs reassinados |

Para traduzir um backtrace de crash em linhas de código:

```sh
adb logcat -b crash -d | $ANDROID_NDK_HOME/ndk-stack -sym build/android-arm64
```

---

## Ciclo de desenvolvimento

Depois da primeira instalação, cada alteração é:

```sh
cmake --build --preset android-arm64
tools/patch_apk.sh gtasa.apk build/android-arm64/libmtasa.so
adb install -r work/gtasa-mtasa.apk      # -r funciona: a chave de debug é a mesma
adb logcat -s MTASA
```

O `patch_apk.sh` guarda a chave em `work/debug.keystore`; não apague esse
arquivo, ou será preciso desinstalar e restaurar os dados de novo.

O próximo passo de código é a Fase 2 do roadmap: implementar
`gfx::IRenderBackend` sobre o RenderWare Im2D e registrá-lo com
`OverlayRenderer::Get().SetBackend(...)`.
