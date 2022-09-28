#include <stdio.h>
#include <unistd.h>		// usleep
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/ipc.h>	// shared mem
#include <sys/shm.h>

#define BLOCK_SIZE 4096
#define KEY 123

struct setpoints {
   uint32_t PosSetX;
   uint32_t PosSetY;
   uint32_t PosSetZ;
};

int main(int argc, char *argv[]) {
	int shmid;
	struct setpoints *result;
	
	shmid = shmget(KEY, sizeof(struct setpoints), 0644|IPC_CREAT);

	result = shmat(shmid, NULL, 0);
	if (result == (void *) -1) {
		printf("Shared memory attach");
		return 1;
	}
	if (argc == 2) {
		result->PosSetX = 0xAFFE;
	} else {
        printf("segment contains: \"%x\"\n", result->PosSetX);
	}
}