#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <semaphore.h>

/* Constants */
#define SHMNAME "santamem"
#define REQ_ARGC 5
#define DECADIC 10

/* Macros */
#define ELFC args[0] // Elf count
#define RDC args[1] // Reindeer count
#define ELFT args[2] // Elf time
#define RDT args[3] // Reindeer time
#define UMASK 0644
#define OP shmptr->operation
#define OP_INC shmptr->operation++
#define TOTAL shmptr->total
#define ENDED shmptr->ended
#define ENDED_INC shmptr->ended++
#define MUTEX shmptr->mutex

struct shm {
    sem_t *mutex;
    unsigned operation;
    unsigned total;
    unsigned ended;
};

int main(int argc, char **argv) {
    int fd;
    struct shm *shmptr;
    int args[REQ_ARGC-1];

    /* Parse arguments */
	if (argc != REQ_ARGC) {
		fprintf(stderr, "invalid amount of arguments given.\n"
				"./proj2 NE NR TE TR\n");
		return EXIT_FAILURE;
	}

	for (int i = 1; i <= REQ_ARGC-1; i++) {
		char *endptr;
		args[i-1] = strtol(argv[i], &endptr, DECADIC);
		if (strlen(endptr)) {
			fprintf(stderr, "Invalid argument is not an integer\n");
			return EXIT_FAILURE;
		}
	}

    /* Create shared memory */
    fd = shm_open(SHMNAME, O_CREAT | O_RDWR, UMASK);
    if (fd < 0) {
        perror("shm_open");
        shm_unlink(SHMNAME);
        return EXIT_FAILURE;
    }

    shmptr = mmap(NULL, sizeof(struct shm), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shmptr == MAP_FAILED) {
        perror("mmap");
        close(fd);
        shm_unlink(SHMNAME);
        return EXIT_FAILURE;
    }
    ftruncate(fd, sizeof(struct shm));

    shmptr->operation = 0;
    shmptr->total = 1 + ELFC + RDC;
    shmptr->ended = 0;

    /* Create semaphore */
    sem_t mutex;
    if (sem_init(&mutex, 1, 1) == -1) {
        perror("sem_init");
        close(fd);
        shm_unlink(SHMNAME);
        return EXIT_FAILURE;
    }

    shmptr->mutex = &mutex;

    FILE *file = fopen("proj.out", "w+");
    if (file == NULL) {
        perror("fopen");
        close(fd);
        shm_unlink(SHMNAME);
        sem_destroy(&mutex);
        return EXIT_FAILURE;
    }

    /* Create processes */
	pid_t pid = fork();

    /* Santa process */
	if (pid == -1) {
		perror ("fork");
		return EXIT_FAILURE;
	} else if (pid == 0) {
        sem_wait(MUTEX);
        fprintf(file, "%d: Santa\n", OP_INC);
        sem_post(MUTEX);

        sem_wait(MUTEX);
        ENDED_INC;
        sem_post(MUTEX);

        fclose(file);
		return EXIT_SUCCESS;
	}

	/* Elf processes */
	for (int i = 1; i <= ELFC; i++) {
		pid = fork();
		if (pid == -1) {
			perror ("fork");
			return EXIT_FAILURE;
		} else if (pid == 0) {
            sem_wait(MUTEX);
            fprintf(file, "%d: Elf\n", OP_INC);
            sem_post(MUTEX);

            sem_wait(MUTEX);
            ENDED_INC;
            sem_post(MUTEX);

            fclose(file);
            return EXIT_SUCCESS;
		}
	}

	/* Reindeer processes */
	for (int i = 1; i <= RDC; i++) {
		pid = fork();
		if (pid == -1) {
			perror ("fork");
			return EXIT_FAILURE;
		} else if (pid == 0) {
            sem_wait(MUTEX);
            fprintf(file, "%d: Reindeer\n", OP_INC);
            sem_post(MUTEX);

            sem_wait(MUTEX);
            ENDED_INC;
            sem_post(MUTEX);

            fclose(file);
			return EXIT_SUCCESS;
		}
	}

    uint8_t all_done = 0;
    while (!all_done) {
        sem_wait(MUTEX);
        if (ENDED == TOTAL) {
            all_done = 1;
        }
        sem_post(MUTEX);
    }

    /* Close shared memory */
    shm_unlink(SHMNAME);

    /* Close semaphore */
    sem_destroy(&mutex);

    fclose(file);
    return EXIT_SUCCESS;
}
