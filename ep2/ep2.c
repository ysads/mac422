#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define debug(...) if (DEBUG) { fprintf(stderr, __VA_ARGS__); }

#define MAX_CICLISTAS 10
#define MAX_CICLISTAS_LARGADA 5
#define FALSE 0
#define TRUE 1
#define INTERVAL_120MS 120000
#define INTERVAL_60MS  60000
#define INTERVAL_40MS  40000
#define INTERVAL_20MS  20000
#define INTERVAL_1MS   1000

int DEBUG = 1;

typedef struct info_ciclista {
    int id;
    int velocidade;
    int quebrado;
    int i;
    int j;
    int volta_atual;
    double tempo_gasto;
    pthread_t thread;
} ciclista_t;

typedef struct info_posicao {
    pthread_mutex_t mutex;
    ciclista_t* ciclista;
    int i;
    int j;
} posicao_t;

typedef struct info_simulacao {
    int d;
    int n;
    int ciclistas_restantes;
    posicao_t*** pista;
    ciclista_t** ciclistas;
    pthread_mutex_t mutex_ciclistas;
} simulacao_t;


/**
 * Variável global que guarda o estado atual da simulação.
 */
simulacao_t* simulacao;


/* ============================================================ */
/*                       FUNÇÕES P/ DEBUG                       */
/* ============================================================ */

void print_ciclistas() {
    int i;
    ciclista_t* ciclista;

    printf("\n");
    for (i = 0; i < simulacao->n; i++) {
        ciclista = simulacao->ciclistas[i];
        if (ciclista != NULL) {
            debug("%2d: %0.2lfms @ (%d, %d)\n", ciclista->id, ciclista->tempo_gasto, ciclista->i, ciclista->j);
        }
    }
}


void print_pista() {
    int i, j;
    posicao_t*** pista;

    pista = simulacao->pista;

    printf("\n");
    for (i = 0; i < simulacao->d; i++) {
        fprintf(stderr, "%2d: \t", (i + 1));
        for (j = 0; j < MAX_CICLISTAS; j++) {
            if (pista[i][j] != NULL && pista[i][j]->ciclista != NULL) {
                fprintf(stderr, "%2d \t", pista[i][j]->ciclista->id);
            } else {
                fprintf(stderr, " -  \t");
            }
        }
        fprintf(stderr, "\n");
    }
}


/**
 * Define quanto tempo deve passar antes entre duas mudanças de posição
 * consecutivas.
 */
int intervalo(ciclista_t* ciclista) {
    switch (ciclista->velocidade) {
        case 30:
            return INTERVAL_120MS;
            break;

        case 60:
            return INTERVAL_60MS;
            break;
    
        case 90:
            return INTERVAL_40MS;
            break;
    }
    debug("Veloc. inválida: %d\n", ciclista->velocidade);
    exit(1);
}


/**
 * Decide para onde mover um ciclista com base no estado atual dele, decidindo inclusive
 * se há a possibilidade de ultrapassar alguém à frente.
 */
posicao_t* proxima_posicao(ciclista_t* ciclista) {
    int prox_i, prox_j;

    prox_i = (ciclista->i + 1) % simulacao->d;
    prox_j = ciclista->j;      // aqui deve vir a decisão de ultrapassagem

    return simulacao->pista[prox_i][prox_j];
}


void mover_ciclista(ciclista_t* ciclista) {
    int prox_posicao_primeiro, mudou_volta;
    posicao_t *prox_posicao, *posicao_atual;

    posicao_atual = simulacao->pista[ciclista->i][ciclista->j];
    prox_posicao = proxima_posicao(ciclista);

    /**
     * Define a ordem com que o código vai tentar atualizar as posições, com o objetivo
     * de evitar o problema dos filósofos famintos.
     */
    prox_posicao_primeiro = rand() % 2;

    // debug("moving %d – %c\n", ciclista->id, (prox_posicao_primeiro ? 'P' : 'A'));

    if (prox_posicao_primeiro) {
        pthread_mutex_lock(&prox_posicao->mutex);
        pthread_mutex_lock(&posicao_atual->mutex);
    } else {
        pthread_mutex_lock(&posicao_atual->mutex);
        pthread_mutex_lock(&prox_posicao->mutex);
    }

    mudou_volta = (prox_posicao->i == 0 && posicao_atual->i == simulacao->d - 1) ? TRUE : FALSE;

    if (prox_posicao->ciclista == NULL) {
        debug("%d\t", ciclista->id);
        debug("put %d\t", prox_posicao->i);
        debug("clear %d\n", posicao_atual->i);
        prox_posicao->ciclista = ciclista;
        posicao_atual->ciclista = NULL;
        ciclista->i = prox_posicao->i;
        ciclista->j = prox_posicao->j;
    }

    /**
     * Libera o acesso às posições atualizadas.
     */
    pthread_mutex_unlock(&prox_posicao->mutex);
    pthread_mutex_unlock(&posicao_atual->mutex);

    if (mudou_volta) {
        ciclista->volta_atual++;
        debug("%d => volta %d!\n", ciclista->id, ciclista->volta_atual);
    }
}


/**
 * Remove um ciclista da pista e registra o momento em que ele finalizou a prova.
 */
void remover_ciclista(ciclista_t* ciclista) {
    posicao_t* posicao;

    posicao = simulacao->pista[ciclista->i][ciclista->j];

    pthread_mutex_lock(&simulacao->mutex_ciclistas);
    pthread_mutex_lock(&posicao->mutex);

    posicao->ciclista = NULL;
    simulacao->ciclistas_restantes--;

    pthread_mutex_unlock(&simulacao->mutex_ciclistas);
    pthread_mutex_unlock(&posicao->mutex);
}


/**
 * Organiza o comportamento de um ciclista dentro da corrida, ie, definindo quando – e se –
 * há mudança de velocidade, de posição, de status, etc.
 */
void* simular_ciclista(void* args) {
    ciclista_t* ciclista = (ciclista_t*) args;
    int tempo_gasto, tempo_espera;

    /**
     * Espera até que o simulador autorize essa linha de ciclistas a se mover.
     */
    // pthread_cond_wait();
    tempo_gasto = 0;

    while (ciclista->volta_atual == 1) {
        tempo_espera = intervalo(ciclista);

        mover_ciclista(ciclista);
        usleep(tempo_espera);
        tempo_gasto += tempo_espera;
    }

    remover_ciclista(ciclista);
    ciclista->tempo_gasto = tempo_gasto / INTERVAL_1MS;

    return NULL;
}


/**
 * Inicializa um ciclista de acordo com as especificações de como eles deveriam estar
 * no começo da primeira volta: a 30km/h e não-ciclistas.
 */
ciclista_t* init_ciclista(int id, int i, int j) {
    ciclista_t* ciclista;

    ciclista = (ciclista_t*) malloc(sizeof(ciclista_t));

    ciclista->id = id;
    ciclista->quebrado = FALSE;
    ciclista->velocidade = 30;
    ciclista->i = i;
    ciclista->j = j;
    ciclista->volta_atual = 1;

    pthread_create(&ciclista->thread, NULL, simular_ciclista, ciclista);
    
    return ciclista;
}


/**
 * Inicializa uma posição vazia da pista, incluindo o mutex que controla o
 * seu acesso.
 */
posicao_t* init_posicao(int i, int j) {
    posicao_t* posicao;

    posicao = malloc(sizeof(posicao_t));
    posicao->ciclista = NULL;
    posicao->i = i;
    posicao->j = j;
    pthread_mutex_init(&posicao->mutex, NULL);

    return posicao;
}


/**
 * Aloca o espaço necessário para o vetor de pista, incluindo o vetor auxiliar de
 * cada posição responsável por armazenar os ciclistas ali presentes naquele momento.
 */
posicao_t*** init_pista(int d) {
    int i, j;
    posicao_t*** pista;

    pista = (posicao_t***) malloc(d * sizeof(posicao_t**));

    for (i = 0; i < d; i++) {
        pista[i] = (posicao_t**) malloc(MAX_CICLISTAS * sizeof(posicao_t*));
        for (j = 0; j < MAX_CICLISTAS; j++) {
            pista[i][j] = init_posicao(i, j);
        }
    }
    return pista;
}


/**
 * Inicializa a simulação, definindo os parâmetros globais que devem coordenar os
 * ciclistas.
 */
simulacao_t* init_simulacao(int d, int n) {
    simulacao_t* sim;

    sim = (simulacao_t*) malloc(sizeof(simulacao_t));
    
    sim->ciclistas = (ciclista_t**) malloc(n * sizeof(ciclista_t*));
    sim->pista = init_pista(d);
    sim->d = d;
    sim->n = n;
    sim->ciclistas_restantes = n;

    pthread_mutex_init(&sim->mutex_ciclistas, NULL);

    int i;
    for (i = 0; i < n; i++) {
        sim->ciclistas[i] = NULL;
    }

    return sim;
}


/**
 * Percorre todas as posições de memória alocadas dinamicamente e as libera.
 */
void free_simulacao(simulacao_t* simulacao, int d) {
    int i, j;
    posicao_t*** pista;

    pista = simulacao->pista;

    for (i = 0; i < d; i++) {
        for (j = 0; j < MAX_CICLISTAS; j++) {
            if (pista[i][j]->ciclista != NULL) {   // remover quando o ep estiver pronto, pq no final não vai haver ciclista na pista
                free(pista[i][j]->ciclista);
            }
            pthread_mutex_destroy(&pista[i][j]->mutex);
            free(pista[i][j]);
        }
        free(pista[i]);
    }
    pthread_mutex_destroy(&simulacao->mutex_ciclistas);
    free(simulacao->pista);
    free(simulacao->ciclistas);
    free(simulacao);
}


/**
 * Dá a largada na simulação, alocando cada ciclista de maneira aleatória em
 * alguma das posições iniciais da pista. Note que cada ciclista aqui é identificado
 * por um inteiro entre 0 e n-1 (inclusive).
 */
void dar_largada(posicao_t*** pista, int d, int n) {
    int i, j, id_ciclista, n_ciclistas;
    int* ids;
    ciclista_t* ciclista;

    n_ciclistas = n;
    ids = (int*) calloc(n, sizeof(int));

    /**
     * Define quantas posições do vetor de pista vão ser necessárias para acomodar
     * o número de ciclistas na largada.
     */
    if (n % MAX_CICLISTAS_LARGADA) {
        i = ceil(n / MAX_CICLISTAS_LARGADA);
    } else {
        i = n / MAX_CICLISTAS_LARGADA - 1;
    }

    j = 0;
    while (n_ciclistas > 0) {
        id_ciclista = rand() % n;

        /**
         * Se um ciclista com o ID sorteado ainda não foi colocado na corrida,
         * aloca suas informações e o adiciona na próxima posição vazia da pista.
         */
        if (!ids[id_ciclista]) {
            ids[id_ciclista] = TRUE;
            ciclista = init_ciclista(id_ciclista, i, j);

            simulacao->pista[i][j]->ciclista = ciclista;
            simulacao->ciclistas[n - n_ciclistas] = ciclista;

            n_ciclistas--;
            j++;

            /**
             * Se já colocamos o máximo de ciclistas nessa posição, vamos pra próxima.
             */
            if (j == MAX_CICLISTAS_LARGADA) {
                j = 0;
                i--;
            }
        }
    }
    free(ids);
}


int main(int argc, char* argv[]) {
    int n, d;
    
    if (argc < 3) {
        fprintf(stderr, "Uso: ./ep2 <d> <n> [debug]\n");
        return 1;
    }
    d = atoi(argv[1]);
    n = atoi(argv[2]);

    /**
     * Define a semente do gerador de números aleatórios.
     */
    srand(time(NULL));

    simulacao = init_simulacao(d, n);
    dar_largada(simulacao->pista, d, n);

    while (simulacao->ciclistas_restantes > 0) {
        print_pista();
        usleep(INTERVAL_60MS);
    }

    print_ciclistas();

    /**
     * Aguarda até que todas as threads tenha finalizado sua simulação.
     */
    for (int i = 0; i < n; i++) {
        pthread_join(simulacao->ciclistas[i]->thread, NULL);
    }

    free_simulacao(simulacao, d);

    return 0;
}
