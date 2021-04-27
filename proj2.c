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
#define UMASK 0644
#define MAXELF 1000
#define MAXRD 20
#define SHMNAME "/myshm"
#define SEM_MUTEX "/sem_mutex"
#define SEM_SANTA "/sem_santa"
#define SEM_REINDEER "/sem_reindeer"
#define SEM_ELF "/sem_elf"
#define SEM_CHRISTMAS "/sem_christmas"
#define SEM_DONE "/sem_done"
#define SEM_HELP "/sem_help"

/* Macros */
#define ELFC args[0] /* Elf count */
#define RDC args[1] /* Reindeer count */
#define ELFT args[2] /* Elf time */
#define RDT args[3] /* Reindeer time */

/* Semaphore macros */
#define MUTEX shmptr->mutex
#define SANTA shmptr->santa
#define REINDEER shmptr->reindeer
#define ELF shmptr->elf
#define CHRISTMAS shmptr->christmas
#define DONE shmptr->done
#define HELP shmptr->help

#define OP shmptr->operation
#define WORKSHOP shmptr->workshop
#define OP_INC shmptr->operation++
#define TOTAL shmptr->total
#define RD_BACK shmptr->reindeer_back
#define RD_TOTAL shmptr->total_reindeer
#define RD_PREINC ++shmptr->reindeer_back
#define RD_HITCH_PREINC ++shmptr->reindeer_hitched
#define ELF_WAIT_PREINC ++shmptr->elves
#define ELF_WAIT_PREDEC --shmptr->elves
#define ELF_WAIT shmptr->elves

struct shm {
    sem_t *mutex; /* mutual exclusion for shm and file operations */
    sem_t *santa; /* semaphore to signal Santa for help/hitching */
    sem_t *reindeer; /* semaphore to signal Reindeer for hitching */
    sem_t *elf; /* semaphore to control elves entering the workshop */
    sem_t *christmas; /* semaphore to signal Christmas can start */
    sem_t *done; /* semaphore to signal all processes ended */
    sem_t *help; /* semaphore to signal elves they are being helped */
    uint8_t workshop; /* boolean if workshop is open */
    unsigned operation; /* operation counter */
    unsigned total; /* total number of processes */
    unsigned elves; /* number of elves waiting for workshop */
    unsigned total_reindeer; /* total number of reindeer */
    unsigned reindeer_back; /* number of reindeer return from holiday */
    unsigned reindeer_hitched; /* number of hitched reindeer */
};

void cleanup(struct shm *shmptr, FILE *file) {
    sem_close(MUTEX);
    sem_unlink(SEM_MUTEX);

    sem_close(SANTA);
    sem_unlink(SEM_SANTA);

    sem_close(REINDEER);
    sem_unlink(SEM_REINDEER);

    sem_close(ELF);
    sem_unlink(SEM_ELF);

    sem_close(CHRISTMAS);
    sem_unlink(SEM_CHRISTMAS);

    sem_close(DONE);
    sem_unlink(SEM_DONE);

    sem_close(HELP);
    sem_unlink(SEM_HELP);

    munmap(shmptr, sizeof(struct shm));
    shm_unlink(SHMNAME);

    if (file) {
        fclose(file);
    }
}

void santa(struct shm *shmptr, FILE* file) {
    /* init output in file */
    sem_wait(MUTEX);
    fprintf(file, "%d: Santa going to sleep\n", OP_INC);
    fflush(file);
    sem_post(MUTEX);

    uint8_t workshop_open = 1;
    while (workshop_open) {
        /* Santa sleeping waiting to be woken up */
        sem_wait(SANTA);
        sem_wait(MUTEX);
        if (RD_BACK == RD_TOTAL) {
            fprintf(file, "%d: Santa: closing workshop\n", OP_INC);
            fflush(file);
            workshop_open = 0;
            WORKSHOP = 0; /* Tell the leves through shared memory */
            /* Open the semaphore for each reindeer to get hitched */
            for (unsigned i = 0; i < RD_TOTAL; i++) {
                sem_post(REINDEER);
            }
        } else if (ELF_WAIT == 3) {
            fprintf(file, "%d: Santa: helping elves\n", OP_INC);
            fflush(file);
            for (uint8_t i = 0; i < 3; i++) {
                sem_post(HELP);
            }
        }
        sem_post(MUTEX);
    }

    /* When all reindeer were hitched start Christmas */
    sem_wait(CHRISTMAS);
    sem_wait(MUTEX);
    fprintf(file, "%d: Santa: Christmas started\n", OP_INC);
    fflush(file);
    sem_post(MUTEX);

    sem_wait(MUTEX);
    sem_post(DONE);
    sem_post(MUTEX);

    cleanup(shmptr, file);
}

void elf(struct shm *shmptr, FILE *file, unsigned id, unsigned elf_time) {
    /* init output in file */
    sem_wait(MUTEX);
    fprintf(file, "%d: Elf %d: started\n", OP_INC, id);
    fflush(file);
    sem_post(MUTEX);

    while (1) {
        /* Alone work */
        srand(time(NULL));
        unsigned wait_time = rand() % (elf_time+1);
        usleep(wait_time);

        /* check if workshop still open */
        sem_wait(MUTEX);
        if (!WORKSHOP) {
            break;
        }
        sem_post(MUTEX);

        sem_wait(ELF);
        sem_wait(MUTEX);
        fprintf(file, "%d: Elf %d: need help\n", OP_INC, id);
        fflush(file);
        if (ELF_WAIT_PREINC == 3) {
            sem_post(SANTA);
        } else {
            sem_post(ELF);
        }
        sem_post(MUTEX);

        sem_wait(HELP);
        sem_wait(MUTEX);
        fprintf(file, "%d: Elf %d: get help\n", OP_INC, id);
        fflush(file);
        if (ELF_WAIT_PREDEC == 0) {
            sem_post(ELF);
        }
        sem_post(MUTEX);
    }

    sem_wait(MUTEX);
    fprintf(file, "%d: Elf %d: taking holidays\n", OP_INC, id);
    fflush(file);
    sem_post(MUTEX);

    cleanup(shmptr, file);
}

void reindeer(struct shm *shmptr, FILE *file, unsigned id, unsigned rd_time) {
    /* init output in file */
    sem_wait(MUTEX);
    fprintf(file, "%d: RD %d: started.\n", OP_INC, id);
    fflush(file);
    sem_post(MUTEX);

    /* holiday wait time */
    srand(time(NULL));
    /* TODO: This is not the wait time we want */
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

    sem_wait(REINDEER);
    sem_wait(MUTEX);
    fprintf(file, "%d: RD: %d: get hitched\n", OP_INC, id);
    fflush(file);

    /* when last hitched reindeer christmas can start */
    if (RD_HITCH_PREINC == RD_TOTAL) {
        fflush(file);
        sem_post(CHRISTMAS);
    }
    sem_post(MUTEX);

    cleanup(shmptr, file);
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
    shmptr->elves = 0;
    shmptr->workshop = 1;
    shmptr->total = 1 + ELFC + RDC;
    shmptr->reindeer_back = 0;
    shmptr->reindeer_hitched = 0;
    shmptr->total_reindeer = RDC;

    FILE *file = NULL;

    /* Create semaphores */
    sem_t *mutex = sem_open(SEM_MUTEX, O_CREAT | O_EXCL, UMASK, 1);
    if (mutex == SEM_FAILED) {
        perror("sem_open");
        cleanup(shmptr, file);
        return EXIT_FAILURE;
    }
    shmptr->mutex = mutex;

    sem_t *santa_sem = sem_open(SEM_SANTA, O_CREAT | O_EXCL, UMASK, 0);
    if (santa_sem == SEM_FAILED) {
        perror("sem_open");
        cleanup(shmptr, file);
        return EXIT_FAILURE;
    }
    shmptr->santa = santa_sem;

    sem_t *reindeer_sem = sem_open(SEM_REINDEER, O_CREAT | O_EXCL, UMASK, 0);
    if (reindeer_sem == SEM_FAILED) {
        perror("sem_open");
        cleanup(shmptr, file);
        return EXIT_FAILURE;
    }
    shmptr->reindeer = reindeer_sem;

    sem_t *elf_sem = sem_open(SEM_ELF, O_CREAT | O_EXCL, UMASK, 1);
    if (elf_sem == SEM_FAILED) {
        perror("sem_open");
        cleanup(shmptr, file);
        return EXIT_FAILURE;
    }
    shmptr->elf = reindeer_sem;

    sem_t *christmas_sem = sem_open(SEM_CHRISTMAS, O_CREAT | O_EXCL, UMASK, 0);
    if (christmas_sem == SEM_FAILED) {
        perror("sem_open");
        cleanup(shmptr, file);
        return EXIT_FAILURE;
    }
    shmptr->christmas = christmas_sem;

    sem_t *done_sem = sem_open(SEM_DONE, O_CREAT | O_EXCL, UMASK, 0);
    if (done_sem == SEM_FAILED) {
        perror("sem_open");
        cleanup(shmptr, file);
        return EXIT_FAILURE;
    }
    shmptr->done = done_sem;

    sem_t *help_sem = sem_open(SEM_HELP, O_CREAT | O_EXCL, UMASK, 0);
    if (help_sem == SEM_FAILED) {
        perror("sem_open");
        cleanup(shmptr, file);
        return EXIT_FAILURE;
    }
    shmptr->help = help_sem;

    file = fopen("proj.out", "w+");
    if (file == NULL) {
        perror("fopen");
        cleanup(shmptr, file);
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

    /* TODO: This does not wait for elves */
    sem_wait(DONE);
    cleanup(shmptr, file);
    return EXIT_SUCCESS;
}
