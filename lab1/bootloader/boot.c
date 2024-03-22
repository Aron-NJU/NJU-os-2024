#include "boot.h"

#define SECTSIZE 512
#define APP_SECTOR 1

void bootMain(void) {
    /* 加载应用程序至内存 */
    char *buffer = (char*)0x8c00;
    int offset = APP_SECTOR;  // 用户程序在磁盘上的扇区偏移
    readSect(buffer, offset);
  
    /* 跳转至应用程序入口 */
    void (*entry)(void); // 定义函数指针
    entry = (void(*)(void))buffer; // 强制类型转换
    entry();
}



void waitDisk(void) { // waiting for disk
	while((inByte(0x1F7) & 0xC0) != 0x40);
}

void readSect(void *dst, int offset) { // reading a sector of disk
	int i;
	waitDisk();
	outByte(0x1F2, 1);
	outByte(0x1F3, offset);
	outByte(0x1F4, offset >> 8);
	outByte(0x1F5, offset >> 16);
	outByte(0x1F6, (offset >> 24) | 0xE0);
	outByte(0x1F7, 0x20);

	waitDisk();
	for (i = 0; i < SECTSIZE / 4; i ++) {
		((int *)dst)[i] = inLong(0x1F0);
	}
}
