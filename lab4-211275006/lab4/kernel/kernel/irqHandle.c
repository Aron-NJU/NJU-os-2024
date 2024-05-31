#include "x86.h"
#include "device.h"

#define SYS_WRITE 0
#define SYS_READ 1
#define SYS_FORK 2
#define SYS_EXEC 3
#define SYS_SLEEP 4
#define SYS_EXIT 5
#define SYS_SEM 6
#define SYS_PID 7

#define STD_OUT 0
#define STD_IN 1

#define SEM_INIT 0
#define SEM_WAIT 1
#define SEM_POST 2
#define SEM_DESTROY 3

extern TSS tss;

extern ProcessTable pcb[MAX_PCB_NUM];
extern int current;

extern Semaphore sem[MAX_SEM_NUM];
extern Device dev[MAX_DEV_NUM];

extern int displayRow;
extern int displayCol;

extern uint32_t keyBuffer[MAX_KEYBUFFER_SIZE];
extern int bufferHead;
extern int bufferTail;

void GProtectFaultHandle(struct StackFrame *sf);
void timerHandle(struct StackFrame *sf);
void keyboardHandle(struct StackFrame *sf);
void syscallHandle(struct StackFrame *sf);

void syscallWrite(struct StackFrame *sf);
void syscallRead(struct StackFrame *sf);
void syscallFork(struct StackFrame *sf);
void syscallExec(struct StackFrame *sf);
void syscallSleep(struct StackFrame *sf);
void syscallExit(struct StackFrame *sf);
void syscallSem(struct StackFrame *sf);
void syscallPid(struct StackFrame *sf);

void syscallWriteStdOut(struct StackFrame *sf);

void syscallReadStdIn(struct StackFrame *sf);

void syscallSemInit(struct StackFrame *sf);
void syscallSemWait(struct StackFrame *sf);
void syscallSemPost(struct StackFrame *sf);
void syscallSemDestroy(struct StackFrame *sf);
void triggerScheduler() {
    // 使用内联汇编触发调度中断
    asm volatile("int $0x20");
}

void irqHandle(struct StackFrame *sf) { // pointer sf = esp
	/* Reassign segment register */
	asm volatile("movw %%ax, %%ds"::"a"(KSEL(SEG_KDATA)));
	/* Save esp to stackTop */
	uint32_t tmpStackTop = pcb[current].stackTop;
	pcb[current].prevStackTop = pcb[current].stackTop;
	pcb[current].stackTop = (uint32_t)sf;

	switch(sf->irq) {
		case -1:
			break;
		case 0xd:
			GProtectFaultHandle(sf);
			break;
		case 0x20:
			timerHandle(sf);
			break;
		case 0x21:
			keyboardHandle(sf);
			break;
		case 0x80:
			syscallHandle(sf);
			break;
		default:assert(0);
	}
	/* Recover stackTop */
	pcb[current].stackTop = tmpStackTop;
}

void GProtectFaultHandle(struct StackFrame *sf) {
	assert(0);
	return;
}

void timerHandle(struct StackFrame *sf) {
	int i;
	uint32_t tmpStackTop;
	i = (current+1) % MAX_PCB_NUM;
	while (i != current) {
		if (pcb[i].state == STATE_BLOCKED && pcb[i].sleepTime != -1) {
			pcb[i].sleepTime --;
			if (pcb[i].sleepTime == 0)
				pcb[i].state = STATE_RUNNABLE;
		}
		i = (i+1) % MAX_PCB_NUM;
	}

	if (pcb[current].state == STATE_RUNNING &&
		pcb[current].timeCount != MAX_TIME_COUNT) {
		pcb[current].timeCount++;
		return;
	}
	else {
		if (pcb[current].state == STATE_RUNNING) {
			pcb[current].state = STATE_RUNNABLE;
			pcb[current].timeCount = 0;
		}
		
		i = (current+1) % MAX_PCB_NUM;
		while (i != current) {
			if (i !=0 && pcb[i].state == STATE_RUNNABLE)
				break;
			i = (i+1) % MAX_PCB_NUM;
		}
		if (pcb[i].state != STATE_RUNNABLE)
			i = 0;
		current = i;
		/* echo pid of selected process */
		//putChar('0'+current);
		pcb[current].state = STATE_RUNNING;
		pcb[current].timeCount = 1;
		/* recover stackTop of selected process */
		tmpStackTop = pcb[current].stackTop;
		pcb[current].stackTop = pcb[current].prevStackTop;
		tss.esp0 = (uint32_t)&(pcb[current].stackTop); // setting tss for user process
		asm volatile("movl %0, %%esp"::"m"(tmpStackTop)); // switch kernel stack
		asm volatile("popl %gs");
		asm volatile("popl %fs");
		asm volatile("popl %es");
		asm volatile("popl %ds");
		asm volatile("popal");
		asm volatile("addl $8, %esp");
		asm volatile("iret");
	}
}

void keyboardHandle(struct StackFrame *sf) {
	ProcessTable *pt = NULL;
	uint32_t keyCode = getKeyCode();
	if (keyCode == 0) // illegal keyCode
		return;
	//putChar(getChar(keyCode));
	keyBuffer[bufferTail] = keyCode;
	bufferTail=(bufferTail+1)%MAX_KEYBUFFER_SIZE;

	if (dev[STD_IN].value < 0) { // with process blocked
		// TODO: deal with blocked situation
 		// 增加设备值，表示有一个进程被唤醒
        dev[STD_IN].value++;

        // 获取阻塞在标准输入设备上的进程表指针
        pt = (ProcessTable *)((uint32_t)(dev[STD_IN].pcb.prev) - (uint32_t)&(((ProcessTable *)0)->blocked));
        
        // 将该进程的状态设置为可运行
        pt->state = STATE_RUNNABLE;
        pt->sleepTime = 0;

        // 从设备队列中移除该进程-最后一个节点
		// 维护双向链表
		dev[STD_IN].pcb.prev = (dev[STD_IN].pcb.prev)->prev;
		(dev[STD_IN].pcb.prev)->next = &(dev[STD_IN].pcb);
	}

	return;
}

void syscallHandle(struct StackFrame *sf) {
	switch(sf->eax) { // syscall number
		case SYS_WRITE:
			syscallWrite(sf);
			break; // for SYS_WRITE
		case SYS_READ:
			syscallRead(sf);
			break; // for SYS_READ
		case SYS_FORK:
			syscallFork(sf);
			break; // for SYS_FORK
		case SYS_EXEC:
			syscallExec(sf);
			break; // for SYS_EXEC
		case SYS_SLEEP:
			syscallSleep(sf);
			break; // for SYS_SLEEP
		case SYS_EXIT:
			syscallExit(sf);
			break; // for SYS_EXIT
		case SYS_SEM:
			syscallSem(sf);
			break; // for SYS_SEM
		case SYS_PID:
			syscallPid(sf);
		default:break;
	}
}

void syscallWrite(struct StackFrame *sf) {
	switch(sf->ecx) { // file descriptor
		case STD_OUT:
			if (dev[STD_OUT].state == 1)
				syscallWriteStdOut(sf);
			break; // for STD_OUT
		default:break;
	}
}

void syscallWriteStdOut(struct StackFrame *sf) {
	int sel = sf->ds; // segment selector for user data, need further modification
	char *str = (char*)sf->edx;
	int size = sf->ebx;
	int i = 0;
	int pos = 0;
	char character = 0;
	uint16_t data = 0;
	asm volatile("movw %0, %%es"::"m"(sel));
	for (i = 0; i < size; i++) {
		asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str+i));
		if(character == '\n') {
			displayRow++;
			displayCol=0;
			if(displayRow==MAX_ROW){
				displayRow=MAX_ROW-1;
				displayCol=0;
				scrollScreen();
			}
		}
		else {
			data = character | (0x0c << 8);
			pos = (MAX_COL*displayRow+displayCol)*2;
			asm volatile("movw %0, (%1)"::"r"(data),"r"(pos+0xb8000));
			displayCol++;
			if(displayCol==MAX_COL){
				displayRow++;
				displayCol=0;
				if(displayRow==MAX_ROW){
					displayRow=MAX_ROW-1;
					displayCol=0;
					scrollScreen();
				}
			}
		}
	}
	
	updateCursor(displayRow, displayCol);
	return;
}

void syscallRead(struct StackFrame *sf) {
	switch(sf->ecx) {
		case STD_IN:
			if (dev[STD_IN].state == 1)
				syscallReadStdIn(sf);
			break; // for STD_IN
		default:
			break;
	}
}

void syscallReadStdIn(struct StackFrame *sf) {
	// TODO: complete `stdin`
	if (dev[STD_IN].value < 0) {
        pcb[current].regs.eax = -1;
    } 
    // 如果标准输入设备的值为 0，表示当前没有数据，需要阻塞当前进程
    else if (dev[STD_IN].value == 0) {
        dev[STD_IN].value--;  // 减少标准输入设备的值

        // 将当前进程加入到标准输入设备的阻塞队列中
        pcb[current].blocked.next = dev[STD_IN].pcb.next;
        pcb[current].blocked.prev = &(dev[STD_IN].pcb);
        dev[STD_IN].pcb.next = &(pcb[current].blocked);
        (pcb[current].blocked.next)->prev = &(pcb[current].blocked);

        // 将当前进程状态设置为阻塞，并设置睡眠时间为 -1（表示无限期睡眠）
        pcb[current].state = STATE_BLOCKED;
        pcb[current].sleepTime = -1;

        // 触发调度程序进行进程切换
        asm volatile("int $0x20");

        // 执行进程切换后的操作
        int sel = sf->ds;  // 获取数据段选择子
        char *str = (char*)sf->edx;  // 获取目标字符串指针
        int size = sf->ebx;  // 获取要读取的字节数
        char c = 0;  // 用于存储读取的字符

        // 设置 ES 段寄存器
        asm volatile("movw %0, %%es"::"m"(sel));
        
        // 从键盘缓冲区读取字符
        int i;
        for (i = 0; i < size - 1; i++) {
            if (bufferHead == bufferTail) break;  // 如果缓冲区为空，退出循环
            c = getChar(keyBuffer[bufferHead]);  // 获取字符
            bufferHead = (bufferHead + 1) % MAX_KEYBUFFER_SIZE;  // 更新缓冲区头指针
            putChar(c);  // 将字符输出到屏幕
            if (c != 0) {
                // 将字符存储到目标字符串中
                asm volatile("movb %0, %%es:(%1)"::"r"(c), "r"(str + i));
            } else {
                i--;  // 如果字符为 0，调整索引以覆盖该位置
            }
        }

        // 在目标字符串末尾添加终止符
        asm volatile("movb $0x00, %%es:(%0)"::"r"(str + i));
        pcb[current].regs.eax = i;  // 设置返回值为读取的字节数
    }
}

void syscallFork(struct StackFrame *sf) {
	int i, j;
	for (i = 0; i < MAX_PCB_NUM; i++) {
		if (pcb[i].state == STATE_DEAD)
			break;
	}
	if (i != MAX_PCB_NUM) {
		/* copy userspace
		   enable interrupt
		 */
		enableInterrupt();
		for (j = 0; j < 0x100000; j++) {
			*(uint8_t *)(j + (i+1)*0x100000) = *(uint8_t *)(j + (current+1)*0x100000);
			//asm volatile("int $0x20"); // Testing irqTimer during syscall
		}
		/* disable interrupt
		 */
		disableInterrupt();
		/* set pcb
		   pcb[i]=pcb[current] doesn't work
		*/
		pcb[i].stackTop = (uint32_t)&(pcb[i].stackTop) -
			((uint32_t)&(pcb[current].stackTop) - pcb[current].stackTop);
		pcb[i].prevStackTop = (uint32_t)&(pcb[i].stackTop) -
			((uint32_t)&(pcb[current].stackTop) - pcb[current].prevStackTop);
		pcb[i].state = STATE_RUNNABLE;
		pcb[i].timeCount = pcb[current].timeCount;
		pcb[i].sleepTime = pcb[current].sleepTime;
		pcb[i].pid = i;
		/* set regs */
		pcb[i].regs.ss = USEL(2+i*2);
		pcb[i].regs.esp = pcb[current].regs.esp;
		pcb[i].regs.eflags = pcb[current].regs.eflags;
		pcb[i].regs.cs = USEL(1+i*2);
		pcb[i].regs.eip = pcb[current].regs.eip;
		pcb[i].regs.eax = pcb[current].regs.eax;
		pcb[i].regs.ecx = pcb[current].regs.ecx;
		pcb[i].regs.edx = pcb[current].regs.edx;
		pcb[i].regs.ebx = pcb[current].regs.ebx;
		pcb[i].regs.xxx = pcb[current].regs.xxx;
		pcb[i].regs.ebp = pcb[current].regs.ebp;
		pcb[i].regs.esi = pcb[current].regs.esi;
		pcb[i].regs.edi = pcb[current].regs.edi;
		pcb[i].regs.ds = USEL(2+i*2);
		pcb[i].regs.es = pcb[current].regs.es;
		pcb[i].regs.fs = pcb[current].regs.fs;
		pcb[i].regs.gs = pcb[current].regs.gs;
		/* set return value */
		pcb[i].regs.eax = 0;
		pcb[current].regs.eax = i;
	}
	else {
		pcb[current].regs.eax = -1;
	}
	return;
}

void syscallExec(struct StackFrame *sf) {
	return;
}

void syscallSleep(struct StackFrame *sf) {
	if (sf->ecx == 0)
		return;
	else {
		pcb[current].state = STATE_BLOCKED;
		pcb[current].sleepTime = sf->ecx;
		asm volatile("int $0x20");
		return;
	}
}

void syscallExit(struct StackFrame *sf) {
	pcb[current].state = STATE_DEAD;
	asm volatile("int $0x20");
	return;
}

void syscallSem(struct StackFrame *sf) {
	switch(sf->ecx) {
		case SEM_INIT:
			syscallSemInit(sf);
			break;
		case SEM_WAIT:
			syscallSemWait(sf);
			break;
		case SEM_POST:
			syscallSemPost(sf);
			break;
		case SEM_DESTROY:
			syscallSemDestroy(sf);
			break;
		default:break;
	}
}

// 查找未使用的信号量
int findUnusedSemaphore() {
    for (int i = 0; i < MAX_SEM_NUM; i++) {
        if (sem[i].state == 0) {
            return i;  // 返回未使用的信号量索引
        }
    }
    return -1;  // 如果没有找到未使用的信号量，返回 -1
}
// 初始化信号量
void initSemaphore(int semIndex, int32_t initialValue) {
    sem[semIndex].state = 1;  // 设置信号量状态为使用中
    sem[semIndex].value = initialValue;  // 设置信号量的初始值
    sem[semIndex].pcb.next = &(sem[semIndex].pcb);  // 初始化信号量的队列指针
    sem[semIndex].pcb.prev = &(sem[semIndex].pcb);
}
void syscallSemInit(struct StackFrame *sf) {
	// TODO: complete `SemInit`
    int semIndex = findUnusedSemaphore();  // 查找未使用的信号量索引

    pcb[current].regs.eax = semIndex;  // 设置返回值为找到的信号量索引

    // 如果找到未使用的信号量，进行初始化
    if (semIndex != -1) {
        initSemaphore(semIndex, (int32_t)sf->edx);  // 初始化信号量
    }

    return;
}


// 检查信号量是否有效
_Bool isValidSemaphore(int semIndex) {
    return sem[semIndex].state != 0;
}

// 阻塞当前进程
void blockCurrentProcess(int semIndex) {
    // 将当前进程加入到信号量的阻塞队列中
    pcb[current].blocked.next = sem[semIndex].pcb.next;
    pcb[current].blocked.prev = &(sem[semIndex].pcb);
    sem[semIndex].pcb.next = &(pcb[current].blocked);
    (pcb[current].blocked.next)->prev = &(pcb[current].blocked);

    // 将当前进程状态设置为阻塞，并设置睡眠时间为 -1（表示无限期睡眠）
    pcb[current].state = STATE_BLOCKED;
    pcb[current].sleepTime = -1;

    // 触发调度程序进行进程切换
    triggerScheduler();
}

// 等待信号量
void waitOnSemaphore(int semIndex) {
    pcb[current].regs.eax = 0;  // 设置返回值为 0，表示成功
    sem[semIndex].value--;  // 减少信号量的值

    // 如果信号量值小于 0，表示有进程需要被阻塞
    if (sem[semIndex].value < 0) {
        blockCurrentProcess(semIndex);  // 阻塞当前进程
    }
}


void syscallSemWait(struct StackFrame *sf) {
	// TODO: complete `SemWait` and note that you need to consider some special situations
    int semIndex = (int)sf->edx;  // 获取信号量索引

    if (isValidSemaphore(semIndex)) {
        waitOnSemaphore(semIndex);  // 等待信号量
    } else {
        pcb[current].regs.eax = -1;  // 设置返回值为 -1，表示信号量未使用
    }
}


// 从信号量队列中移除一个阻塞的进程
void removeFromSemaphoreQueue(int semIndex) {
    sem[semIndex].pcb.prev = (sem[semIndex].pcb.prev)->prev;
    (sem[semIndex].pcb.prev)->next = &(sem[semIndex].pcb);
}
// 获取阻塞在信号量上的进程表指针
ProcessTable* getNextBlockedProcess(int semIndex) {
    return (ProcessTable*)((uint32_t)(sem[semIndex].pcb.prev) - (uint32_t)&(((ProcessTable*)0)->blocked));
}
void syscallSemPost(struct StackFrame *sf) {
    int semIndex = (int)sf->edx;
    ProcessTable *pt = NULL;
    // 检查信号量索引是否在合法范围内
    if (semIndex < 0 || semIndex >= MAX_SEM_NUM) {
        pcb[current].regs.eax = -1;  // 设置返回值为-1，表示错误
        return;
    }

	// TODO
    // 检查信号量是否有效
    if (sem[semIndex].state == 0) {
        pcb[current].regs.eax = -1;  // 设置返回值为-1，表示信号量未使用
    } else {
        pcb[current].regs.eax = 0;  // 设置返回值为0，表示成功
        sem[semIndex].value++;  // 增加信号量的值
        // 如果有进程阻塞在信号量上，唤醒一个阻塞进程
        if (sem[semIndex].value <= 0) {
            pt = getNextBlockedProcess(semIndex);  // 获取阻塞的进程表指针
            removeFromSemaphoreQueue(semIndex);  // 从信号量队列中移除该进程
            pt->state = STATE_RUNNABLE;  // 将进程状态设置为可运行
            pt->sleepTime = 0;  // 重置睡眠时间
        }
    }
}
void syscallSemDestroy(struct StackFrame *sf) {
	// TODO: complete `SemDestroy`
	// 获取信号量的索引
    int semIndex = (int)sf->edx;
    // 检查信号量是否有效
    if (sem[semIndex].state == 0) {
        // 如果信号量未被使用，返回错误码 -1
        pcb[current].regs.eax = -1;
    } 
	else {
        // 如果信号量有效，设置返回值为 0
        pcb[current].regs.eax = 0;
        // 销毁信号量，将其状态设置为未使用
        sem[semIndex].state = 0;
        // 触发调度程序进行进程切换
        triggerScheduler();
    }
    return;
}
void syscallPid(struct StackFrame *sf){
	pcb[current].regs.eax = current;
	return ;
}
