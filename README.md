# MTA:SA → Android (arm64) — base do port

Payload nativo (`libmtasa.so`) carregado dentro do processo do GTA: San Andreas
mobile para hospedar um cliente compatível com MTA:SA sobre a `libGTASA.so`.

**Estado: esqueleto.** O que existe hoje carrega, localiza a `libGTASA.so`,
resolve três símbolos e instala três hooks (game loop, render 2D, toque). Nada foi
testado em dispositivo ainda.

Leia primeiro: [`docs/FEASIBILITY.md`](docs/FEASIBILITY.md) — estudo de
viabilidade, arquitetura e roadmap. Ponto mais importante: o módulo de rede do
MTA é fechado, então compatibilidade com servidores oficiais não é alcançável só
com este repositório.

## Build

```sh
export ANDROID_NDK_HOME=/caminho/android-ndk-r27c
cmake --preset android-arm64
cmake --build --preset android-arm64      # -> build/android-arm64/libmtasa.so
```

Opções: `-DMTASA_HOOK_BACKEND=and64|dobby|none`, `-DMTASA_WITH_RAKNET=ON`.

Testes de host (sem NDK, sem hooks):

```sh
cmake --preset host && cmake --build --preset host && ctest --preset host
```

## Testar em um aparelho

Requer um APK do jogo que você possui, com `lib/arm64-v8a/libGTASA.so`.

```sh
tools/dump_symbols.sh caminho/libGTASA.so          # Fase 0: confere os símbolos
tools/patch_apk.sh gtasa.apk build/android-arm64/libmtasa.so
adb install -r work/gtasa-mtasa.apk && adb logcat -s MTASA
```

Não redistribua o APK gerado: ele contém o jogo da Rockstar.

## Licença

A definir. Ao incorporar código do `mtasa-blue`, o projeto passa a ser GPLv3.
Projeto não afiliado à Rockstar Games, Take-Two ou à equipe do Multi Theft Auto.
