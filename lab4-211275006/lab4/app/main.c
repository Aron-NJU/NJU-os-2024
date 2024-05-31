#include "lib.h"
#include "types.h"

void consumer(sem_t empty, sem_t mutex, sem_t full);
void producer(int id, sem_t empty, sem_t mutex, sem_t full) ;

void pro_con(int num) {
    int ret = 0;
    sem_t empty, full, mutex;

    // 初始化信号量
    sem_init(&empty, 5);
    sem_init(&full, 0);
    sem_init(&mutex, 1);

    // 创建多个子进程
    for (int i = 0; i < num; i++) {
        if (ret == 0) {
            ret = fork();
        } else if (ret > 0) {
            break;
        }
    }

    int id = get_pid();

    if (id == 1) {
        // 消费者进程
        consumer(empty, mutex, full);
    } else {
        // 生产者进程
        producer(id - 1,empty, mutex, full);
    }

    exit();
}

void consumer(sem_t empty, sem_t mutex, sem_t full) {
    for (int i = 0; i < 8; i++) {
        sem_wait(&full);      // 等待有产品可消费
        sem_wait(&mutex);     // 进入临界区

        printf("Consumer : consume\n");
        sleep(128);

        sem_post(&mutex);     // 离开临界区
        sem_post(&empty);     // 增加空闲位置
    }
}

void producer(int id, sem_t empty, sem_t mutex, sem_t full) {
    for (int i = 0; i < 2; i++) {
        sem_wait(&empty);     // 等待有空闲位置
        sem_wait(&mutex);     // 进入临界区

        printf("Producer %d: produce\n", id);
        sleep(128);

        sem_post(&mutex);     // 离开临界区
        sem_post(&full);      // 增加产品数量
    }
}

int uEntry(void) {
	// For lab4.1
	pro_con(4);
	exit();
	// Test 'scanf' 
	int dec = 0;
	int hex = 0;
	char str[6];
	char cha = 0;
	int ret = 0;
	while(1){
		printf("Input:\" Test %%c Test %%6s %%d %%x\"\n");
		ret = scanf(" Test %c Test %6s %d %x", &cha, str, &dec, &hex);
		printf("Ret: %d; %c, %s, %d, %x.\n", ret, cha, str, dec, hex);
		if (ret == 4)
			break;
	}
	
	// For lab4.2
	// Test 'Semaphore'

	int i = 4;

	sem_t sem;
	printf("Father Process: Semaphore Initializing.\n");
	ret = sem_init(&sem, 2);
	if (ret == -1) {
		printf("Father Process: Semaphore Initializing Failed.\n");
		exit();
	}

	ret = fork();
	if (ret == 0) {
		while( i != 0) {
			i --;
			printf("Child Process: Semaphore Waiting.\n");
			sem_wait(&sem);
			printf("Child Process: In Critical Area.\n");
		}
		printf("Child Process: Semaphore Destroying.\n");
		sem_destroy(&sem);
		exit();
	}
	else if (ret != -1) {
		while( i != 0) {
			i --;
			printf("Father Process: Sleeping.\n");
			sleep(128);
			printf("Father Process: Semaphore Posting.\n");
			sem_post(&sem);
		}
		printf("Father Process: Semaphore Destroying.\n");
		sem_destroy(&sem);
		//exit();
	}

	// For lab4.3
	// TODO: You need to design and test the philosopher problem.
	// Producer-Consumer problem and Reader& Writer Problem are optional.
	// Note that you can create your own functions.
	// Requirements are demonstrated in the guide.
	return 0;
}
