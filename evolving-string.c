#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <unistd.h>  // For sleep function

#define MAX_POPULATION 1000
#define TOURNAMENT_SIZE 5

struct individual {
    char *genes;
    float fitness;
};

struct population {
    float mutationRate;
    char *target;
    struct individual *individuals;
    int size;
    int generation;  // Added to track generations
    float best_fitness;  // Track best fitness
    char *best_genes;    // Track best solution
};


// Function to calculate fitness of an individual
float calculate_fitness(const char *individual, const char *target) {
    int len = strlen(target);
    int correct = 0;
    
    for (int i = 0; i < len; i++) {
        if (individual[i] == target[i]) {
            correct++;
        }
    }
    
    return (float)correct / len;
}

// Function to create a random character in the desired range
char random_char() {
    // Generate random printable ASCII characters (from space to ~)
    return (char)(32 + rand() % (127 - 32));
}

// Function to create a random individual
char* create_individual(int length) {
    char* individual = malloc(sizeof(char) * (length + 1));
    
    for (int i = 0; i < length; i++) {
        individual[i] = random_char();
    }
    individual[length] = '\0';  // Null terminate the string
    
    return individual;
}

// Function to perform tournament selection
int tournament_select(struct population *pop) {
    int best_index = rand() % pop->size;
    float best_fitness = pop->individuals[best_index].fitness;
    
    for (int i = 0; i < TOURNAMENT_SIZE - 1; i++) {
        int index = rand() % pop->size;
        if (pop->individuals[index].fitness > best_fitness) {
            best_index = index;
            best_fitness = pop->individuals[index].fitness;
        }
    }
    return best_index;
}

// Function to create a child from two parents
char* crossover(const char *parent1, const char *parent2, int length) {
    char *child = malloc(sizeof(char) * (length + 1));
    int midpoint = rand() % length;
    
    for (int i = 0; i < length; i++) {
        if (i < midpoint) {
            child[i] = parent1[i];
        } else {
            child[i] = parent2[i];
        }
    }
    child[length] = '\0';
    return child;
}

// Function to mutate an individual
void mutate(char *individual, float mutation_rate, int length) {
    for (int i = 0; i < length; i++) {
        if ((float)rand() / RAND_MAX < mutation_rate) {
            individual[i] = random_char();
        }
    }
}

// Function to evolve the population
void evolve(struct population *pop) {
    struct individual *new_population = malloc(sizeof(struct individual) * pop->size);
    
    // Keep track of best individual
    int best_index = 0;
    for (int i = 1; i < pop->size; i++) {
        if (pop->individuals[i].fitness > pop->individuals[best_index].fitness) {
            best_index = i;
        }
    }
    
    // Update population's best if necessary
    if (pop->individuals[best_index].fitness > pop->best_fitness) {
        pop->best_fitness = pop->individuals[best_index].fitness;
        strcpy(pop->best_genes, pop->individuals[best_index].genes);
    }
    
    // Elitism: keep the best individual
    new_population[0].genes = strdup(pop->individuals[best_index].genes);
    new_population[0].fitness = pop->individuals[best_index].fitness;
    
    // Create new population
    for (int i = 1; i < pop->size; i++) {
        // Select parents
        int parent1_idx = tournament_select(pop);
        int parent2_idx = tournament_select(pop);
        
        // Create child
        new_population[i].genes = crossover(
            pop->individuals[parent1_idx].genes,
            pop->individuals[parent2_idx].genes,
            strlen(pop->target)
        );
        
        // Mutate child
        mutate(new_population[i].genes, pop->mutationRate, strlen(pop->target));
        
        // Calculate fitness
        new_population[i].fitness = calculate_fitness(new_population[i].genes, pop->target);
    }
    
    // Free old population and update with new one
    for (int i = 0; i < pop->size; i++) {
        free(pop->individuals[i].genes);
    }
    free(pop->individuals);
    pop->individuals = new_population;
    pop->generation++;
}

// Modified create_population function to initialize new fields
struct population* create_population(const char *target_str, int population_size) {
    struct population *pop = malloc(sizeof(struct population));
    pop->mutationRate = 0.01;
    pop->target = strdup(target_str);
    pop->size = population_size;
    pop->generation = 0;
    pop->best_fitness = 0.0;
    pop->best_genes = malloc(sizeof(char) * (strlen(target_str) + 1));
    pop->best_genes[0] = '\0';
    
    pop->individuals = malloc(sizeof(struct individual) * population_size);
    int target_length = strlen(target_str);
    
    for (int i = 0; i < population_size; i++) {
        pop->individuals[i].genes = create_individual(target_length);
        pop->individuals[i].fitness = calculate_fitness(pop->individuals[i].genes, target_str);
    }
    
    return pop;
}

// Modified free_population function
void free_population(struct population *pop) {
    for (int i = 0; i < pop->size; i++) {
        free(pop->individuals[i].genes);
    }
    free(pop->individuals);
    free(pop->target);
    free(pop->best_genes);
    free(pop);
}

// Clear screen function (ANSI escape codes)
void clear_screen() {
    printf("\033[2J\033[H");
}

int main() {
    srand(time(NULL));
    const char *target = "Fuck da Police from the street to the underground";
    int population_size = 100;
    struct population *pop = create_population(target, population_size);
    
    while (pop->best_fitness < 1.0) {
        clear_screen();
        // Display current status
        printf("\033[1;36m=== Genetic Algorithm Status ===\033[0m\n");
        printf("Target: %s\n", pop->target);
        printf("Generation: %d\n", pop->generation);
        printf("Best Fitness: %.2f\n", pop->best_fitness);
        printf("Best Solution: %s\n\n", pop->best_genes);
        
        printf("\033[1;33mCurrent Population Sample:\033[0m\n");
        for (int i = 0; i < 10; i++) {
            printf("Individual %d: %s (fitness: %.2f)\n",
                   i + 1,
                   pop->individuals[i].genes,
                   pop->individuals[i].fitness);
        }
        
        evolve(pop);
        usleep(70000);  // Sleep for 100ms to make the evolution visible
    }    
    // Display final result
    clear_screen();
    printf("\033[1;32m=== Solution Found! ===\033[0m\n");
    printf("Target: %s\n", pop->target);
    printf("Solution: %s\n", pop->best_genes);
    printf("Generations: %d\n", pop->generation);    
    free_population(pop);
    return 0;
}

#include <stdlib.h>
#include <time.h>

// Global or per-session tracking
static int *pick_counts = NULL;
static int total_picks = 0;

// Initialize (call once per collection)
void init_pick_tracking(int n) {
    pick_counts = calloc(n, sizeof(int));
    total_picks = 0;
    srand(time(NULL));
}

// Enforced uniform selection
n_Folder *select_uniform(n_Folder **folders, int n) {
    if (!pick_counts) init_pick_tracking(n);

    // Find the item with the lowest pick count
    int min_count = INT_MAX;
    int candidate = -1;
    for (int i = 0; i < n; i++) {
        if (pick_counts[i] < min_count) {
            min_count = pick_counts[i];
            candidate = i;
        }
    }

    // If multiple have the same low count, pick randomly among them
    int *low_candidates = malloc(n * sizeof(int));
    int low_count = 0;
    for (int i = 0; i < n; i++) {
        if (pick_counts[i] == min_count) {
            low_candidates[low_count++] = i;
        }
    }

    int selected_index = low_candidates[rand() % low_count];
    free(low_candidates);

    pick_counts[selected_index]++;
    total_picks++;

    return folders[selected_index];
}

// Reset tracking if needed
void reset_tracking() {
    if (pick_counts) free(pick_counts);
    pick_counts = NULL;
    total_picks = 0;
}