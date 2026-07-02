#include "gerador.h"
#include "simulacao.h"
#include <stdlib.h>

/*
 * Thread do Gerador de Pacotes (uma so no sistema).
 *
 * Cria pacotes ao longo do tempo e os enfileira em estacoes de coleta
 * escolhidas ao acaso, ate atingir a meta (cfg.total_pacotes) ou a simulacao
 * ser encerrada. As estacoes sao recurso compartilhado; a insercao na fila e
 * protegida pelo mutex da estacao (ver estacao_enfileirar).
 */
void *thread_gerador(void *arg)
{
    Simulacao *sim = (Simulacao *)arg;

    /* Semente PROPRIA desta thread: rand_r e thread-safe (nao mexe no estado
     * global de rand()). O XOR so afasta a semente da usada no posicionamento
     * inicial, para os sorteios nao coincidirem. */
    unsigned int seed = sim->cfg.seed ^ 0x9e3779b9u;

    int gerados = 0;   /* contador local: unico gerador, entao dispensa trava */

    while (simulacao_ativa(sim) && gerados < sim->cfg.total_pacotes) {
        /* Escolhe uma estacao de coleta ao acaso. */
        int idx = rand_r(&seed) % sim->cfg.num_estacoes;

        /* id do pacote = numero de ordem (0, 1, 2, ...), garantidamente unico
         * porque so esta thread gera pacotes. */
        Pacote *p = pacote_criar(gerados, idx);
        if (p) {
            estacao_enfileirar(&sim->estacoes[idx], p);
            stats_inc_gerados(sim);
            gerados++;
        }

        /* Ritmo da geracao. Espalha os pacotes ao longo do tempo em vez de
         * despeja-los todos de uma vez. */
        dormir_ms(sim->cfg.intervalo_geracao_ms);
    }

    return NULL;
}
