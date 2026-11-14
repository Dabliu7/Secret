# Estudo de viabilidade e arquitetura — cliente MTA:SA sobre `libGTASA.so` (Android arm64)

> Status: documento de partida. Tudo marcado **[verificar na Fase 0]** é hipótese
> que depende de inspecionar o binário real; nada aqui foi validado em dispositivo.

## 0. Veredito

| Pilar | Viabilidade | Risco dominante |
|---|---|---|
| Loader + hooks arm64 | Alta | Baixo — técnica consolidada |
| Camada de jogo (`game_sa`/`multiplayer_sa`) | Média | **Volume**: reescrita quase total |
| Overlay 2D (CEGUI, `dxDraw*`) | Alta | Thread/fila de render do port mobile |
| Shaders `dxCreateShader` (HLSL/.fx) | Baixa a média | Tradução HLSL→GLSL ES + framework de Effects |
| VM Lua | Alta | Memória de assets, não a VM |
| Rede compatível com servidores **oficiais** | **Bloqueada** | Módulo de rede do MTA é fechado |

Três premissas do enunciado precisam de correção, porque mudam o plano:

1. **O módulo de rede do MTA não é código aberto.** O repositório `mtasa-blue` (GPLv3)
   contém cliente, servidor, `game_sa`, GUI etc., mas `netc.dll` (cliente) e
   `net.dll/.so` (servidor) são distribuídos só como binário. Eles embutem o RakNet
   modificado, a criptografia do transporte, a verificação de serial e o anti-cheat.
   Sem um `netc` para Android arm64 não há como falar com servidores existentes.
   Este projeto **não** inclui engenharia reversa do `netc` nem contorno do
   anti-cheat; os caminhos legítimos estão na seção 4.
2. **O MTA usa Lua 5.1 (PUC, com patches próprios), não LuaJIT.** Trocar a VM é
   uma decisão de projeto, com custos (seção 5).
3. **Não existe `mta_dxproxy.dll`.** O hook de Direct3D 9 vive dentro de `core.dll`
   (`CProxyDirect3D9` / `CProxyDirect3DDevice9`). Os módulos reais do cliente são
   `core`, `client` (deathmatch), `game_sa`, `multiplayer_sa`, `cgui`, `netc`, `xmll`
   e o `loader`.

Conclusão: um cliente **compatível em scripting e gameplay**, conectando a um
servidor com módulo de rede aberto, é viável como esforço de vários anos-pessoa.
Entrar em servidores oficiais/comunitários atuais depende de cooperação da equipe
do MTA (um `netc` arm64) — é uma dependência organizacional, não técnica.

## 1. Repositório e automação

```
.
├── .github/workflows/build.yml   # NDK r27c -> libmtasa.so arm64-v8a + testes de host
├── CMakeLists.txt                # alvo `mtasa` (SHARED) e opções
├── CMakePresets.json             # presets `android-arm64` e `host`
├── cmake/Dependencies.cmake      # FetchContent com commits fixados (hook, RakNet)
├── include/mtasa/                # API interna estável (Hook.h, Log.h)
├── src/
│   ├── core/                     # JNI_OnLoad, bootstrap, log
│   ├── hook/                     # abstração de inline hook + localização de módulos
│   ├── game/                     # símbolos da libGTASA.so e hooks do jogo
│   ├── gfx/                      # overlay e interface de backend de render
│   ├── net/                      # INetTransport (+ RakNet opcional)
│   └── lua/                      # alocador com teto de memória
├── tests/                        # testes de host (sem dispositivo)
├── tools/                        # dump_symbols.sh, patch_apk.sh
└── docs/
```

Não há `/jni`: esse diretório é convenção do `ndk-build` (`Android.mk`). Com CMake
+ toolchain file do NDK ele não tem função. Quando o código portado do `mtasa-blue`
entrar, ele vai em `vendor/mtasa-blue` como submódulo, compilado seletivamente.

Decisões do build:

- **Só `arm64-v8a`**, API 24, `c++_static` (o jogo traz o próprio STL; não
  compartilhar evita conflito de ABI), visibilidade `hidden` e `--exclude-libs,ALL`
  para que só `JNI_OnLoad` seja exportado.
- **Página de 16 KB** (`max-page-size=16384`) e nenhum `4096` fixo no código.
- Dependências **fixadas por commit**; o RakNet é montado à mão porque o CMake do
  upstream é da era 2.6.
- O CI tem uma matriz: `raknet=OFF` é a combinação que bloqueia merge;
  `raknet=ON` roda como experimental (`continue-on-error`).

## 2. Injeção e loader

### 2.1 Do modelo Windows para o Android

| | Windows (MTA atual) | Android (este projeto) |
|---|---|---|
| Processo | `Multi Theft Auto.exe` cria `gta_sa.exe` suspenso | O próprio app do jogo; não há processo externo |
| Injeção | `CreateRemoteThread` + `LoadLibrary(core.dll)` | `System.loadLibrary("mtasa")` no APK reempacotado |
| Entrada | `DllMain` | `JNI_OnLoad` |
| Endereços | Fixos (EXE 1.0 US sem ASLR) | Base por ASLR + símbolo (`dlsym`) ou offset por build |
| Hooks | JMP de 5 bytes, `naked` + asm x86 | Trampolim arm64 (`LDR x17; BR x17`, 16 bytes) em C++ |

Sem root não existe injeção entre processos no Android (SELinux, `ptrace`
restrito). As formas de entrar no processo, em ordem de preferência:

1. **Patch de smali** — após o `System.loadLibrary("GTASA")` da activity,
   carregar `mtasa`. É o que `tools/patch_apk.sh` faz. Simples, ordem de carga
   determinística (rodamos antes do main loop nativo).
2. **`DT_NEEDED` na `libGTASA.so`** (`patchelf --add-needed libmtasa.so`). Não toca
   no dex, mas somos carregados *antes* do jogo — por isso `main.cpp` tem o
   fallback que espera a biblioteca aparecer.
3. **Launcher próprio** (APK separado que carrega os `.so` do jogo instalado via
   `createPackageContext`). Melhor experiência de distribuição — só o nosso código
   é distribuído — porém exige reimplementar a activity/JNI glue do jogo. Meta de
   médio prazo, não de protótipo.

Em todos os casos o APK final é reassinado; **distribui-se o patcher e a
`libmtasa.so`, nunca o APK ou assets da Rockstar**.

### 2.2 Qual build do jogo

`arm64-v8a` só existe nas builds mais recentes do GTA:SA mobile (família 2.10+;
as 1.08/2.00 clássicas são armv7) **[verificar na Fase 0]**. A build alvo deve
ser fixada pelo *build-id* do ELF e o payload deve se recusar a hookar uma build
desconhecida — é o que `ResolveAll` faz hoje ao exigir todos os símbolos.

Grande vantagem em relação ao PC: a `libGTASA.so` exporta boa parte dos nomes
C++ na `.dynsym` **[verificar]**. Isso permite resolver por nome
(`_ZN5CGame7ProcessEv`) em vez de manter tabelas de endereços, e hookar em
fronteira de função em vez de no meio dela.

### 2.3 Frameworks de hook

| | And64InlineHook | Dobby | ShadowHook (ByteDance) |
|---|---|---|---|
| Tamanho | 2 arquivos | médio | médio |
| Manutenção | parada | irregular | ativa, usada em produção |
| Unhook / hooks encadeados | não | sim | sim |
| Funções curtas, instalação com threads rodando | frágil | razoável | tratado |
| Integração CMake pura | trivial | sim | via AAR/prefab (pede Gradle) ou fontes à mão |

Recomendação: **toda chamada passa por `mtasa::hook::Install`** (já no esqueleto).
`And64InlineHook` basta para as Fases 1–2 (poucos hooks, instalados antes do jogo
rodar). Ao passar de algumas dezenas de hooks, migrar o backend para
**ShadowHook**. O Dobby ficou fora do build: o HEAD atual (`5dfc854`) não monta
com o NDK r27c — `closure_bridge_arm64.asm` usa relocações `@PAGE` de Mach-O.
Complementos:

- **Hook de PLT/GOT** (bhook/xHook) para *imports* da `libGTASA.so` —
  `eglSwapBuffers`, `fopen`/`AAsset_*` (redirecionar arquivos de resources), `gl*`.
  É mais seguro que inline hook e não depende de símbolo interno.
- **xDL** para ler `.symtab` de funções não exportadas, quando houver.

### 2.4 Pontos de interceptação iniciais

| Objetivo | Alvo candidato **[verificar]** | Uso |
|---|---|---|
| Game loop | `CGame::Process` | pulso de rede, Lua, sync |
| Desenho 2D | `Render2dStuff` | overlay MTA sobre o HUD |
| Toque | `AND_TouchEvent(int,int,int,int)` | UI consome antes do jogo |
| Init do RW | `CGame::InitialiseRenderWare` | criar texturas/fontes |
| Arquivos | `OS_FileOpen` / imports de libc | `engineLoad*`, IMG virtuais |

### 2.5 O custo real: `game_sa` e `multiplayer_sa`

É aqui que o esforço se concentra, não no loader:

- `game_sa` espelha centenas de classes do jogo com **layout x86 de 32 bits** e
  endereços do EXE 1.0. Em arm64 ponteiros têm 8 bytes, o alinhamento muda e o
  port mobile alterou structs (rendering, áudio, controles). Cada classe precisa
  ser remapeada e validada (`static_assert(sizeof/offsetof)`).
- `multiplayer_sa` são centenas de hooks `naked` em asm x86 no meio de funções.
  Nada é reaproveitável; cada um vira um hook de função inteira em C++.
- Subsistemas sem equivalente direto: controles (touch × teclado/mouse — binds de
  script precisam de botões virtuais), áudio (BASS tem build Android; o áudio do
  jogo é OpenAL), modelos de PC em memória de celular.

Estratégia: **não portar `game_sa` de cima para baixo**. Portar por fatia vertical
guiada por funcionalidade (ped local → peds remotos → veículos → ...), mantendo as
interfaces `CGame*` do SDK do MTA para que `client/` (deathmatch) compile quase
intacto por cima.

## 3. Pipeline gráfico

### 3.1 O que muda

No PC o MTA embrulha `IDirect3DDevice9` e desenha no `EndScene/Present`. No
Android não há device para embrulhar: o jogo usa RenderWare sobre OpenGL ES 2.0, e
o port mobile enfileira comandos para um **thread de render separado**
**[verificar na Fase 0 — é o item gráfico mais importante]**. Chamar `gl*` direto
de um hook no thread do jogo corromperia o estado ou falharia por falta de contexto.

Vulkan não se aplica: o contexto é do jogo, e ele é GLES.

### 3.2 Camadas

```
 scripts Lua (dxDraw*, gui*)      CEGUI 0.4 (cgui)
            │                           │
            ▼                           ▼
      DxBatcher  ───────────►  gfx::IRenderBackend  (Vertex2D, textura, scissor)
                                        │
                     ┌──────────────────┴──────────────────┐
                     ▼                                     ▼
          RwIm2DBackend (padrão)                 GlesBackend (diagnóstico)
   RwIm2DRenderIndexedPrimitive, RwRaster     GL direto no thread de render,
   -> entra na fila de render do jogo         com save/restore de estado
```

- **Backend padrão sobre RenderWare Im2D.** Usar a API que o próprio jogo usa
  para o HUD resolve de uma vez thread, contexto, shaders e formato de vértice.
  Texturas são `RwRaster`. É a mesma abordagem usada por clientes comunitários de
  multiplayer para SA mobile para desenhar UI.
- **CEGUI**: o MTA usa CEGUI 0.4 com `DirectX9GUIRenderer`. O renderer do CEGUI é
  pequeno (`addQuad`, `doRender`, `createTexture`, clipping): escreve-se um
  `RwGUIRenderer` sobre `IRenderBackend`. Fontes continuam via FreeType. Como
  `cgui` fica atrás das interfaces `CGUI*` do SDK, trocá-lo depois por algo mais
  leve não afeta os scripts.
- **Entrada**: toque → `injectMousePosition/ButtonDown/Up`; teclado virtual via
  JNI (`InputMethodManager`) → `injectChar`. Escala por DPI obrigatória: GUIs de
  servidor são desenhadas para 1080p com mouse.
- **`dxDraw*`**: um batcher que agrupa por textura e emite listas indexadas.
  `dxCreateRenderTarget` → raster de câmera do RW (FBO).

### 3.3 Shaders (`dxCreateShader`) — o maior risco gráfico

Os shaders do MTA são arquivos **.fx de D3DX Effects em HLSL (SM2/SM3)**. Não há
tradutor de chamadas possível aqui; é preciso:

1. Parser do framework de Effects (techniques, passes, render states, semânticas
   e anotações que o MTA usa para ligar matrizes e texturas).
2. HLSL → GLSL ES: glslang (front-end HLSL) → SPIR-V → SPIRV-Cross → GLSL ES 1.00.
   HLSL legado tem cobertura parcial; haverá shaders que não traduzem.
3. `engineApplyShaderToWorldTexture`: substituir o programa que o pipeline mobile
   monta para o material — exige engenharia reversa do gerador de shaders do port.

Plano: fora do MVP. `dxCreateShader` retorna `false` (scripts bem escritos já
tratam isso) e o trabalho começa só depois do sync básico.

### 3.4 Performance

- Um draw call por (textura, scissor); atlas para fontes e ícones.
- Nunca ler estado (`glGet*`, `glReadPixels`) por frame; scissor em vez de stencil.
- Buffers de vértice persistentes, índices de 16 bits, zero alocação por frame.
- Texturas customizadas (`engineLoadTXD`) chegam em DXT; a maior parte das GPUs
  mobile não tem S3TC. Transcodificar para ETC2/ASTC ao carregar (com cache em
  disco) — descomprimir para RGBA8888 multiplica a memória por 4–8.

## 4. Rede

### 4.1 O bloqueio

Ver veredito: o protocolo de fio é do `netc` fechado. O que é aberto e portável é
tudo **acima** dele: `NetBitStreamInterface`, IDs de pacote, estruturas de sync
(`Shared/sdk/net`), lógica de `CClientGame`/`CNetAPI`.

Caminhos:

| | Como | Alcance |
|---|---|---|
| **A** | Equipe do MTA fornece `netc` arm64 (ou aceita o port upstream) | Servidores existentes |
| **B** | Par de módulos de rede **abertos** (cliente + servidor) implementando a interface `CNet` sobre RakNet/SLikeNet | Servidores que instalarem o módulo; sem lista oficial nem anti-cheat |

O protótipo segue **B**, com a fronteira em `net::INetTransport`, de modo que
trocar para **A** depois seja substituir uma implementação. Vale abrir a conversa
com a equipe do MTA cedo: sem ela o projeto é um ecossistema paralelo.

Efeito colateral de B: scripts **compilados/criptografados** (`luac.mtasa.com`)
são decodificados pelo `netc`; só scripts em texto ou bytecode puro funcionam.

### 4.2 Compilar para arm64

O RakNet 4.081 aberto compila com clang atual excluindo `ReplicaManager3.cpp`
(testado em host x86-64; **não testado no NDK** — por isso o job é experimental).
SLikeNet é o fork mantido e a alternativa se o NDK reclamar.

Compatibilidade binária da camada de pacotes entre x86 Windows e arm64 Android —
todos estes são bugs silenciosos clássicos:

- `char` é **unsigned** em arm; compilar o código portado com `-fsigned-char`.
- `long` tem 8 bytes (LP64) contra 4 no Windows; `wchar_t` 4 contra 2. Serializar
  só tipos de largura fixa.
- Nunca serializar struct por `memcpy` (padding e alinhamento diferem).
- Endianness é igual (little); floats IEEE iguais, mas `-ffast-math` desligado no
  código de sync para não divergir em compressão de vetores/quaternions.

### 4.3 Latência e estabilidade em rede móvel

- **Troca Wi-Fi ↔ celular**: o IP local muda e o socket UDP morre. Registrar
  `ConnectivityManager.NetworkCallback` via JNI e reconectar com retomada de
  sessão em vez de esperar o timeout.
- **App em segundo plano**: o Android congela o processo; ao voltar, a conexão
  expirou. Desconectar de forma limpa em `onPause` prolongado.
- **MTU**: redes celulares/CGNAT ficam abaixo de 1500; limitar a ~1200 bytes.
- **Jitter**: buffer de interpolação adaptativo (o do MTA é fixo, pensado para
  banda larga) e taxa de envio de sync reduzida quando a perda sobe.
- Rede fora do thread do jogo só para I/O; **aplicar** pacotes sempre em
  `CGame::Process`.

## 5. VM Lua e memória

### 5.1 Qual VM

Recomendação: **Lua 5.1 do próprio `mtasa-blue` (`vendor/lua`)**, compilada com o
NDK. É C89 puro, portável para arm64 sem alteração, e garante comportamento
idêntico ao PC (os patches do MTA, `lua_Number` double, formato de bytecode).

LuaJIT é opcional e posterior. Custos em Android arm64:

- **Ponteiros com tag**: desde o Android 11 o heap devolve ponteiros com tag no
  byte alto (e MTE em hardware novo). O LuaJIT arm64 assume 47 bits para
  `lightuserdata` e aborta. Mitigação: `allowNativeHeapPointerTagging="false"`
  no manifesto (o `patch_apk.sh` já aplica) e/ou alocador próprio via `mmap` em
  faixa baixa.
- Bytecode incompatível com `luac` 5.1; diferenças sutis de semântica que quebram
  scripts existentes.
- O ganho é pequeno: o gargalo dos scripts de MTA são as chamadas à API do jogo,
  não a interpretação.

### 5.2 Segurança de ponteiros

O MTA já tem o desenho certo e ele deve ser preservado: scripts nunca recebem
ponteiros nativos. Elementos são `userdata` contendo um **ID** resolvido por
`CIdArray`/`CElementIDs` a cada chamada, então um elemento destruído vira `false`,
não um *use-after-free*. Regras para o port:

- IDs continuam de 32 bits; nenhum cast `ponteiro ↔ int` (comum em código antigo
  de 32 bits — compilar com `-Werror=pointer-to-int-cast` e similares).
- Uma `lua_State` por resource, toda chamada via `lua_pcall`; `lua_atpanic`
  registra e mata só o resource.
- Desabilitar `io`, `os.execute`, `package.loadlib`, `debug` perigoso, como no PC.
- HWASan nos builds de desenvolvimento (arm64 tem suporte nativo).

### 5.3 Evitar OOM

O processo já é o jogo inteiro; em aparelhos de 3–4 GB a folga é de poucas
centenas de MB antes do low-memory killer — que **mata sem exceção nem log**.

- **Teto por VM**: `lua::CappedAllocator` (já implementado e testado) faz o
  script receber `LUA_ERRMEM` em vez de derrubar o processo.
- **Orçamento global** definido a partir de `ActivityManager.getMemoryInfo()` na
  inicialização, dividido entre VMs, texturas e modelos.
- **`onTrimMemory`** (via JNI) → GC completo em todas as VMs e descarte de caches.
- Os assets são o risco real, não o heap Lua: `engineLoadDFF/TXD/COL` e
  `dxCreateTexture` precisam de orçamento próprio, falha limpa (`false`) ao
  estourar e limites de download por servidor.
- Watchdog de instruções (`lua_sethook`) contra loops infinitos, como no PC.

## 6. Roadmap de prototipagem

Cada fase tem um critério de saída observável; não avançar sem ele.

| Fase | Entrega | Critério de saída |
|---|---|---|
| **0 — Reconhecimento** | Build alvo fixada (versão + build-id); `tools/dump_symbols.sh`; projeto Ghidra; respostas aos itens **[verificar]** (símbolos, thread de render, assinatura do toque) | `dump_symbols.sh` reporta os 3 símbolos como `ok`, ou a tabela de offsets está preenchida |
| **1 — Loader** | `patch_apk.sh` + `libmtasa.so` carregando no jogo | `logcat -s MTASA` mostra "primeiro CGame::Process interceptado" e o jogo roda 10 min sem crash |
| **2 — Overlay** | `RwIm2DBackend`, texto com FreeType, toque capturado | Retângulo + texto + FPS sobre o HUD; botão na tela responde ao toque; custo < 1 ms/frame |
| **3 — Rede** | `INetTransport` sobre RakNet; servidor de eco; reconexão em troca de rede | Handshake e ping estável contra o servidor de teste em Wi-Fi e 4G |
| **4 — Lua + HUD** | Lua 5.1 embutida, `CappedAllocator`, `dxDrawText/Rectangle/Image`, `addEventHandler("onClientRender")` | Um resource de HUD simples do PC roda sem alteração |
| **5 — Entidades locais** | Fatia de `game_sa`: ped local, câmera, spawn, criação de veículo/objeto | `setElementPosition`, `createVehicle` funcionam via script |
| **6 — Sync** | Peds remotos (puresync), entrada/saída de veículo, chat | Dois aparelhos se veem andando no mesmo servidor de teste |
| **7 — Paridade** | CEGUI completo, download de resources, `engineLoad*`, áudio, binds/controles virtuais | Um gamemode simples da comunidade (ex.: `play`) jogável |
| **8 — Shaders e ecossistema** | Tradução de .fx; decisão do caminho A/B de rede com a equipe do MTA | — |

Fases 0–2 validam a base técnica e cabem em semanas. A partir da 5 o trabalho é
proporcional ao tamanho do `game_sa`: meses por fase com uma equipe pequena.

## 7. Riscos não técnicos

- **Licença**: código derivado do `mtasa-blue` torna o projeto **GPLv3**.
- **Take-Two/Rockstar**: mods de multiplayer para GTA já receberam notificações de
  remoção. Distribuir apenas código próprio (nunca APK, `.so` ou assets do jogo)
  reduz a exposição, não a elimina.
- **Marca**: "Multi Theft Auto"/"MTA" pertencem ao projeto original; um cliente
  não oficial deve ter outro nome ao ser publicado.
