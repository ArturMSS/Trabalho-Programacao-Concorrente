#include "esteira.h"
#include "simulacao.h"
#include "comum.h"
#include <stdlib.h>

/*
 * Esteira transportadora: buffer LIMITADO de posicoes discretas, recurso
 * compartilhado classico (produtor/consumidor). posicoes[0] e a ENTRADA (in),
 * onde os robos coletores inserem; posicoes[tamanho-1] e a SAIDA (out), de
 * onde os robos entregadores retiram. A thread da esteira faz os pacotes
 * avancarem em direcao a saida. Todo acesso ao vetor passa pelo mutex.
 */

int esteira_init(Esteira *e, int tamanho)
{
    e->tamanho  = tamanho;
    e->ocupadas = 0;
    /* calloc zera tudo: NULL representa posicao vazia. */
    e->posicoes = calloc((size_t)tamanho, sizeof(Pacote *));
    if (!e->posicoes)
        return -1;

    if (pthread_mutex_init(&e->mutex, NULL) != 0) {
        free(e->posicoes);
        e->posicoes = NULL;
        return -1;
    }
    return 0;
}

void esteira_destroy(Esteira *e)
{
    if (e->posicoes) {
        /* Libera os pacotes que ainda estavam na esteira ao encerrar. */
        for (int i = 0; i < e->tamanho; i++)
            free(e->posicoes[i]);
        free(e->posicoes);
        e->posicoes = NULL;
    }
    pthread_mutex_destroy(&e->mutex);
}

int esteira_tenta_inserir(Esteira *e, Pacote *p)
{
    pthread_mutex_lock(&e->mutex);
    int ok = 0;
    if (e->posicoes[0] == NULL) {   /* entrada livre: aceita o pacote */
        e->posicoes[0] = p;
        e->ocupadas++;
        ok = 1;
    }
    pthread_mutex_unlock(&e->mutex);
    return ok;                      /* 0 => entrada ocupada, coletor aguarda */
}

void esteira_avancar(Esteira *e)
{
    pthread_mutex_lock(&e->mutex);
    /* Percorre da saida para a entrada. Processar de tras para frente garante
     * que cada pacote avance no maximo UMA posicao por tick (nao "arrasta" o
     * mesmo pacote varias casas). A saida (ultima posicao) nunca e esvaziada
     * aqui: o pacote la permanece ate um entregador o retirar. */
    for (int i = e->tamanho - 1; i > 0; i--) {
        if (e->posicoes[i] == NULL && e->posicoes[i - 1] != NULL) {
            e->posicoes[i]     = e->posicoes[i - 1];
            e->posicoes[i - 1] = NULL;
        }
    }
    pthread_mutex_unlock(&e->mutex);
}

Pacote *esteira_retirar_saida(Esteira *e)
{
    pthread_mutex_lock(&e->mutex);
    Pacote *p = e->posicoes[e->tamanho - 1];
    if (p) {
        e->posicoes[e->tamanho - 1] = NULL;
        e->ocupadas--;
    }
    pthread_mutex_unlock(&e->mutex);
    return p;                       /* NULL => saida vazia, entregador aguarda */
}

int esteira_ocupadas(Esteira *e)
{
    pthread_mutex_lock(&e->mutex);
    int n = e->ocupadas;
    pthread_mutex_unlock(&e->mutex);
    return n;
}

void *thread_esteira(void *arg)
{
    Simulacao *sim = (Simulacao *)arg;

    /* Enquanto a simulacao estiver ativa, faz os pacotes avancarem uma posicao
     * a cada "tick" da simulacao (cfg.passo_ms). */
    while (simulacao_ativa(sim)) {
        esteira_avancar(&sim->esteira);
        dormir_ms(sim->cfg.passo_ms);
    }
    return NULL;
}
