#include "estacao.h"
#include <stdlib.h>

int estacao_init(Estacao *est, Vec2 pos)
{
    est->pos = pos;
    est->cabeca = NULL;
    est->cauda = NULL;
    est->quantidade = 0;
    if (pthread_mutex_init(&est->mutex, NULL) != 0)
        return -1;
    return 0;
}

void estacao_destroy(Estacao *est)
{
    /* Libera os nos restantes e os pacotes que nunca chegaram a ser coletados. */
    NoPacote *no = est->cabeca;
    while (no) {
        NoPacote *prox = no->prox;
        free(no->pacote);
        free(no);
        no = prox;
    }
    est->cabeca = NULL;
    est->cauda = NULL;
    est->quantidade = 0;
    pthread_mutex_destroy(&est->mutex);
}

void estacao_enfileirar(Estacao *est, Pacote *p)
{
    /* Aloca o no FORA da secao critica para mante-la curta. */
    NoPacote *no = malloc(sizeof(NoPacote));
    if (!no) return;            /* sem memoria: pacote descartado */
    no->pacote = p;
    no->prox   = NULL;

    pthread_mutex_lock(&est->mutex);
    if (est->cauda)
        est->cauda->prox = no;  /* fila nao-vazia: encadeia no fim */
    else
        est->cabeca = no;       /* fila vazia: vira tambem a cabeca */
    est->cauda = no;
    est->quantidade++;
    pthread_mutex_unlock(&est->mutex);
}

Pacote *estacao_desenfileirar(Estacao *est)
{
    pthread_mutex_lock(&est->mutex);
    NoPacote *no = est->cabeca;
    if (!no) {                  /* fila vazia */
        pthread_mutex_unlock(&est->mutex);
        return NULL;
    }
    est->cabeca = no->prox;
    if (!est->cabeca)           /* removeu o ultimo: zera a cauda */
        est->cauda = NULL;
    est->quantidade--;
    pthread_mutex_unlock(&est->mutex);

    /* free() do no fora da secao critica; devolve so o pacote ao chamador. */
    Pacote *p = no->pacote;
    free(no);
    return p;
}

int estacao_quantidade(Estacao *est)
{
    pthread_mutex_lock(&est->mutex);
    int q = est->quantidade;
    pthread_mutex_unlock(&est->mutex);
    return q;
}
