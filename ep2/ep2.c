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
#define INTERVAL_60MS 60000
#define INTERVAL_20MS 20000

int DEBUG = 1;

typedef struct info_ciclista {
    int id;
    int velocidade;
    int quebrado;
    pthread_t* thread;
} ciclista_t;

typedef struct info_posicao {
    pthread_mutex_t* mutex;
    ciclista_t* ciclista;
} posicao_t;

typedef struct info_simulacao {
    int d;
    int n;
    posicao_t*** pista;
    ciclista_t** quebrados;
} simulacao_t;


/**
 * Inicializa um ciclista de acordo com as especificações de como eles deveriam estar
 * no começo da primeira volta: a 30km/h e não-quebrados.
 */
ciclista_t* init_ciclista(int id, int posicao_inicial) {
    ciclista_t* info_ciclista;

    info_ciclista = malloc(sizeof(info_ciclista));

    info_ciclista->id = id;
    info_ciclista->quebrado = FALSE;
    info_ciclista->velocidade = 30;
    info_ciclista->thread = NULL;
    
    return info_ciclista;
}


/**
 * Inicializa uma posição vazia da pista, incluindo o mutex que controla o
 * seu acesso.
 */
posicao_t* init_posicao() {
    posicao_t* posicao;

    posicao = malloc(sizeof(posicao_t));
    posicao->ciclista = NULL;
    posicao->mutex = NULL;
    // pthread_mutex_init(posicao->mutex, NULL);

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
            pista[i][j] = init_posicao();
        }
    }
    return pista;
}


simulacao_t* init_simulacao(int d, int n) {
    simulacao_t* simulacao;

    simulacao = (simulacao_t*) malloc(sizeof(simulacao_t));
    
    simulacao->quebrados = (ciclista_t**) malloc(n * sizeof(ciclista_t*));
    simulacao->pista = init_pista(d);
    simulacao->d = d;
    simulacao->n = n;

    return simulacao;
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
            free(pista[i][j]);
        }
        free(pista[i]);
    }
    free(simulacao->pista);
    free(simulacao->quebrados);
    free(simulacao);
}


/**
 * Organiza o comportamento de um ciclista dentro da corrida, ie, definindo quando – e se –
 * há mudança de velocidade, de posição, de status, etc.
 */
void* ciclista(void* args) {
    // while () {
        // se dá pra ultrapassar pela esquerda
            // ultrapassa
        // atualiza posição
    // }

    return NULL;
}


/**
 * Dá a largada na simulação, alocando cada ciclista de maneira aleatória em
 * alguma das posições iniciais da pista. Note que cada ciclista aqui é identificado
 * por um inteiro entre 0 e n-1 (inclusive).
 */
void dar_largada(posicao_t*** pista, int d, int n) {
    int i, j, id_ciclista, posicao_pista;
    int* ids;
    ciclista_t* info_ciclista;

    i = n;
    ids = (int*) calloc(n, sizeof(int));

    /**
     * Define quantas posições do vetor de pista vão ser necessárias para acomodar
     * o número de ciclistas na largada.
     */
    if (n % MAX_CICLISTAS_LARGADA) {
        posicao_pista = ceil(n / MAX_CICLISTAS_LARGADA);
    } else {
        posicao_pista = n / MAX_CICLISTAS_LARGADA - 1;
    }

    j = 0;
    while (i > 0) {
        id_ciclista = rand() % n;

        /**
         * Se um ciclista com o ID sorteado ainda não foi colocado na corrida,
         * aloca suas informações e o adiciona na próxima posição vazia da pista.
         */
        if (!ids[id_ciclista]) {
            ids[id_ciclista] = TRUE;
            info_ciclista = init_ciclista(id_ciclista, i);
            pista[posicao_pista][j]->ciclista = info_ciclista;

            i--;
            j++;

            /**
             * Se já colocamos o máximo de ciclistas nessa posição, vamos pra próxima.
             */
            if (j == MAX_CICLISTAS_LARGADA) {
                j = 0;
                posicao_pista--;
            }
        }
    }
    free(ids);
}


void print_pista(posicao_t*** pista, int d) {
    int i, j;

    for (i = 0; i < d; i++) {
        fprintf(stderr, "pos %2d: \t", (i + 1));
        for (j = 0; j < MAX_CICLISTAS; j++) {
            if (pista[i][j] != NULL && pista[i][j]->ciclista != NULL) {
                fprintf(stderr, "%02d \t", pista[i][j]->ciclista->velocidade);
            } else {
                fprintf(stderr, "  -  \t");
            }
        }
        fprintf(stderr, "\n");
    }
}


int main(int argc, char* argv[]) {
    int n, d;
    simulacao_t* simulacao;
    
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

    free_simulacao(simulacao, d);

    return 0;
}
