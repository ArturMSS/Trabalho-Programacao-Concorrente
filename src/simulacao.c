#include "simulacao.h"
#include "comum.h"
#include <stdlib.h>
#include <string.h>

/* Retorna posicao aleatoria de celula LIVRE e sem ocupante (sem mutex --
 * chamado apenas durante init, fase single-thread). Fallback linear se
 * as tentativas aleatorias falharem (mapa muito cheio). */
static Vec2 celula_livre_rand(Mapa *m)
{
    Vec2 pos;
    int total = m->largura * m->altura;

    for (int t = 0; t < total * 8; t++) {
        pos.x = rand() % m->largura;
        pos.y = rand() % m->altura;
        Celula *c = &m->celulas[pos.y * m->largura + pos.x];
        if (c->tipo == CELULA_LIVRE && c->ocupante == SEM_OCUPANTE)
            return pos;
    }

    /* Fallback: varredura linear */
    for (pos.y = 0; pos.y < m->altura; pos.y++)
        for (pos.x = 0; pos.x < m->largura; pos.x++) {
            Celula *c = &m->celulas[pos.y * m->largura + pos.x];
            if (c->tipo == CELULA_LIVRE && c->ocupante == SEM_OCUPANTE)
                return pos;
        }

    pos.x = 0;
    pos.y = 0;
    return pos;
}

int simulacao_init(Simulacao *sim, Config cfg)
{
    memset(sim, 0, sizeof(*sim));
    sim->cfg = cfg;

    if (mapa_init(&sim->mapa, cfg.largura, cfg.altura) != 0)
        return -1;

    if (esteira_init(&sim->esteira, cfg.tamanho_esteira) != 0) {
        mapa_destroy(&sim->mapa);
        return -1;
    }

    /* Posicoes fixas da esteira: entrada na borda esquerda, saida na direita,
     * ambas na linha central -- ficam sempre acessiveis independentemente
     * do tamanho do mapa. */
    sim->entrada.x = 0;
    sim->entrada.y = cfg.altura / 2;
    sim->saida.x   = cfg.largura - 1;
    sim->saida.y   = cfg.altura / 2;

    sim->mapa.celulas[sim->entrada.y * cfg.largura + sim->entrada.x].tipo = CELULA_ENTRADA;
    sim->mapa.celulas[sim->saida.y   * cfg.largura + sim->saida.x  ].tipo = CELULA_SAIDA;

    /* Seed para posicionamento reproduzivel */
    srand(cfg.seed);

    /* Obstaculos */
    for (int i = 0; i < cfg.num_obstaculos; i++) {
        Vec2 p = celula_livre_rand(&sim->mapa);
        sim->mapa.celulas[p.y * cfg.largura + p.x].tipo = CELULA_PAREDE;
    }

    /* Estacoes */
    sim->estacoes = malloc((size_t)cfg.num_estacoes * sizeof(Estacao));
    if (!sim->estacoes) goto fail;

    for (int i = 0; i < cfg.num_estacoes; i++) {
        Vec2 p = celula_livre_rand(&sim->mapa);
        sim->mapa.celulas[p.y * cfg.largura + p.x].tipo = CELULA_ESTACAO;
        estacao_init(&sim->estacoes[i], p);
    }

    /* Despachos */
    sim->despachos = malloc((size_t)cfg.num_despachos * sizeof(Vec2));
    if (!sim->despachos) goto fail;

    for (int i = 0; i < cfg.num_despachos; i++) {
        Vec2 p = celula_livre_rand(&sim->mapa);
        sim->mapa.celulas[p.y * cfg.largura + p.x].tipo = CELULA_DESPACHO;
        sim->despachos[i] = p;
    }

    /* Robos coletores -- IDs 1..num_coletores */
    sim->coletores = malloc((size_t)cfg.num_coletores * sizeof(Robo));
    if (!sim->coletores) goto fail;

    for (int i = 0; i < cfg.num_coletores; i++) {
        Vec2 p = celula_livre_rand(&sim->mapa);
        sim->coletores[i].id     = i + 1;
        sim->coletores[i].tipo   = ROBO_COLETOR;
        sim->coletores[i].pos    = p;
        sim->coletores[i].estado = ROBO_OCIOSO;
        sim->coletores[i].carga  = NULL;
        sim->mapa.celulas[p.y * cfg.largura + p.x].ocupante = i + 1;
    }

    /* Robos entregadores -- IDs num_coletores+1..num_coletores+num_entregadores */
    sim->entregadores = malloc((size_t)cfg.num_entregadores * sizeof(Robo));
    if (!sim->entregadores) goto fail;

    for (int i = 0; i < cfg.num_entregadores; i++) {
        Vec2 p       = celula_livre_rand(&sim->mapa);
        int  id      = cfg.num_coletores + i + 1;
        sim->entregadores[i].id     = id;
        sim->entregadores[i].tipo   = ROBO_ENTREGADOR;
        sim->entregadores[i].pos    = p;
        sim->entregadores[i].estado = ROBO_OCIOSO;
        sim->entregadores[i].carga  = NULL;
        sim->mapa.celulas[p.y * cfg.largura + p.x].ocupante = id;
    }

    /* Estatisticas */
    if (pthread_mutex_init(&sim->stats.mutex, NULL) != 0) goto fail;
    sim->stats.pacotes_gerados  = 0;
    sim->stats.pacotes_entregues = 0;
    sim->stats.total_pacotes    = cfg.total_pacotes;
    sim->stats.inicio           = agora_seg();

    sim->rodando = 1;
    return 0;

fail:
    simulacao_destroy(sim);
    return -1;
}

void simulacao_destroy(Simulacao *sim)
{
    if (sim->estacoes) {
        for (int i = 0; i < sim->cfg.num_estacoes; i++)
            estacao_destroy(&sim->estacoes[i]);
        free(sim->estacoes);
        sim->estacoes = NULL;
    }

    free(sim->despachos);
    sim->despachos = NULL;

    free(sim->coletores);
    sim->coletores = NULL;

    free(sim->entregadores);
    sim->entregadores = NULL;

    esteira_destroy(&sim->esteira);
    mapa_destroy(&sim->mapa);

    pthread_mutex_destroy(&sim->stats.mutex);
}

int simulacao_ativa(Simulacao *sim)
{
    return sim->rodando != 0;
}

void simulacao_parar(Simulacao *sim)
{
    sim->rodando = 0;
}

/* --- Estatisticas (secoes criticas) --- */

void stats_inc_gerados(Simulacao *sim)
{
    pthread_mutex_lock(&sim->stats.mutex);
    sim->stats.pacotes_gerados++;
    pthread_mutex_unlock(&sim->stats.mutex);
}

void stats_inc_entregues(Simulacao *sim)
{
    pthread_mutex_lock(&sim->stats.mutex);
    sim->stats.pacotes_entregues++;
    int atingiu = (sim->stats.pacotes_entregues >= sim->stats.total_pacotes);
    pthread_mutex_unlock(&sim->stats.mutex);

    if (atingiu)
        simulacao_parar(sim);
}

int stats_pacotes_entregues(Simulacao *sim)
{
    pthread_mutex_lock(&sim->stats.mutex);
    int n = sim->stats.pacotes_entregues;
    pthread_mutex_unlock(&sim->stats.mutex);
    return n;
}

int simulacao_pacotes_aguardando(Simulacao *sim)
{
    int total = 0;
    for (int i = 0; i < sim->cfg.num_estacoes; i++)
        total += estacao_quantidade(&sim->estacoes[i]);
    return total;
}
