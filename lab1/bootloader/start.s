# TODO: This is lab1.1
/* Real Mode Hello World 
.code16

.global start
start:
	movw %cs, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %ss
	movw $0x7d00, %ax
	movw %ax, %sp # setting stack pointer to 0x7d00
	# TODO:通过中断输出Hello World
	movw $message, %si

print_loop:
	lodsb                    #Load byte at address DS:(E)SI into AL
	or %al, %al              # Compare AL with 0
	jz done                  # Jump short if ZF=1 (AL=0)
	movb $0x0e, %ah          # Function AH=0eh (TTY output)
	int $0x10                # BIOS interrupt 10h
	jmp print_loop           #Jump to loop start

done:
    hlt
loop:
	jmp loop

message:
	.string "Hello, World!\n\0"

*/

# TODO: This is lab1.2
/* Protected Mode Hello World 

.code16

.global start
start:
	movw %cs, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %ss

	# 关闭中断
    cli     

	# 启动A20总线
	inb $0x92, %al 
	orb $0x02, %al
	outb %al, $0x92

	# 加载GDTR
	data32 addr32 lgdt gdtDesc # loading gdtr, data32, addr32


	# TODO：设置CR0的PE位（第0位）为1
    movl %cr0, %eax
    orl $0x1, %eax
    movl %eax, %cr0


	# 长跳转切换至保护模式
	data32 ljmp $0x08, $start32 # reload code segment selector and ljmp to start32, data32


.code32
start32:
	movw $0x10, %ax # 设置数据段选择符为0x10
	movw %ax, %ds     # 将数据段选择符加载到ds寄存器中
	movw %ax, %es     # 将数据段选择符加载到es寄存器中
	movw %ax, %fs     # 将数据段选择符加载到fs寄存器中
	movw %ax, %ss     # 将数据段选择符加载到ss寄存器中
	movw $0x18, %ax   # 设置图形数据段选择符为0x18
	movw %ax, %gs     # 将图形数据段选择符加载到gs寄存器中
	
	movl $0x8000, %eax  # 设置堆栈指针esp为0x8000
	movl %eax, %esp

# 输出Hello World
    movl $message, %esi   # 将消息地址加载到esi寄存器中
    call print_string     # 调用打印字符串的函数

loop32:
	jmp loop32

print_string:
    movl $0xb8000, %edi  # 将显存地址 0xb8000 加载到edi寄存器中，这是文本模式下的显存地址范围
    movb $0x07, %ah      # 设置属性：亮白色字符，黑色背景
.loop:
    lodsb                # 从esi指向的地址加载一个字节到al寄存器，并且esi加1 
    testb %al, %al       # 检查al寄存器是否为0，即字符串是否结束 
    jz .done             # 如果是则结束 
    stosw                # 将ax寄存器中的内容存放到es:di指向的地址处，并且edi加2 
    jmp .loop            # 继续循环 
.done:
    ret                  # 返回 

message:
    .string "Hello, World!\n\0"

.p2align 2
gdt: # 8 bytes for each table entry, at least 1 entry
	# .word limit[15:0],base[15:0]
	# .byte base[23:16],(0x90|(type)),(0xc0|(limit[19:16])),base[31:24]
	# GDT第一个表项为空
	.word 0,0
	.byte 0,0,0,0

    # code segment entry
	.word 0xffff,0  #段限制：15：0
	.byte 0,0x9a,0xcf,0 # 基地址0；读/执行且DPL=0；G=1(4KB粒度), D/B=1(32比特默认操作大小), L=0, AVL=0, 段限制19:16 = 1111

    # data segment entry
	.word 0xffff,0 
	.byte 0,0x92,0xcf,0 #读/写

    # graphics segment entry
	.word 0xffff,0x8000 
	.byte 0x0b,0xfa,0xcf,0 # 读写/可执行


gdtDesc: 
    .word (gdtDesc - gdt -1) 
    .long gdt 


*/


# TODO: This is lab1.3

/* Protected Mode Loading Hello World APP */
.code16

.global start
start:
	movw %cs, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %ss
	# 关闭中断
    cli     

	# 启动A20总线
	inb $0x92, %al 
	orb $0x02, %al
	outb %al, $0x92

	# 加载GDTR
	data32 addr32 lgdt gdtDesc # loading gdtr, data32, addr32


	# TODO：设置CR0的PE位（第0位）为1
    movl %cr0, %eax
    orl $0x1, %eax
    movl %eax, %cr0


	# 长跳转切换至保护模式
	data32 ljmp $0x08, $start32 # reload code segment selector and ljmp to start32, data32

.code32
start32:
	movw $0x10, %ax # setting data segment selector
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	movw %ax, %ss
	movw $0x18, %ax # setting graphics data segment selector
	movw %ax, %gs
	
	movl $0x8000, %eax # setting esp
	movl %eax, %esp
	jmp bootMain # jump to bootMain in boot.c

.p2align 2
gdt: # 8 bytes for each table entry, at least 1 entry
	# .word limit[15:0],base[15:0]
	# .byte base[23:16],(0x90|(type)),(0xc0|(limit[19:16])),base[31:24]
	# GDT第一个表项为空
	.word 0,0
	.byte 0,0,0,0

    # code segment entry
	.word 0xffff,0  #段限制：15：0
	.byte 0,0x9a,0xcf,0 # 基地址0；读/执行且DPL=0；G=1(4KB粒度), D/B=1(32比特默认操作大小), L=0, AVL=0, 段限制19:16 = 1111

    # data segment entry
	.word 0xffff,0 
	.byte 0,0x92,0xcf,0 #读/写

    # graphics segment entry
	.word 0xffff,0x8000 
	.byte 0x0b,0xfa,0xcf,0 # 读写/可执行

gdtDesc: 
	.word (gdtDesc - gdt - 1) 
	.long gdt 

