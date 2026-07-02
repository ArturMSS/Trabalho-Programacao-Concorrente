#include "robo_coletor.h"
#include "simulacao.h"
#include <stdlib.h>

/*
 * Thread de um Robo Coletor (existem N delas). Ciclo:
 *   1. escolher a estacao NAO-vazia mais proxima;
 *   2. deslocar-se ate ela;
 *   3. coletar um pacote (desenfileirar) -- pode falhar se outro robo levou
 *      o ultimo antes; nesse caso, volta a procurar;
 *   4. levar o pacote ate a entrada da esteira e inseri-lo, aguardando se a
 *      entrada estiver ocupada.
 * O argumento e um ArgRobo * (contem o Simulacao * e o Robo * controlado).
 */

/* Indice da estacao com pacote mais proxima do robo, ou -1 se todas vazias.
 * Usa 'quantidade' apenas como PISTA de para onde ir; a coleta em si e feita
 * por estacao_desenfileirar (atomica), que decide de verdade. */
static int escolher_estacao(Simulacao *sim, Robo *r)
{
    int melhor = -1, melhor_dist = 0;
    for (int i = 0; i < sim->cfg.num_estacoes; i++) {
        if (estacao_quantidade(&sim->estacoes[i]) <= 0)
            continue;
        int d = distancia_manhattan(r->pos, sim->estacoes[i].pos);
        if (melhor < 0 || d < melhor_dist) {
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

void *thread_coletor(void *arg)
{
    ArgRobo   *a   = (ArgRobo *)arg;
    Simulacao *sim = a->sim;
    Robo      *r   = a->robo;

    while (simulacao_ativa(sim)) {
        /* 1. Para onde ir? */
        int est = escolher_estacao(sim, r);
        if (est < 0) {                      /* nenhuma estacao tem pacote agora */
            r->estado = ROBO_OCIOSO;
            dormir_ms(sim->cfg.passo_ms);   /* espera antes de reavaliar */
            continue;
        }

        /* 2. Deslocar-se ate a estacao escolhida. */
        r->estado = ROBO_INDO;
        if (!ir_ate(sim, r, sim->estacoes[est].pos))
            break;                          /* simulacao encerrou no caminho */

        /* 3. Coletar. Operacao atomica: se veio NULL, outro robo levou o
         * ultimo pacote -- volta ao passo 1 sem drama. */
        Pacote *p = estacao_desenfileirar(&sim->estacoes[est]);
        if (!p)
            continue;
        r->carga  = p;
        r->estado = ROBO_CARREGADO;

        /* 4. Levar ate a entrada da esteira. */
        if (!ir_ate(sim, r, sim->entrada))
            break;                          /* encerrou carregando: cleanup abaixo */

        /* 5. Inserir na esteira. Se a entrada estiver ocupada, aguarda e tenta
         * de novo (o roteiro manda esperar ter espaco). */
        int inseriu = 0;
        while (simulacao_ativa(sim)) {
            if (esteira_tenta_inserir(&sim->esteira, r->carga)) {
                inseriu = 1;
                break;
            }
            dormir_ms(sim->cfg.passo_ms);
        }
        if (!inseriu)
            break;                          /* encerrou ainda carregando */

        r->carga  = NULL;                   /* pacote agora pertence a esteira */
        r->estado = ROBO_OCIOSO;
    }

    /* Encerramento: se a thread parou segurando um pacote, libera para nao
     * vazar memoria (ele nunca chegou a esteira). */
    if (r->carga) {
        free(r->carga);
        r->carga = NULL;
    }
    return NULL;
}
