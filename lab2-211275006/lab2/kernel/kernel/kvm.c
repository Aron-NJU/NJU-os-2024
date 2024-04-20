#include "x86.h"
#include "device.h"

SegDesc gdt[NR_SEGMENTS];       // the new GDT, NR_SEGMENTS=7, defined in x86/memory.h
TSS tss;

//init GDT and LDT
void initSeg() { // setup kernel segements
	gdt[SEG_KCODE] = SEG(STA_X | STA_R, 0,       0xffffffff, DPL_KERN);
	gdt[SEG_KDATA] = SEG(STA_W,         0,       0xffffffff, DPL_KERN);
	//gdt[SEG_UCODE] = SEG(STA_X | STA_R, 0,       0xffffffff, DPL_USER);
	gdt[SEG_UCODE] = SEG(STA_X | STA_R, 0x00200000,0x000fffff, DPL_USER);
	//gdt[SEG_UDATA] = SEG(STA_W,         0,       0xffffffff, DPL_USER);
	gdt[SEG_UDATA] = SEG(STA_W,         0x00200000,0x000fffff, DPL_USER);
	gdt[SEG_TSS] = SEG16(STS_T32A,      &tss, sizeof(TSS)-1, DPL_KERN);
	gdt[SEG_TSS].s = 0;
	setGdt(gdt, sizeof(gdt)); // gdt is set in bootloader, here reset gdt in kernel

	/*
	 * 初始化TSS
	 */
	tss.esp0 = 0x1fffff;
	tss.ss0 = KSEL(SEG_KDATA);
	asm volatile("ltr %%ax":: "a" (KSEL(SEG_TSS)));

	/*设置正确的段寄存器*/
	asm volatile("movw %%ax,%%ds":: "a" (KSEL(SEG_KDATA)));
	//asm volatile("movw %%ax,%%es":: "a" (KSEL(SEG_KDATA)));
	//asm volatile("movw %%ax,%%fs":: "a" (KSEL(SEG_KDATA)));
	//asm volatile("movw %%ax,%%gs":: "a" (KSEL(SEG_KDATA)));
	asm volatile("movw %%ax,%%ss":: "a" (KSEL(SEG_KDATA)));

	lLdt(0);
	
}

void enterUserSpace(uint32_t entry) {
	/*
	 * Before enter user space 
	 * you should set the right segment registers here
	 * and use 'iret' to jump to ring3
	 */
	uint32_t EFLAGS = 0;
	asm volatile("pushl %0":: "r" (USEL(SEG_UDATA))); // push ss
	asm volatile("pushl %0":: "r" (0x2fffff)); 
	asm volatile("pushfl"); //push eflags, sti
	asm volatile("popl %0":"=r" (EFLAGS));
	asm volatile("pushl %0"::"r"(EFLAGS|0x200));
	asm volatile("pushl %0":: "r" (USEL(SEG_UCODE))); // push cs
	asm volatile("pushl %0":: "r" (entry)); 
	asm volatile("iret");
}

/*
kernel is loaded to location 0x100000, i.e., 1MB
size of kernel is not greater than 200*512 bytes, i.e., 100KB
user program is loaded to location 0x200000, i.e., 2MB
size of user program is not greater than 200*512 bytes, i.e., 100KB
*/
#define UMAIN_ADDR 0x200000      // 用户程序加载地址
#define SECTOR_COUNT 200         // 用户程序占用的扇区数
#define UMAIN_START_SECTOR 201   // 用户程序起始扇区
void loadUMain(void) {
	// TODO: 参照bootloader加载内核的方式，由kernel加载用户程序
	//int phoff = 0x34; // program header offset
	int offset = 0x1000; // .text section offset
	uint32_t loadAddress = 0x200000; 
	uint32_t uMainEntry = 0x200000;

	for (int i = 0; i < SECTOR_COUNT; i++) {
		readSect((void*)(loadAddress + i*512), UMAIN_START_SECTOR+i); //第二个参数是偏移量
	}
	// 解析ELF头和程序头
	uMainEntry = ((struct ELFHeader *)loadAddress)->entry; // entry address of the program
	//phoff = ((struct ELFHeader *)loadAddress)->phoff;
	//offset = ((struct ProgramHeader *)(loadAddress + phoff))->off;

	// 调整内存中的程序代码位置
	for (int i = 0; i < SECTOR_COUNT * 512; i++) {
		*(uint8_t *)(loadAddress + i) = *(uint8_t *)(loadAddress + i + offset);
	}
	putStr("Loading user program at address: 0x");
    putNum(loadAddress);
    putStr("\nEntry point: 0x");
    putNum(uMainEntry);
    putStr("\n");
	enterUserSpace(uMainEntry);
}
