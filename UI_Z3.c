#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define VM_SIZE 64					//pocet pamatovych miest
#define GEN_SIZE 256				//maximalna hodnota v pamatovom mieste
#define A_SIZE 8					//velkost adresy
#define I_SIZE 2					//velkost instrukcnej casti adresy
#define STEPS 500					//pocet krokov pri vypocte virtualneho stroja
#define T_FITNESS 1000				//hodnota, o ktoru sa navysi fitness pri najdeni pokladu
#define MUTATION_BASE 1000			//zaklad pri zadavani mutacii (generuju sa cisla od 0 po 1000) 
#define GENERATIONS 10000			//pocet generacii, po ktorych sa caka na spatnu vazbu od uzivatela

//#define POPULATION
//#define CROSSING
//#define PRINT_FITNESS

typedef struct coordinates {	//suradnice v mape
	int h;						//horizontalne, rastie smerom doprava
	int v;						//vertikalne, rastie smerom nadol
} COORDINATES;

typedef struct m {				//mapa
	int **map;					//policka v ampe
	int width;
	int height;
	int t_num;					//pocet pokladov
	COORDINATES start;			//startovacia pozicia
	COORDINATES *treasures;		//pole suradnic pokladov
} M;

typedef struct gen {			//jedinec(genom) a jeho fitness
	unsigned char genome[VM_SIZE];
	int fitness;
} GEN;

M map;
FILE *f;						//vystupny subor
FILE *p;

void merge(GEN *a, GEN *pom, int lavy, int pravy) {

	int prvy = lavy;
	int stredny = (lavy + pravy) / 2;
	int druhy = stredny + 1;
	int i = lavy;

	while (prvy <= stredny && druhy <= pravy) {
		if ((a)[prvy].fitness >= (a)[druhy].fitness) pom[i] = (a)[prvy++];
		else pom[i] = (a)[druhy++];
		i++;
	}

	while (prvy <= stredny) {
		pom[i] = (a)[prvy++];
		i++;
	}
	while (druhy <= pravy) {
		pom[i] = (a)[druhy++];
		i++;
	}
	for (i = lavy; i <= pravy; i++) (a)[i] = pom[i];

}

void merge_sort(GEN *a, GEN *pom, int lavy, int pravy) {

	if (lavy == pravy) return;
	int stredny = (lavy + pravy) / 2;
	merge_sort(a, pom, lavy, stredny);
	merge_sort(a, pom, stredny + 1, pravy);
	merge(a, pom, lavy, pravy);

}

void sort(GEN *a, int n)
{
	GEN *pom = (GEN *)malloc(n*sizeof(GEN));
	merge_sort(a, pom, 0, n - 1);
	free(pom);
}

void set_treasures() {		//nastavi hodnoty policok s pokladmi v mape na 1
	int i;
	for (i = 0; i < map.t_num; i++) {
		map.map[map.treasures[i].v][map.treasures[i].h] = 1;
	}
}

void create_map(char *name) {			//nacitanie mapy zo suboru
	FILE *f_map;
	int i, j;
	char *s = (char *)malloc(50 * sizeof(char));
	f_map = fopen(name, "r");
	if (f_map == NULL) {
		printf("Nepodarilo sa nacitat mapu!");
		exit(1);
	}
	
	fscanf(f_map, "%s %d",s, &map.width);	
	fscanf(f_map, "%s %d", s, &map.height);
		
	if (map.map != NULL) free(map.map);
	map.map = (int **)malloc(map.height*sizeof(int*));
	for (i = 0; i < map.height; i++) {
		map.map[i] = (int *)malloc(map.width*sizeof(int));
		for (j = 0; j < map.width; j++) {
			map.map[i][j] = 0;
		}
	}
	
	fscanf(f_map, "%s %d", s, &map.t_num);
	map.treasures = (COORDINATES *) malloc(map.t_num*sizeof(COORDINATES));
	for (i = 0; i < map.t_num; i++) fscanf(f_map, "%d %d", &map.treasures[i].v, &map.treasures[i].h);
	set_treasures();

	fscanf(f_map,"%s %d %d", s, &map.start.v, &map.start.h);
	map.map[map.start.v][map.start.h] = 2;
	fclose(f_map);

	fprintf(f, "Mapa: %dx%d, %d pokladov\n", map.height, map.width, map.t_num);
	for (int i = 0; i < map.height; i++) {
		for (int j = 0; j < map.width; j++) fprintf(f, "%d ", map.map[i][j]);
		fprintf(f, "\n");
	}
	
}


int vm_simulation(unsigned char *vm, int print) {		//simulacia virtualneho stroja
	int i;
	int actual_address = 0;
	int instruction, value;	
	int fitness = 0;
	int p=0;							//pomocna premenna, urcuje v specialnych pripadoch koniec cyklu
	int treasure_num = 0;				//pocet najdenych pokladov
	COORDINATES act = map.start;
	
	if (print) {
		fprintf(f, "Program:\n");
		for (i = 0; i < VM_SIZE; i++) fprintf(f, "%d ", vm[i]);
		fprintf(f, "\nCesta:\n");
	}

	for (i = 0; i < STEPS; i++) {
		instruction = (vm[actual_address] >> (A_SIZE - I_SIZE));
		value = vm[actual_address] & (VM_SIZE - 1);

		actual_address++;
		if (actual_address == VM_SIZE) actual_address = 0;
		switch (instruction) {
		case 0: vm[value]++;
			break;
		case 1: vm[value]--;
			break;
		case 2: actual_address = value;
			break;		
		case 3:									//vypis
			if (vm[value] < VM_SIZE) {
				act.v--;
				if (print) fprintf(f, "H, ");
			}
			else {
				if (vm[value] < 2 * VM_SIZE) {
					act.v++;
					if (print) fprintf(f, "D, ");
				}
				else {
					if (vm[value] < 3 * VM_SIZE) {
						act.h++;
						if (print) fprintf(f, "P, ");
					}
					else {
						act.h--;
						if (print) fprintf(f, "L, ");
					}
				}
			}
			if (act.h >= map.width || act.h < 0 || act.v >= map.height || act.v < 0) { //vysli sme z mapy
				p = 1;
				break;
			}
			if (map.map[act.v][act.h] == 1) {	//nasli sme poklad
				treasure_num++;
				fitness += T_FITNESS;
				map.map[act.v][act.h] = 0;
			}
			if (treasure_num == map.t_num) p=1;
			fitness--;		
			break;
		default: return -1;
			break;
		}
		if (p) break;
	}

	if (fitness<0) fitness = 0;
	if (print) fprintf(f, "\n");
	set_treasures();		//nastavenie pokladov v mape, pretoze ak boli najdene, boli v mape vynulovane
	return fitness;
}



void generate_genomes(GEN **population, int population_size, int start_index) {		//generovanie novych nahodnych jedincov
	int i, j;
	for (i = start_index; i < population_size; i++) {
		for (j = 0; j < VM_SIZE; j++) {
			(*population)[i].genome[j] = rand() % GEN_SIZE;
		}
	}
}

void cross_genomes(unsigned char *next, GEN *old_population, long int roulette_size) {		//krizenie
	long int left = rand() % roulette_size;		//hodnota pre laveho
	long int right = rand() % roulette_size;	//hodnota pre praveho
	int cross_index = rand() % VM_SIZE;			//index, od ktoreho sa menia geny noveho jedinca
	int left_index = 0, right_index = 0;
	long int i;
	i = 0;
	while (i <= left) {		//najdenie laveho rodica
		i += old_population[left_index].fitness;
		left_index++;
	}
	left_index--;
	i = 0;
	while (i <= right) {	//najdenie praveho rodica
		i += old_population[right_index].fitness;
		right_index++;
	}
	right_index--;

	//krizenie
	for (i = 0; i < cross_index; i++) {
		next[i] = old_population[left_index].genome[i];
	}
	for (i = cross_index; i < VM_SIZE; i++) {
		next[i] = old_population[right_index].genome[i];
	}

#ifdef CROSSING		//kontrolny vypis
	fprintf(f,"roulette: %li, left: %li, right: %li, left_index: %li, right_index: %li, cross_index: %li\n", roulette_size, left, right, left_index, right_index, cross_index);
	fprintf(f,"left (fitness %d):\n", old_population[left_index].fitness);
	for (i = 0; i < VM_SIZE; i++) {
		fprintf(f,"%d ", old_population[left_index].genome[i]);
	}
	fprintf(f,"\nright (fitness %d):\n", old_population[right_index].fitness);
	for (i = 0; i < VM_SIZE; i++) {
		fprintf(f,"%d ", old_population[right_index].genome[i]);
	}
	fprintf(f,"\nnew:\n");
	for (i = 0; i < VM_SIZE; i++) {
		fprintf(f,"%d ", next[i]);
	}
	fprintf(f, "\n\n\n");
#endif

}

unsigned char mutate(unsigned char u, int shift) {	//mutacia
	int i;
	int result = 0;
	for (i = A_SIZE - 1; i > 0; i--) {
		result = result | ((u >> i) & 1);
		if (i == shift) {
			if (result & 1) result--;
			else result++;
		}
		result <<= 1;
	}
	return result;
}

//vytvorenie novej generacie
int new_generation(GEN **population, int population_size, int elite, int cross, double mutation_probability, long long int *mutations_count) {
	
	GEN *next_population = (GEN *)malloc(population_size*sizeof(GEN));
	int i, j, k;
	long int roulette_size = 0;
	int p = 0;
	long int mutations = mutation_probability * MUTATION_BASE;
	long int m;

	//skopirovanie elity
	for (i = 0; i < elite; i++) {
		for (j = 0; j < VM_SIZE; j++) next_population[i].genome[j] = (*population)[i].genome[j];		
	}
	i = 0;
	//vypocet hodnoty v rulete 
	while ((*population)[i].fitness > 0) roulette_size += (*population)[i++].fitness;
	
	//krizenie
	if (roulette_size > 0) {
		for (i = elite; i < elite + cross; i++) {
			cross_genomes(next_population[i].genome, *population, roulette_size);
		}
		generate_genomes(&next_population, population_size, elite + cross);
	}
	else generate_genomes(&next_population, population_size, elite);

	//simulacia virtualneho stroja a vypocet fitness
	unsigned char *vm = (unsigned char *)malloc(VM_SIZE*sizeof(unsigned char));
	for (i = 0; i < population_size; i++) {
		memcpy(vm, next_population[i].genome, VM_SIZE);
		next_population[i].fitness = vm_simulation(vm,0);				
	}

	//zoradenie jedincov v novej populacii
	sort(next_population, population_size);

	//mutacie
	
	for (i = elite; i < population_size; i++) {
		for (j = 0; j < VM_SIZE; j++) {
			for (k = 0; k < A_SIZE; k++) {
				m = rand() % MUTATION_BASE;
				if (m < mutations) {
					next_population[i].genome[j] = mutate(next_population[i].genome[j], k);
					(*mutations_count)++;
				}
			}
		}

	}		

#ifdef POPULATION
	fprintf(f, "\nroulette: %d\n", roulette_size);
	for (i = 0; i < population_size; i++) {
		
		fprintf(f, "fitness: %8d,   ", next_population[i].fitness);
		for (j = 0; j < VM_SIZE; j++) {
			fprintf(f, "%d ", next_population[i].genome[j]);
		}
		fprintf(f, "\n\n");
	}
#endif

	p = next_population[0].fitness;
	free(*population);
	*population = next_population;
	
	return p;
}

//evolucia, zistovanie poctu generacii a mutacii
int evolution(GEN *population, int population_size, int elite, int cross, double mutation_probability) {
	
	int i;
	long int generations_count = 1;		
	long long mutations_count = 0;
	char c,d;
	unsigned char *vm = (unsigned char *)malloc(VM_SIZE*sizeof(unsigned char));

	do {
		i = new_generation(&population, population_size, elite, cross, mutation_probability, &mutations_count);
		generations_count++;

		//cakanie na uzivatela
		if (generations_count % GENERATIONS == 0) {
			printf("Bolo vytvorenych %ld generacii. Chcete pokracovat? (A/N)", generations_count);
		
				scanf("%c", &c);
				scanf("%c", &d);
			if (toupper(c) == 'N') {
				
				memcpy(vm, population[0].genome, VM_SIZE);
				fprintf(f, "Nájdené riešenie:\nfitness: %d\n", population[0].fitness);				
				vm_simulation(vm, 1);
				break;
			}

		}

#ifdef PRINT_FITNESS
		fprintf(p, "%d\n", population[0].fitness);
#endif

	} while (i < T_FITNESS*(map.t_num-1));

	fprintf(f, "Nájdené riešenie:\nFitness: %d\n", population[0].fitness);
	memcpy(vm, population[0].genome, VM_SIZE);
	vm_simulation(vm, 1);

	fprintf(f, "Poèet krokov: %d\n", map.t_num*T_FITNESS - i);
	fprintf(f, "Ve¾kos populácie: %d\nElita: %d%%\nKríženci: %d%%\nPravdepodobnos mutácie: %lf\n", population_size, elite, cross, mutation_probability);
	fprintf(f,"Pocet generacii: %li\n", generations_count);
	fprintf(f, "Pocet mutácií: %lli\n\n", mutations_count);

#ifdef PRINT_FITNESS
	fprintf(p, "\n\n");
#endif

	return generations_count;
}

//prva populacia, vytvori sa dvojnasobny pocet jedincov, no pokracuje len polovica
int initiate_evolution(int population_size, double elite_ratio, double cross_ratio, double mutation_probability) {
	int i;
	GEN *first_population = (GEN *)malloc(2 * population_size*sizeof(GEN));

	if (elite_ratio + cross_ratio > 100 || mutation_probability > 1) {
		printf("Neplatne vstupne hodnoty!\n");
		return 0;
	}

	generate_genomes(&first_population, 2 * population_size, 0);

	unsigned char *vm = (unsigned char *)malloc(VM_SIZE*sizeof(unsigned char));
	for (i = 0; i < 2 * population_size; i++) {
		memcpy(vm, first_population[i].genome, VM_SIZE);
		first_population[i].fitness = vm_simulation(vm,0);
	}	

	sort(first_population, 2 * population_size);
	
	return evolution(first_population, population_size, elite_ratio*population_size / 100, cross_ratio*population_size / 100, mutation_probability);
	
}

void test() {		//test s urcitymi vstupnymi parametrami
	printf("Prbieha test. Vysledky budu zapisane v subore result.txt");
	create_map("map8.txt");
	initiate_evolution(100, 2, 80, 0.01);
	create_map("map.txt");	
	initiate_evolution(100, 1, 75, 0.01);
	initiate_evolution(200, 1, 75, 0.001);
	initiate_evolution(50, 4, 75, 0.01);
	initiate_evolution(100, 5, 75, 0.01);
	initiate_evolution(100, 2, 20, 0.01);
	initiate_evolution(100, 2, 50, 0.01);
	initiate_evolution(100, 2, 80, 0.01);	
}

void test2(int pop_size, double elite) {
	char pop[50];
	sprintf(pop, "test3_%d.txt", pop_size);	
	int i, c , sum_c = 0, t = 0;

	FILE *ft = fopen(pop, "w");
	create_map("map.txt");

	for (i = 0; i < 50; i++) {
		clock_t before = clock();
		c = initiate_evolution(pop_size, elite, 75, 0.01);
		printf(".");
		clock_t difference = clock() - before;
		fprintf(ft, "%d\n", difference * 1000 / CLOCKS_PER_SEC);
		t += difference * 1000 / CLOCKS_PER_SEC;
		sum_c += c;
	}
	fprintf(ft, "Priemerny cas hladania: %d ms\nPriemerny pocet generacii: %d\n", t/50, sum_c/50);
	printf("\n");
	fclose(ft);
} //testovanie pre statistiky

void start() {			//cez stdin
	int pop_size;
	int elite_r;
	int cross_r;
	double mut;	
	char map_name[20];
	
	printf("Zadajte nazov vstupneho suboru:\n");
	scanf("%d", map_name);
	create_map(map_name);
	printf("Zadajte velkost populacie:\n");
	scanf("%d", &pop_size);
	printf("Zadajte velkost elity (v %%):\n");
	scanf("%d", &elite_r);
	printf("Zadajte pomer krizenia (v %%):\n");
	scanf("%d", &cross_r);
	printf("Zadajte pravdepodobnost mutacii (v desatinnom tvare, 3 des. miesta):\n");
	scanf("%lf", &mut);
	initiate_evolution(pop_size, elite_r, cross_r, mut);
	printf("Hotovo. Vysledky najdete v subore result.txt.\n");
}

int main() {

	srand(time(NULL));	
	f = fopen("result.txt", "w");

#ifdef PRINT_FITNESS
	p = fopen("fitness.txt", "w");
#endif

	
	//test();	
	start();	


	fclose(f);
#ifdef PRINT_FITNESS
	fclose(p);
#endif

	return 0;
}