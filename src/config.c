#include "config.h"
#include <string.h>
#include <stdio.h>

Config config_carregar_cenario(int n)
{
    Config c;
    memset(&c, 0, sizeof(c));

    switch (n) {
        default:
        case 1:
            snprintf(c.nome, sizeof(c.nome), "Simples");
            c.largura              = 18;
            c.altura               = 12;
            c.num_coletores        = 2;
            c.num_entregadores     = 1;
            c.num_estacoes         = 3;
            c.num_despachos        = 2;
            c.num_obstaculos       = 4;
            c.tamanho_esteira      = 6;
            c.total_pacotes        = 20;
            c.intervalo_geracao_ms = 1200;
            c.passo_ms             = 300;
            c.seed                 = 42u;
            break;

        case 2:
            snprintf(c.nome, sizeof(c.nome), "Intermediario");
            c.largura              = 24;
            c.altura               = 16;
            c.num_coletores        = 3;
            c.num_entregadores     = 2;
            c.num_estacoes         = 4;
            c.num_despachos        = 3;
            c.num_obstaculos       = 12;
            c.tamanho_esteira      = 8;
            c.total_pacotes        = 40;
            c.intervalo_geracao_ms = 900;
            c.passo_ms             = 200;
            c.seed                 = 137u;
            break;

        case 3:
            snprintf(c.nome, sizeof(c.nome), "Alta concorrencia");
            c.largura              = 32;
            c.altura               = 20;
            c.num_coletores        = 5;
            c.num_entregadores     = 4;
            c.num_estacoes         = 6;
            c.num_despachos        = 4;
            c.num_obstaculos       = 24;
            c.tamanho_esteira      = 12;
            c.total_pacotes        = 80;
            c.intervalo_geracao_ms = 600;
            c.passo_ms             = 150;
            c.seed                 = 271u;
            break;
    }

    return c;
}

int config_carregar_dinamico(Config *cfg)
{
    /* TODO (opcional): ler parametros do usuario em tempo de execucao. */
    (void)cfg;
    return 0;
}
