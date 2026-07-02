/*
 * tests/teste_fundacao.c — valida a fundacao do projeto:
 *   - config_carregar_cenario(1)
 *   - simulacao_init / simulacao_destroy
 *   - impressao do mapa via printf
 *   - mapa_tenta_ocupar, mapa_tenta_mover, mapa_liberar e colisao negada
 *
 * Compilar e executar: make teste
 */

#include <stdio.h>
#include <assert.h>
#include "config.h"
#include "simulacao.h"
#include "mapa.h"

static char celula_char(const Simulacao *sim, int x, int y)
{
    const Celula *c = &sim->mapa.celulas[y * sim->mapa.largura + x];

    if (c->ocupante != SEM_OCUPANTE) {
        int id = c->ocupante;
        return (id <= sim->cfg.num_coletores) ? 'C' : 'E';
    }

    switch (c->tipo) {
        case CELULA_LIVRE:    return '.';
        case CELULA_PAREDE:   return '#';
        case CELULA_ESTACAO:  return 'P';
        case CELULA_ENTRADA:  return 'I';
        case CELULA_SAIDA:    return 'O';
        case CELULA_DESPACHO: return 'D';
        default:              return '?';
    }
}

static void imprimir_mapa(const Simulacao *sim)
{
    const Mapa *m = &sim->mapa;
    printf("Mapa %dx%d (I=entrada esteira, O=saida, P=estacao, D=despacho,\n"
           "          C=coletor, E=entregador, #=parede, .=livre):\n\n",
           m->largura, m->altura);

    for (int y = 0; y < m->altura; y++) {
        for (int x = 0; x < m->largura; x++)
            putchar(celula_char(sim, x, y));
        putchar('\n');
    }
    putchar('\n');
}

/* Procura a primeira celula com tipo==CELULA_LIVRE e ocupante==SEM_OCUPANTE. */
static int achar_celula_livre(const Mapa *m, Vec2 *out)
{
    for (int y = 0; y < m->altura; y++)
        for (int x = 0; x < m->largura; x++) {
            const Celula *c = &m->celulas[y * m->largura + x];
            if (c->tipo == CELULA_LIVRE && c->ocupante == SEM_OCUPANTE) {
                out->x = x;
                out->y = y;
                return 1;
            }
        }
    return 0;
}

int main(void)
{
    printf("=== Teste de Fundacao ===\n\n");

    /* --- 1. Carrega cenario e inicializa simulacao --- */
    Config cfg = config_carregar_cenario(1);
    printf("Cenario: \"%s\" (%dx%d)\n"
           "  coletores=%d  entregadores=%d  estacoes=%d  despachos=%d\n"
           "  obstaculos=%d  esteira=%d  meta=%d pacotes\n\n",
           cfg.nome, cfg.largura, cfg.altura,
           cfg.num_coletores, cfg.num_entregadores,
           cfg.num_estacoes, cfg.num_despachos,
           cfg.num_obstaculos, cfg.tamanho_esteira, cfg.total_pacotes);

    Simulacao sim;
    int rc = simulacao_init(&sim, cfg);
    assert(rc == 0 && "simulacao_init deve retornar 0 em sucesso");

    imprimir_mapa(&sim);

    /* --- 2. Verifica flag rodando --- */
    assert(simulacao_ativa(&sim) && "simulacao deve estar ativa apos init");

    /* --- 3. mapa_tenta_ocupar: celula livre -> deve aceitar --- */
    Vec2 a;
    assert(achar_celula_livre(&sim.mapa, &a) && "deve existir celula livre no mapa");

    int ok = mapa_tenta_ocupar(&sim.mapa, a.x, a.y, 999);
    assert(ok == 1 && "ocupar celula livre deve retornar 1");
    printf("[OK] mapa_tenta_ocupar: celula (%d,%d) ocupada por id=999\n", a.x, a.y);

    /* --- 4. Colisao negada: mesma celula por id diferente --- */
    int colide = mapa_tenta_ocupar(&sim.mapa, a.x, a.y, 998);
    assert(colide == 0 && "ocupar celula ja ocupada deve retornar 0");
    printf("[OK] colisao negada: segunda tentativa em (%d,%d) retornou 0\n", a.x, a.y);

    /* --- 5. mapa_tenta_mover: id=999 de 'a' para outra celula livre --- */
    Vec2 b;
    assert(achar_celula_livre(&sim.mapa, &b) && "deve existir segunda celula livre");
    assert((b.x != a.x || b.y != a.y) && "celulas a e b devem ser distintas");

    int moveu = mapa_tenta_mover(&sim.mapa, a, b, 999);
    assert(moveu == 1 && "mover para celula livre deve retornar 1");

    /* Origem deve ficar livre */
    const Celula *orig = mapa_celula(&sim.mapa, a.x, a.y);
    assert(orig->ocupante == SEM_OCUPANTE && "origem deve estar livre apos mover");

    /* Destino deve ter o ocupante */
    const Celula *dest = mapa_celula(&sim.mapa, b.x, b.y);
    assert(dest->ocupante == 999 && "destino deve ter ocupante=999 apos mover");

    printf("[OK] mapa_tenta_mover: id=999 moveu de (%d,%d) para (%d,%d)\n",
           a.x, a.y, b.x, b.y);

    /* --- 6. mapa_liberar --- */
    mapa_liberar(&sim.mapa, b.x, b.y);
    assert(mapa_celula(&sim.mapa, b.x, b.y)->ocupante == SEM_OCUPANTE
           && "celula deve estar livre apos mapa_liberar");
    printf("[OK] mapa_liberar: (%d,%d) liberada\n", b.x, b.y);

    /* --- 7. simulacao_parar --- */
    simulacao_parar(&sim);
    assert(!simulacao_ativa(&sim) && "simulacao deve estar inativa apos parar");
    printf("[OK] simulacao_parar: rodando=0\n");

    /* --- Limpeza --- */
    simulacao_destroy(&sim);

    printf("\nTodos os testes passaram.\n");
    return 0;
}
