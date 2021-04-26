#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <semaphore.h>
#include <time.h>

/* Constants */
#define REQ_ARGC 5
#define DECADIC 10

/* Macros */
#define ELFC args[0] // Elf count
#define RDC args[1] // Reindeer count
#define ELFT args[2] // Elf time
#define RDT args[3] // Reindeer time
#define UMASK 0644
#define MAXELF 1000
#define MAXRD 20

#define OP shmptr->operation
#define OP_INC shmptr->operation++
#define TOTAL shmptr->total
#define ENDED shmptr->ended
#define ENDED_INC shmptr->ended++
#define MUTEX shmptr->mutex
#define SANTA shmptr->santa
#define RD_BACK shmptr->reindeer_back
#define RD_TOTAL shmptr->total_reindeer
#define RD_PREINC ++shmptr->reindeer_back

struct shm {
    sem_t *mutex; /* mutual exclusion for shm and file operations */
    sem_t *santa; /* semaphore to signal Santa for help/hitching */
    unsigned operation;
    unsigned total;
    unsigned ended;
    unsigned elves;
    unsigned elf_id[MAXELF]; /* This is a bit wastefull */
    unsigned total_reindeer;
    unsigned reindeer_back;
    unsigned reindeer_id[MAXRD];
};

void santa(struct shm *shmptr, FILE* file) {
    sem_wait(MUTEX);
    fprintf(file, "%d: Santa goint to sleep\n", OP_INC);
    sem_post(MUTEX);

    sem_wait(SANTA);
    sem_wait(MUTEX);
    puts("santa waking up");
    if (RD_BACK == RD_TOTAL) {
        fprintf(file, "%d: Santa: closing workshop", OP_INC);
        /* TODO: hitch the reindeer */
    }
    sem_post(MUTEX);

    sem_wait(MUTEX);
    ENDED_INC;
    sem_post(MUTEX);

    fclose(file);
}

void elf(struct shm *shmptr, FILE *file, unsigned id, unsigned time) {
    (void)time;
    sem_wait(MUTEX);
    fprintf(file, "%d: Elf %d: started\n", OP_INC, id);
    sem_post(MUTEX);

    sem_wait(MUTEX);
    ENDED_INC;
    sem_post(MUTEX);

    fclose(file);
}

void reindeer(struct shm *shmptr, FILE *file, unsigned id, unsigned rd_time) {
    /* Output in file */
    sem_wait(MUTEX);
    fprintf(file, "%d: RD %d: started.\n", OP_INC, id);
    sem_post(MUTEX);

    /* Holiday wait time */
    srand(time(NULL));
    unsigned wait_time = ((unsigned)(rand() + rd_time/2.0)) % rd_time;
    usleep(wait_time);

    sem_wait(MUTEX);
    /* When last reindeer signal Santa */
    if (RD_PREINC == RD_TOTAL) {
        puts("Last reindeer, calling Santa");
        sem_post(SANTA);
    }
    sem_post(MUTEX);

    sem_wait(MUTEX);
    ENDED_INC;
    sem_post(MUTEX);

    fclose(file);
}

int main(int argc, char **argv) {
    struct shm *shmptr;
    unsigned args[REQ_ARGC-1];

    /* Parse arguments */
	if (argc != REQ_ARGC) {
		fprintf(stderr, "invalid amount of arguments given.\n"
				"./proj2 NE NR TE TR\n");
		return EXIT_FAILURE;
	}

	for (unsigned i = 1; i <= REQ_ARGC-1; i++) {
		char *endptr;
		args[i-1] = strtol(argv[i], &endptr, DECADIC);
		if (strlen(endptr)) {
			fprintf(stderr, "Invalid argument is not an integer\n");
			return EXIT_FAILURE;
		}
	}

    /* TODO: Add control if arguments are within limits */

    /* Create anonymous shared memory */
    shmptr = mmap(NULL, sizeof(struct shm), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shmptr == MAP_FAILED) {
        perror("mmap");
        return EXIT_FAILURE;
    }

    shmptr->operation = 1;
    shmptr->total = 1 + ELFC + RDC;
    shmptr->ended = 0;
    shmptr->reindeer_back = 0;
    shmptr->total_reindeer = RDC;

    /* Create semaphores */
    sem_t mutex, santa_sem;
    if (sem_init(&mutex, 1, 1) == -1) {
        perror("sem_init");
        munmap(shmptr, sizeof(struct shm));
        return EXIT_FAILURE;
    }

    if (sem_init(&santa_sem, 1, 0) == -1) {
        perror("sem_init");
        munmap(shmptr, sizeof(struct shm));
        sem_destroy(&mutex);
        return EXIT_FAILURE;
    }

    shmptr->mutex = &mutex;
    shmptr->santa = &santa_sem;

    FILE *file = fopen("proj.out", "w+");
    if (file == NULL) {
        perror("fopen");
        munmap(shmptr, sizeof(struct shm));
        sem_destroy(&mutex);
        sem_destroy(&santa_sem);
        return EXIT_FAILURE;
    }

    /* Create processes */
	pid_t pid = fork();

    /* Santa process */
	if (pid == -1) {
		perror ("fork");
		return EXIT_FAILURE;
	} else if (pid == 0) {
        santa(shmptr, file);
		return EXIT_SUCCESS;
	}

	/* Elf processes */
	for (unsigned i = 1; i <= ELFC; i++) {
		pid = fork();
		if (pid == -1) {
			perror ("fork");
			return EXIT_FAILURE;
		} else if (pid == 0) {
            elf(shmptr, file, i, ELFT);
            return EXIT_SUCCESS;
		}
	}

	/* Reindeer processes */
	for (unsigned i = 1; i <= RDC; i++) {
		pid = fork();
		if (pid == -1) {
			perror ("fork");
			return EXIT_FAILURE;
		} else if (pid == 0) {
            reindeer(shmptr, file, i, RDT);
			return EXIT_SUCCESS;
		}
	}

    uint8_t all_done = 0;
    while (!all_done) {
        /* FIXME: This is active waiting */
        sem_wait(MUTEX);
        if (ENDED == TOTAL) {
            all_done = 1;
        }
        sem_post(MUTEX);
    }

    /* Close shared memory */
    munmap(shmptr, sizeof(struct shm));

    /* Close semaphores */
    sem_destroy(&mutex);
    sem_destroy(&santa_sem);

    fclose(file);
    return EXIT_SUCCESS;
}
