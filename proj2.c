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
#define SHMNAME "/myshm"
#define SEM_MUTEX "/sem_mutex"
#define SEM_SANTA "/sem_santa"

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
    unsigned operation; /* operation counter */
    unsigned total; /* total number of processes */
    unsigned ended; /* number of finished processes */
    unsigned elves; /* number of elves waiting for workshop */
    unsigned elf_id[MAXELF]; /* "queue" of waiting elves' id */
    unsigned total_reindeer; /* total number of reindeer */
    unsigned reindeer_back; /* number of reindeer return from holiday */
    unsigned reindeer_id[MAXRD]; /* "queue" of wating reindeer's id */
};

void santa(struct shm *shmptr, FILE* file) {
    /* init output in file */
    sem_wait(MUTEX);
    fprintf(file, "%d: Santa going to sleep\n", OP_INC);
    fflush(file);
    sem_post(MUTEX);

    /* Santa sleeping waiting to be woken up */
    sem_wait(SANTA);
    sem_wait(MUTEX);
    if (RD_BACK == RD_TOTAL) {
        fprintf(file, "%d: Santa: closing workshop\n", OP_INC);
        fflush(file);
        /* TODO: hitch the reindeer */
    }
    sem_post(MUTEX);

    sem_wait(MUTEX);
    ENDED_INC;
    sem_post(MUTEX);

    fclose(file);
    sem_close(SANTA);
    sem_close(MUTEX);
}

void elf(struct shm *shmptr, FILE *file, unsigned id, unsigned time) {
    (void)time;
    /* init output in file */
    sem_wait(MUTEX);
    fprintf(file, "%d: Elf %d: started\n", OP_INC, id);
    fflush(file);
    sem_post(MUTEX);

    sem_wait(MUTEX);
    ENDED_INC;
    sem_post(MUTEX);

    fclose(file);
    sem_close(SANTA);
    sem_close(MUTEX);
}

void reindeer(struct shm *shmptr, FILE *file, unsigned id, unsigned rd_time) {
    /* init output in file */
    sem_wait(MUTEX);
    fprintf(file, "%d: RD %d: started.\n", OP_INC, id);
    fflush(file);
    sem_post(MUTEX);

    /* holiday wait time */
    srand(time(NULL));
    unsigned wait_time = ((unsigned)(rand() + rd_time/2.0)) % rd_time;
    usleep(wait_time);

    /* return from holiday */
    sem_wait(MUTEX);
    fprintf(file, "%d: RD: %d: return home\n", OP_INC, id);
    fflush(file);

    /* when last reindeer wake Santa up */
    if (RD_PREINC == RD_TOTAL) {
        sem_post(SANTA);
    }
    sem_post(MUTEX);

    sem_wait(MUTEX);
    ENDED_INC;
    sem_post(MUTEX);

    fclose(file);
    sem_close(SANTA);
    sem_close(MUTEX);
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

    /* Create shared memory */
    int fd = shm_open(SHMNAME, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        perror("shm_open");
        return EXIT_FAILURE;
    }

    if (ftruncate(fd, sizeof(struct shm))) {
        perror("ftruncate");
        shm_unlink(SHMNAME);
        return EXIT_FAILURE;
    }

    shmptr = mmap(NULL, sizeof(struct shm), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shmptr == MAP_FAILED) {
        perror("mmap");
        shm_unlink(SHMNAME);
        return EXIT_FAILURE;
    }

    shmptr->operation = 1;
    shmptr->total = 1 + ELFC + RDC;
    shmptr->ended = 0;
    shmptr->reindeer_back = 0;
    shmptr->total_reindeer = RDC;

    /* Create semaphores */
    sem_t *mutex = sem_open(SEM_MUTEX, O_CREAT | O_EXCL, UMASK, 1);
    if (mutex == SEM_FAILED) {
        perror("sem_open");
        munmap(shmptr, sizeof(struct shm));
        sem_unlink(SEM_MUTEX);
        return EXIT_FAILURE;
    }

    sem_t *santa_sem = sem_open(SEM_SANTA, O_CREAT | O_EXCL, UMASK, 0);
    if (santa_sem == SEM_FAILED) {
        perror("sem_open");
        munmap(shmptr, sizeof(struct shm));
        sem_close(mutex);
        sem_unlink(SEM_MUTEX);
        return EXIT_FAILURE;
    }

    shmptr->mutex = mutex;
    shmptr->santa = santa_sem;

    FILE *file = fopen("proj.out", "w+");
    if (file == NULL) {
        perror("fopen");
        munmap(shmptr, sizeof(struct shm));
        sem_close(santa_sem);
        sem_unlink(SEM_SANTA);
        sem_close(mutex);
        sem_unlink(SEM_MUTEX);
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
    shm_unlink(SHMNAME);

    /* Close semaphores */
    sem_close(santa_sem);
    sem_unlink(SEM_SANTA);
    sem_close(mutex);
    sem_unlink(SEM_MUTEX);

    fclose(file);
    return EXIT_SUCCESS;
}
