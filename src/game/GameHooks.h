#pragma once
#include "mtasa/Hook.h"

namespace mtasa::game {
// Resolve símbolos e instala os hooks iniciais. Idempotente.
bool InstallHooks(const hook::ModuleInfo& gtasa);
}  // namespace mtasa::game
