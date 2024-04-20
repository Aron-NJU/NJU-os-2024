#include "x86.h"
#include "device.h"
#include "common.h"

extern int displayRow;
extern int displayCol;

extern uint32_t keyBuffer[MAX_KEYBUFFER_SIZE];
extern int bufferHead;
extern int bufferTail;

int tail = 0;
void GProtectFaultHandle(struct TrapFrame *tf);

void KeyboardHandle(struct TrapFrame *tf);

void syscallHandle(struct TrapFrame *tf);
void syscallWrite(struct TrapFrame *tf);
void syscallPrint(struct TrapFrame *tf);
void syscallRead(struct TrapFrame *tf);
void syscallGetChar(struct TrapFrame *tf);
void syscallGetStr(struct TrapFrame *tf);


void irqHandle(struct TrapFrame *tf) { // pointer tf = esp
	/*
	 * 中断处理程序
	 */
	/* Reassign segment register */
	putStr("irq.\n");
	asm volatile("movw %%ax, %%ds"::"a"(KSEL(SEG_KDATA)));
	//asm volatile("movw %%ax, %%es"::"a"(KSEL(SEG_KDATA)));
	//asm volatile("movw %%ax, %%fs"::"a"(KSEL(SEG_KDATA)));
	//asm volatile("movw %%ax, %%gs"::"a"(KSEL(SEG_KDATA)));
	switch(tf->irq) {
		// TODO: 填好中断处理程序的调用
		case -1:{
			putStr("empty.\n");
			break;
		}
		case 0xd:{
			putStr("GPF.\n");
			GProtectFaultHandle(tf);
			break;
		}
		case 0x21:{
			putStr("keyboard.\n");
			KeyboardHandle(tf);
			break;
		}
		case 0x80:{
			putStr("syscall.\n");
			syscallHandle(tf);
			break;
		}
		default:{
			putStr("unkown.\n");
			assert(0);
		}
	}
}

void GProtectFaultHandle(struct TrapFrame *tf){
	assert(0);
	return;
}

void KeyboardHandle(struct TrapFrame *tf){
	uint32_t code = getKeyCode();

	if(code == 0xe){ // 退格符
		//要求只能退格用户键盘输入的字符串，且最多退到当行行首
		if(displayCol>0&&displayCol>tail){
			displayCol--;
			uint16_t data = 0 | (0x0c << 8);
			int pos = (80*displayRow+displayCol)*2;
			asm volatile("movw %0, (%1)"::"r"(data),"r"(pos+0xb8000));
		}
	}else if(code == 0x1c){ // 回车符
		//处理回车情况
		keyBuffer[bufferTail++]='\n';
		displayRow++;
		displayCol=0;
		tail=0;
		if(displayRow==25){
			scrollScreen();
			displayRow=24;
			displayCol=0;
		}
	}else if (code < 0x81) { 
		// 正常的字符输入
        char ascii = getChar(code);
        if (ascii != 0) { // 有效字符
            keyBuffer[bufferTail++] = ascii; // 存入缓冲区

            // 显示字符到屏幕
            uint16_t data = ascii | (0x0c << 8);
            int pos = (80 * displayRow + displayCol) * 2;
            asm volatile("movw %0, (%1)"::"r"(data), "r"(pos + 0xb8000));
            displayCol++; // 增加列位置
            if (displayCol >= 80) { // 到达行末尾，自动换行
                displayRow++;
                displayCol = 0;
                if (displayRow == 25) {
                    scrollScreen(); // 滚动屏幕
                    displayRow = 24;
                }
            }
        }
    }
	updateCursor(displayRow, displayCol);
	
}

void syscallHandle(struct TrapFrame *tf) {
	
	switch(tf->eax) { // syscall number
		case 0:
			syscallWrite(tf);
			break; // for SYS_WRITE
		case 1:
			syscallRead(tf);
			break; // for SYS_READ
		default:break;
	}
}

void syscallWrite(struct TrapFrame *tf) {
	switch(tf->ecx) { // file descriptor
		case 0:
			syscallPrint(tf);
			break; // for STD_OUT
		default:break;
	}
}

void syscallPrint(struct TrapFrame *tf) {
	putStr("syscall print.\n");
	int sel = USEL(SEG_UDATA); //TODO: segment selector for user data, need further modification
	char *str = (char*)tf->edx;
	int size = tf->ebx;
	int i = 0;
	int pos = 0;
	char character = 0;
	uint16_t data = 0;
	asm volatile("movw %0, %%es"::"m"(sel));
	for (i = 0; i < size; i++) {
		asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str+i));
		// TODO: 完成光标的维护和打印到显存
		if (character == '\n') {
            // 处理换行符
            displayRow++;
            displayCol = 0;
            if (displayRow == 25) {
                scrollScreen(); // 滚动屏幕
                displayRow = 24;
            }
        } else {
            // 显示字符
            data = (uint16_t)character | (0x0c << 8); // 默认属性（例如白底黑字）
            pos = (80 * displayRow + displayCol) * 2; // 计算显存位置
            // 使用了段寄存器 asm volatile("movw %0, %%es:(%1)" :: "r"(data), "r"(0xB8000 + pos));
			asm volatile("movw %0, (%1)" :: "r"(data), "r"(0xB8000 + pos));
            displayCol++;
            if (displayCol >= 80) {
                displayRow++;
                displayCol = 0;
                if (displayRow == 25) {
                    scrollScreen(); // 滚动屏幕
                    displayRow = 24;
                }
            }
        }
	}
	
	updateCursor(displayRow, displayCol);
}

void syscallRead(struct TrapFrame *tf){
	switch(tf->ecx){ //file descriptor
		case 0:
			syscallGetChar(tf);
			break; // for STD_IN
		case 1:
			syscallGetStr(tf);
			break; // for STD_STR
		default:break;
	}
}

void syscallGetChar(struct TrapFrame *tf){
	// TODO: 自由实现
	putStr("syscall getchar.\n");
	if (bufferHead!=bufferTail|| keyBuffer[bufferTail+1] == '\n'){
		bufferTail--;
		tf->eax=keyBuffer[bufferHead];
		bufferTail  = bufferHead; 
}
	enableInterrupt();//asm volatile("sti"); 已封装，声明在include/x86/cpu; 能够响应
	while(bufferHead==bufferTail|| keyBuffer[bufferTail - 1] != '\n'){
		waitForInterrupt();
}
	disableInterrupt();//asm volatile("cli");
	putChar(keyBuffer[bufferHead]);
	putChar(keyBuffer[bufferTail-1]);
	tf->eax=keyBuffer[bufferHead];
	bufferTail  = bufferHead; // 防止测试下次str前面还有数字
}

void syscallGetStr(struct TrapFrame *tf){
	// TODO: 自由实现
	putStr("syscall getstr.\n");
	// 保存初始寄存器状态
    uint32_t old_edx = tf->edx;

    int sel = USEL(SEG_UDATA); 
    char *str = (char*)tf->edx;
    int size = tf->ebx;
    char c = 0;
	int i =0 ;
    asm volatile("movw %0, %%es"::"m"(sel));// 设置额外段寄存器
	while (i < size - 1) {
        enableInterrupt();
		while(bufferHead==bufferTail){
		waitForInterrupt();

}
		disableInterrupt();
        c = keyBuffer[bufferHead];
        bufferHead = (bufferHead + 1) % MAX_KEYBUFFER_SIZE;
		
        if (c == '\n' || c == '\r') {
            str[i] = '\0';
            break;
        }
		asm volatile("movb %0, %%es:(%1)"::"r"(c),"r"(str + i)); // 写入到用户空间
        i++;
    }

    asm volatile("movb $0x00, %%es:(%0)"::"r"(str + size)); // 设置字符串结束符
    bufferHead = bufferTail; // 重置缓冲区头尾指针
	tf->edx = old_edx;
	putChar('\n');
	putStr(str);
	putChar('\n');
    tf->ebx = i;  // 可以在 ebx 寄存器返回字符串长度
}
