#pragma once
#include "mtasa/Hook.h"

namespace mtasa::game {

// Endereços resolvidos da libGTASA.so. A resolução é feita por símbolo exportado
// (a build Android mantém a maior parte dos nomes C++ na .dynsym); para funções
// não exportadas, cai na tabela de offsets por build (ver Symbols.cpp).
struct Symbols {
    void* CGame_Process = nullptr;   // tick de simulação, 1x por frame
    void* Render2dStuff = nullptr;   // ponto de desenho 2D (HUD), depois do mundo 3D
    void* AND_TouchEvent = nullptr;  // entrada de toque vinda do Java
};

// Resolve um símbolo (nome mangled) ou retorna nullptr.
void* Resolve(const hook::ModuleInfo& module, const char* mangled_name);

// Preenche a tabela. Retorna quantos símbolos obrigatórios faltaram.
int ResolveAll(const hook::ModuleInfo& module, Symbols* out);

}  // namespace mtasa::game
