#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <mpi.h>

#define WIDTH 1024
#define HEIGHT 1024
#define DEPTH 314
#define THRESHOLD 25
#define CUBE_SIZE 4
#define BLOCK_COUNT ((WIDTH / CUBE_SIZE) * (HEIGHT / CUBE_SIZE) * (DEPTH / CUBE_SIZE))
#define HASH_SIZE 10007 // Tamaño de la tabla hash, un número primo

typedef struct HashNode {
    uint64_t blockValue;
    int count;
    struct HashNode *next;
} HashNode;

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

int main(int argc, char *argv[]) {
    FILE *file;
    uint8_t *image;
    uint8_t *metadata;
    uint64_t *blocks;  // Array para almacenar los bloques como uint64_t
    HashNode *hashTable[HASH_SIZE] = {0};
    size_t size = WIDTH * HEIGHT * DEPTH;
    struct timespec start, end;
    double time_taken;

    // Inicialización de MPI
    int rank, sizeMPI;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &sizeMPI);

    // Reservar memoria para la imagen, los metadatos y los bloques
    image = (uint8_t *)malloc(size);
    metadata = (uint8_t *)malloc(size);
    blocks = (uint64_t *)malloc(BLOCK_COUNT * sizeof(uint64_t));

    if (!image || !metadata || !blocks) {
        printf("Error: Fallo al asignar memoria\n");
        MPI_Finalize();
        return 1;
    }

    // Leer el archivo de entrada solo en el proceso raíz
    if (rank == 0) {
        file = fopen("assets/c8.raw", "rb");
        if (!file) {
            printf("Error: No se pudo abrir el archivo\n");
            free(image);
            free(metadata);
            free(blocks);
            MPI_Finalize();
            return 1;
        }

        if (fread(image, 1, size, file) != size) {
            printf("Error: No se pudo leer el archivo\n");
            fclose(file);
            free(image);
            free(metadata);
            free(blocks);
            MPI_Finalize();
            return 1;
        }

        fclose(file);

        // Aplicar umbral para procesar la imagen en datos binarios
        for (size_t i = 0; i < size; i++) {
            metadata[i] = (image[i] > THRESHOLD) ? 1 : 0;
        }
    }

    // Dividir el trabajo de extracción de bloques entre los procesos
    int blockIndex = 0;
    int blocksPerProcess = BLOCK_COUNT / sizeMPI;
    int startBlock = rank * blocksPerProcess;
    int endBlock = (rank == sizeMPI - 1) ? BLOCK_COUNT : startBlock + blocksPerProcess;

    // Extraer bloques para este proceso
    for (int pz = startBlock; pz < endBlock; pz++) {
        for (int py = 0; py <= HEIGHT - CUBE_SIZE; py += CUBE_SIZE) {
            for (int px = 0; px <= WIDTH - CUBE_SIZE; px += CUBE_SIZE) {
                size_t start = px + py * WIDTH + pz * WIDTH * HEIGHT;
                blocks[blockIndex++] = extract_block(metadata, start);
            }
        }
    }

    // Comparar bloques para encontrar duplicados en un solo bucle
    printf("\nOptimizado con hash y mpi...\n");
    clock_gettime(CLOCK_MONOTONIC, &start);

    int pairCount = 0;
    for (int i = 0; i < blockIndex; i++) {
        pairCount += insertOrUpdate(hashTable, blocks[i]);
    }

    // Realizar una reducción para combinar los resultados de todos los procesos
    int globalPairCount = 0; // Asegúrate de inicializar la variable
    MPI_Reduce(&pairCount, &globalPairCount, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);


    clock_gettime(CLOCK_MONOTONIC, &end);
    time_taken = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    if (rank == 0) {
        printf("Versión optimizada encontró %d pares de bloques duplicados\n", globalPairCount);
        printf("Tiempo tomado: %f segundos\n");
    }

    // Liberar memoria
    freeHashTable(hashTable);
    free(image);
    free(metadata);
    free(blocks);

    MPI_Finalize();
    return 0;
}
