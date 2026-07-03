#include "robo_entregador.h"
#include "simulacao.h"
#include <stdlib.h>

/*
 * Thread de um Robo Entregador (existem M delas). Ciclo:
 *   1. deslocar-se ate a saida da esteira (out);
 *   2. retirar um pacote da saida -- se ainda nao houver, aguarda ali (o
 *      pacote chega quando a esteira avanca);
 *   3. escolher o ponto de despacho (D) mais proximo e ir ate ele;
 *   4. entregar: o pacote sai do sistema e o contador global sobe.
 * O argumento e um ArgRobo * (contem o Simulacao * e o Robo * controlado).
 */

/* Indice do ponto de despacho mais proximo do robo. Ha sempre ao menos um
 * despacho, e qualquer um serve (nao ha fila para disputar), entao basta o
 * mais perto para reduzir o deslocamento. */
static int escolher_despacho(Simulacao *sim, Robo *r)
{
    int melhor = 0;
    int melhor_dist = distancia_manhattan(r->pos, sim->despachos[0]);
    for (int i = 1; i < sim->cfg.num_despachos; i++) {
        int d = distancia_manhattan(r->pos, sim->despachos[i]);
        if (d < melhor_dist) {
            melhor = i;
            melhor_dist = d;
        }
    }
    return melhor;
}

/* Anda passo a passo ate ficar adjacente a 'alvo'. Retorna 1 ao chegar, ou 0
 * se a simulacao foi encerrada antes. Um passo por 'tick' (cfg.passo_ms). */
static int ir_ate(Simulacao *sim, Robo *r, Vec2 alvo)
{
    while (simulacao_ativa(sim)) {
        if (robo_passo_ate(sim, r, alvo))
            return 1;                       /* chegou (adjacente ao alvo) */
        dormir_ms(sim->cfg.passo_ms);       /* andou/bloqueou: espera o tick */
    }
    return 0;
}

void *thread_entregador(void *arg)
{
    ArgRobo   *a   = (ArgRobo *)arg;
    Simulacao *sim = a->sim;
    Robo      *r   = a->robo;

    while (simulacao_ativa(sim)) {
        /* 1. Ir ate a saida da esteira. */
        r->estado = ROBO_INDO;
        if (!ir_ate(sim, r, sim->saida))
            break;                          /* simulacao encerrou no caminho */

        /* 2. Retirar um pacote da saida. Operacao atomica: se veio NULL, a
         * saida ainda esta vazia -- aguarda um tick e tenta de novo (o roteiro
         * manda esperar ter pacote para retirar). Outro entregador pode ter
         * levado antes; nesse caso simplesmente segue esperando. */
        Pacote *p = NULL;
        while (simulacao_ativa(sim)) {
            p = esteira_retirar_saida(&sim->esteira);
            if (p)
                break;
            dormir_ms(sim->cfg.passo_ms);
        }
        if (!p)
            break;                          /* encerrou sem pegar pacote */
        r->carga  = p;
        r->estado = ROBO_CARREGADO;

        /* 3. Levar ate o ponto de despacho mais proximo. */
        int desp = escolher_despacho(sim, r);
        if (!ir_ate(sim, r, sim->despachos[desp]))
            break;                          /* encerrou carregando: cleanup abaixo */

        /* 4. Entregar: o pacote deixa o sistema e o contador global de
         * entregues sobe (ao atingir a meta, encerra a simulacao). */
        free(r->carga);
        r->carga  = NULL;
        r->estado = ROBO_OCIOSO;
        stats_inc_entregues(sim);
    }

    /* Encerramento: se a thread parou segurando um pacote (retirado da esteira
     * mas ainda nao entregue), libera para nao vazar memoria. */
    if (r->carga) {
        free(r->carga);
        r->carga = NULL;
    }
    return NULL;
}
