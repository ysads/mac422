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
#define INTERVAL_60MS 60000
#define INTERVAL_40MS 40000
#define INTERVAL_20MS 20000

int DEBUG = 1;

typedef struct info_ciclista {
    int id;
    int velocidade;
    int quebrado;
    int i;
    int j;
    int volta_atual;
    pthread_t thread;
} ciclista_t;

typedef struct info_posicao {
    pthread_mutex_t mutex;
    ciclista_t* ciclista;
} posicao_t;

typedef struct info_simulacao {
    int d;
    int n;
    posicao_t*** pista;
    ciclista_t** ciclistas;
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
            debug("%2d: %d km/h @ (%d, %d)\n", ciclista->id, ciclista->velocidade, ciclista->i, ciclista->j);
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
 * Tenta atualizar a posição de um ciclista, fazendo lock no mutex responsável
 * pela posição de destino. Também altera o ciclista para que ele tenha as
 * coordenadas da nova posição e a referência correta para a volta atual dele.
 */
void atualizar_posicao(ciclista_t* ciclista, int prox_i, int prox_j) {
    posicao_t *prox_posicao, *posicao_atual;

    posicao_atual = simulacao->pista[ciclista->i][ciclista->j];
    prox_posicao = simulacao->pista[prox_i][prox_j];
    
    pthread_mutex_lock(&prox_posicao->mutex);   // Possível problema dos filósofos famintos aqui!
    pthread_mutex_lock(&posicao_atual->mutex);  // Não está claro em que ordem deve haver o lock
    
    posicao_atual->ciclista = NULL;
    prox_posicao->ciclista = ciclista;
    
    ciclista->i = prox_i;
    ciclista->j = prox_j;
    
    pthread_mutex_unlock(&posicao_atual->mutex);
    pthread_mutex_unlock(&prox_posicao->mutex);

    if (prox_i == 0) {
        ciclista->volta_atual++;
        debug("%d => volta %d!\n", ciclista->id, ciclista->volta_atual);
    }
}


/**
 * Organiza o comportamento de um ciclista dentro da corrida, ie, definindo quando – e se –
 * há mudança de velocidade, de posição, de status, etc.
 */
void* simular_ciclista(void* args) {
    int prox_i, prox_j;
    ciclista_t* ciclista = (ciclista_t*) args;

    /**
     * Espera até que o simulador autorize essa linha de ciclistas a se mover.
     */
    // pthread_cond_wait();

    while (ciclista->volta_atual == 1) {
        prox_i = (ciclista->i + 1) % simulacao->d;
        prox_j = ciclista->j;      // aqui deve vir a decisão de ultrapassagem

        atualizar_posicao(ciclista, prox_i, prox_j);
        usleep(intervalo(ciclista));
    }

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
posicao_t* init_posicao() {
    posicao_t* posicao;

    posicao = malloc(sizeof(posicao_t));
    posicao->ciclista = NULL;
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
            pista[i][j] = init_posicao();
        }
    }
    return pista;
}


simulacao_t* init_simulacao(int d, int n) {
    simulacao_t* sim;

    sim = (simulacao_t*) malloc(sizeof(simulacao_t));
    
    sim->ciclistas = (ciclista_t**) malloc(n * sizeof(ciclista_t*));
    sim->pista = init_pista(d);
    sim->d = d;
    sim->n = n;

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
    
    print_ciclistas();

    // pthread_create(&simulacao->ciclistas[0]->thread, NULL, simular_ciclista, simulacao->ciclistas[0]);
    for (int i = 0; i < 20; i++) {
        print_pista();
        usleep(INTERVAL_60MS);
    }
    // pthread_join(simulacao->ciclistas[0]->thread, NULL);

    /**
     * Aguarda até que todas as threads tenha finalizado sua simulação.
     */
    for (int i = 0; i < n; i++) {
        pthread_join(simulacao->ciclistas[i]->thread, NULL);
    }

    free_simulacao(simulacao, d);

    return 0;
}
