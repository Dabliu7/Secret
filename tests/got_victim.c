/* Biblioteca de teste: caller() chama target() via PLT, como a libGame.so faz
 * com suas próprias funções exportadas. */
int got_victim_target(int x) { return x + 1; }
int got_victim_caller(int x) { return got_victim_target(x) * 10; }
