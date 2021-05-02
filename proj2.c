/**
 * IOS Project 2 - Santa Claus Problem
 * Author: Tadeáš Vintrlík <xvintr04>
 * E-mail: xvintr04@stud.fit.vutbr.cz
 * Date: 2021-04-27
 */
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
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
#define SHMNAME "/xvintr04_shm"
#define SEM_MUTEX "/xvintr04_sem_mutex"
#define SEM_SANTA "/xvintr04_sem_santa"
#define SEM_REINDEER "/xvintr04_sem_reindeer"
#define SEM_ELF "/xvintr04_sem_elf"
#define SEM_CHRISTMAS "/xvintr04_sem_christmas"
#define SEM_DONE "/xvintr04_sem_done"
#define SEM_HELP "/xvintr04_sem_help"
#define SEM_DONE_HELPING "/xvintr04_sem_done_helping"

/* Parameter macros */
#define ELFC args[0] /* Elf count */
#define RDC args[1]  /* Reindeer count */
#define ELFT args[2] /* Elf time */
#define RDT args[3]  /* Reindeer time */

/* Semaphore macros */
#define MUTEX shmptr->mutex
#define SANTA shmptr->santa
#define REINDEER shmptr->reindeer
#define ELF shmptr->elf
#define CHRISTMAS shmptr->christmas
#define DONE shmptr->done
#define HELP shmptr->help
#define DONE_HELPING shmptr->done_help

/* Shared memory macros */
#define OP shmptr->operation
#define WORKSHOP shmptr->workshop
#define OP_INC shmptr->operation++
#define RD_BACK shmptr->reindeer_back
#define RD_TOTAL shmptr->total_reindeer
#define RD_PREINC ++shmptr->reindeer_back
#define RD_HITCH_PREINC ++shmptr->reindeer_hitched
#define ELF_WAIT_PREINC ++shmptr->elves
#define ELF_WAIT_PREDEC --shmptr->elves
#define ELF_WAIT shmptr->elves
#define ELF_TOTAL shmptr->total_elves

/* Macros */
#define SEM_KILL(semptr,name) sem_close(semptr);sem_unlink(name);

struct shm {
    sem_t *mutex; /* mutual exclusion for shm and file operations */
    sem_t *santa; /* semaphore to signal Santa for help/hitching */
    sem_t *reindeer; /* semaphore to signal Reindeer for hitching */
    sem_t *elf; /* semaphore to control elves entering the workshop */
    sem_t *christmas; /* semaphore to signal Christmas can start */
    sem_t *done; /* semaphore to signal all processes ended */
    sem_t *help; /* semaphore to signal elves they are being helped */
    sem_t *done_help; /* semaphore to signal santa he can go back to sleep */
    uint8_t workshop; /* boolean if workshop is open */
    unsigned operation; /* operation counter */
    unsigned elves; /* number of elves waiting for workshop */
    unsigned total_reindeer; /* total number of reindeer */
    unsigned total_elves; /* total number of elves */
    unsigned reindeer_back; /* number of reindeer return from holiday */
    unsigned reindeer_hitched; /* number of hitched reindeer */
};

void cleanup(struct shm *shmptr, FILE *file) {
    /* Process has finished and should open the sem */
    sem_post(DONE);

    /* Free semaphores */
    SEM_KILL(MUTEX, SEM_MUTEX);
    SEM_KILL(SANTA, SEM_SANTA);
    SEM_KILL(REINDEER, SEM_REINDEER);
    SEM_KILL(ELF, SEM_ELF);
    SEM_KILL(CHRISTMAS, SEM_CHRISTMAS);
    SEM_KILL(DONE, SEM_DONE);
    SEM_KILL(HELP, SEM_HELP);
    SEM_KILL(DONE_HELPING, SEM_DONE_HELPING);

    /* Free shared memeory */
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
            WORKSHOP = 0; /* Tell the elves through shared memory */
            /* Open all semaphores elves can be stuck on to start holiday
             * This could be a little excesive since usually there will be
             * around ELF_WAIT elves waiting, but sometimes they can get stuck
             * elsewhere, this makes sure it does not happen
             */
            for (unsigned i = 0; i <= ELF_TOTAL; i++) {
                sem_post(ELF);
                sem_post(HELP);
            }
            /* Open the semaphore for each reindeer to get hitched */
            for (unsigned i = 0; i <= RD_TOTAL; i++) {
                sem_post(REINDEER);
            }
        } else if (ELF_WAIT == 3) {
            fprintf(file, "%d: Santa: helping elves\n", OP_INC);
            fflush(file);
            for (uint8_t i = 0; i < 3; i++) {
                sem_post(HELP);
            }
            /* Wait until all the elves are helped */
            sem_post(MUTEX);
            sem_wait(DONE_HELPING);
            sem_wait(MUTEX);
            fprintf(file, "%d: Santa going to sleep\n", OP_INC);
            fflush(file);
        }
        sem_post(MUTEX);
    }

    /* When all reindeer were hitched start Christmas */
    sem_wait(CHRISTMAS);
    sem_wait(MUTEX);
    fprintf(file, "%d: Santa: Christmas started\n", OP_INC);
    fflush(file);
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
        unsigned wait_time = rand() % (elf_time+1);
        usleep(wait_time * 1000);

        /* check if workshop still open */
        if (!WORKSHOP) { break; }

        sem_wait(ELF);
        if (!WORKSHOP) { break; }
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
        if (!WORKSHOP) { break; }
        sem_wait(MUTEX);
        fprintf(file, "%d: Elf %d: get help\n", OP_INC, id);
        fflush(file);
        if (ELF_WAIT_PREDEC == 0) {
            /* tell elves they can start entering workshop again
             * and Santa he can go to sleep */
            sem_post(DONE_HELPING);
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
    unsigned wait_time = rand() % ((rd_time/2)+1) + ((rd_time/2)+1);
    usleep(wait_time * 1000);

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

uint8_t check_args(int args[REQ_ARGC-1]) {
    if (ELFC <= 0 || ELFC >= 1000) {
        fprintf(stderr, "Invalid amount of eleves.\n");
        return 0;
    } else if (RDC <= 0 || RDC >= 20) {
        fprintf(stderr, "Invalid amount of reindeer.\n");
        return 0;
    } else if (ELFT < 0 || ELFT > 1000) {
        fprintf(stderr, "Invalid time for elf.\n");
        return 0;
    } else if (RDT < 0 || RDT > 1000) {
        fprintf(stderr, "Invalid time for reindeer.\n");
        return 0;
    }
    return 1;
}

int main(int argc, char **argv) {
    struct shm *shmptr;
    int args[REQ_ARGC-1];

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

    if (!check_args(args)) {
        return EXIT_FAILURE;
    }

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
    shmptr->reindeer_back = 0;
    shmptr->reindeer_hitched = 0;
    shmptr->total_reindeer = RDC;
    shmptr->total_elves = ELFC;

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
    shmptr->elf = elf_sem;

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

    sem_t *done_helping_sem = sem_open(SEM_DONE_HELPING, O_CREAT | O_EXCL, UMASK, 0);
    if (done_helping_sem == SEM_FAILED) {
        perror("sem_open");
        cleanup(shmptr, file);
        return EXIT_FAILURE;
    }
    shmptr->done_help = done_helping_sem;

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
	for (int i = 1; i <= ELFC; i++) {
		pid = fork();
		if (pid == -1) {
			perror ("fork");
			return EXIT_FAILURE;
		} else if (pid == 0) {
            srand(i*time(NULL)); /* create rand seed from id and time */
            elf(shmptr, file, i, ELFT);
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
            srand(i*time(NULL)); /* create rand seed from id and time */
            reindeer(shmptr, file, i, RDT);
			return EXIT_SUCCESS;
		}
	}

    /* wait for all the processses to end */
    for (int i = 0; i < 1 + ELFC + RDC; i++) {
        sem_wait(DONE);
    }
    cleanup(shmptr, file);
    return EXIT_SUCCESS;
}
