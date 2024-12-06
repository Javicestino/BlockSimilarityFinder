#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <pthread.h>

#define WIDTH 1024
#define HEIGHT 1024
#define DEPTH 314
#define THRESHOLD 25
#define CUBE_SIZE 4
#define BLOCK_COUNT ((WIDTH / CUBE_SIZE) * (HEIGHT / CUBE_SIZE) * (DEPTH / CUBE_SIZE))
#define HASH_SIZE 10007 // Tamaño de la tabla hash, un número primo
#define NUM_THREADS 4 // Número de hilos para paralelizar

typedef struct HashNode {
    uint64_t blockValue;
    int count;
    struct HashNode *next;
} HashNode;

// Mutex para proteger la tabla de hash
pthread_mutex_t hashMutex = PTHREAD_MUTEX_INITIALIZER;

// Función para crear un nodo hash
HashNode *createNode(uint64_t blockValue) {
    HashNode *node = (HashNode *)malloc(sizeof(HashNode));
    if (!node) {
        fprintf(stderr, "Error al asignar memoria para el nodo hash\n");
        exit(EXIT_FAILURE);
    }
    node->blockValue = blockValue;
    node->count = 1;
    node->next = NULL;
    return node;
}

// Función hash simple
int hashFunction(uint64_t value) {
    return value % HASH_SIZE;
}

// Insertar o actualizar la tabla hash
int insertOrUpdate(HashNode **hashTable, uint64_t blockValue) {
    int hashIndex = hashFunction(blockValue);
    HashNode *current = hashTable[hashIndex];

    // Buscar si el valor ya existe
    while (current) {
        if (current->blockValue == blockValue) {
            current->count++;
            return current->count - 1; // Devuelve cuántos pares previos encontró
        }
        current = current->next;
    }

    // Si no existe, crear un nuevo nodo
    HashNode *newNode = createNode(blockValue);
    newNode->next = hashTable[hashIndex];
    hashTable[hashIndex] = newNode;

    return 0; // No había pares previos
}

// Liberar memoria de la tabla hash
void freeHashTable(HashNode **hashTable) {
    for (int i = 0; i < HASH_SIZE; i++) {
        HashNode *current = hashTable[i];
        while (current) {
            HashNode *temp = current;
            current = current->next;
            free(temp);
        }
    }
}

// Extraer un bloque como uint64_t
uint64_t extract_block(uint8_t *data, size_t start) {
    uint64_t block = 0;
    int bit = 0;

    for (int z = 0; z < CUBE_SIZE; z++) {
        for (int y = 0; y < CUBE_SIZE; y++) {
            for (int x = 0; x < CUBE_SIZE; x++) {
                size_t offset = start + x + y * WIDTH + z * WIDTH * HEIGHT;
                if (data[offset]) {
                    block |= (1ULL << bit);
                }
                bit++;
            }
        }
    }
    return block;
}

typedef struct ThreadData {
    uint64_t *blocks;
    int startIndex;
    int endIndex;
    HashNode **hashTable;
    int *pairCount;
} ThreadData;

// Función que ejecuta cada hilo
void *processBlocks(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    for (int i = data->startIndex; i < data->endIndex; i++) {
        pthread_mutex_lock(&hashMutex);  // Bloqueo de mutex para acceder de forma segura
        *data->pairCount += insertOrUpdate(data->hashTable, data->blocks[i]);
        pthread_mutex_unlock(&hashMutex);  // Liberación del mutex
    }
    return NULL;
}

int main() {
    FILE *file;
    uint8_t *image;
    uint8_t *metadata;
    uint64_t *blocks;  // Array para almacenar los bloques como uint64_t
    HashNode *hashTable[HASH_SIZE] = {0};
    size_t size = WIDTH * HEIGHT * DEPTH;
    struct timespec start, end;
    double time_taken;

    // Reservar memoria para la imagen, los metadatos y los bloques
    image = (uint8_t *)malloc(size);
    metadata = (uint8_t *)malloc(size);
    blocks = (uint64_t *)malloc(BLOCK_COUNT * sizeof(uint64_t));

    if (!image || !metadata || !blocks) {
        printf("Error: Fallo al asignar memoria\n");
        return 1;
    }

    // Leer el archivo de entrada
    file = fopen("assets/c8.raw", "rb");
    if (!file) {
        printf("Error: No se pudo abrir el archivo\n");
        free(image);
        free(metadata);
        free(blocks);
        return 1;
    }

    if (fread(image, 1, size, file) != size) {
        printf("Error: No se pudo leer el archivo\n");
        fclose(file);
        free(image);
        free(metadata);
        free(blocks);
        return 1;
    }

    fclose(file);

    // Aplicar umbral para procesar la imagen en datos binarios
    for (size_t i = 0; i < size; i++) {
        metadata[i] = (image[i] > THRESHOLD) ? 1 : 0;
    }

    // Extraer todos los bloques y almacenarlos en un array lineal
    int blockIndex = 0;
    for (int pz = 0; pz <= DEPTH - CUBE_SIZE; pz += CUBE_SIZE) {
        for (int py = 0; py <= HEIGHT - CUBE_SIZE; py += CUBE_SIZE) {
            for (int px = 0; px <= WIDTH - CUBE_SIZE; px += CUBE_SIZE) {
                size_t start = px + py * WIDTH + pz * WIDTH * HEIGHT;
                blocks[blockIndex++] = extract_block(metadata, start);
            }
        }
    }

    // Crear y ejecutar los hilos
    printf("\nOptimizado con pthreads...\n");
    clock_gettime(CLOCK_MONOTONIC, &start);

    pthread_t threads[NUM_THREADS];
    ThreadData threadData[NUM_THREADS];
    int pairCount = 0;

    int blockPerThread = BLOCK_COUNT / NUM_THREADS;

    // Dividir el trabajo entre los hilos
    for (int i = 0; i < NUM_THREADS; i++) {
        threadData[i].blocks = blocks;
        threadData[i].startIndex = i * blockPerThread;
        threadData[i].endIndex = (i == NUM_THREADS - 1) ? BLOCK_COUNT : (i + 1) * blockPerThread;
        threadData[i].hashTable = hashTable;
        threadData[i].pairCount = &pairCount;

        pthread_create(&threads[i], NULL, processBlocks, (void *)&threadData[i]);
    }

    // Esperar que todos los hilos terminen
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    time_taken = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Versión con pthreads encontró %d pares de bloques duplicados\n", pairCount);
    printf("Tiempo tomado: %f segundos\n", time_taken);

    // Liberar memoria
    freeHashTable(hashTable);
    free(image);
    free(metadata);
    free(blocks);
    return 0;
}
