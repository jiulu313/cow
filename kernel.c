/* 定义 8 位无符号整数类型，避免依赖 C 标准库头文件。 */
typedef unsigned char uint8_t;

/* 定义 16 位无符号整数类型，用来写 VGA 字符单元。 */
typedef unsigned short uint16_t;

/* 定义 32 位无符号整数类型，用来保存处理函数地址。 */
typedef unsigned int uint32_t;

/* VGA 文本模式显存的物理起始地址。 */
enum { VGA_BUFFER = 0xB8000 };

/* VGA 文本模式每一行拥有的字符列数。 */
enum { VGA_WIDTH = 80 };

/* VGA 文本模式拥有的总行数。 */
enum { VGA_HEIGHT = 25 };

/* VGA 颜色值：黑色背景、浅灰色前景。 */
enum { COLOR_LIGHT_GREY = 0x07 };

/* VGA 颜色值：黑色背景、浅绿色前景。 */
enum { COLOR_LIGHT_GREEN = 0x0A };

/* VGA 颜色值：黑色背景、浅红色前景。 */
enum { COLOR_LIGHT_RED = 0x0C };

/* IDT 中断向量的总数量。 */
enum { IDT_ENTRIES = 256 };

/* 保存终端下一次输出所在的屏幕行。 */
static uint16_t terminal_row;

/* 保存终端下一次输出所在的屏幕列。 */
static uint16_t terminal_column;

/* 声明汇编文件提供的 32 个异常入口地址组成的表。 */
extern void (*exception_stub_table[32])(void);

/* 定义 IDT 中单个 8 字节门描述符的内存布局。 */
struct idt_entry {
    /* 保存处理函数地址的低 16 位。 */
    uint16_t offset_low;

    /* 保存处理函数所在代码段的选择子。 */
    uint16_t selector;

    /* 此字节必须为 0。 */
    uint8_t zero;

    /* 保存门类型、特权级与“存在”标志。 */
    uint8_t type_attributes;

    /* 保存处理函数地址的高 16 位。 */
    uint16_t offset_high;
} __attribute__((packed));

/* 定义 lidt 指令需要的 6 字节 IDT 描述符布局。 */
struct idt_pointer {
    /* 保存 IDT 总字节数减 1。 */
    uint16_t limit;

    /* 保存 IDT 在内存中的线性地址。 */
    uint32_t base;
} __attribute__((packed));

/* 为 256 个中断向量预留门描述符；此数组位于 BSS。 */
static struct idt_entry idt[IDT_ENTRIES];

/* 将一个 ASCII 字符与一个 VGA 颜色属性合成为一个 16 位字符单元。 */
static uint16_t vga_entry(char character, uint8_t color) {
    /* 字符放在低 8 位，颜色属性放在高 8 位。 */
    return (uint16_t)character | ((uint16_t)color << 8);
}

/* 向 x86 I/O 端口写入一个字节。 */
static void outb(uint16_t port, uint8_t value) {
    /* 使用 outb 指令，将 value 写入 DX 指定的端口。 */
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

/* 将当前终端行列位置同步到 VGA 文本模式的硬件光标。 */
static void terminal_update_cursor(void) {
    /* 计算当前光标在 80 × 25 文本模式中的线性位置。 */
    const uint16_t position = terminal_row * VGA_WIDTH + terminal_column;

    /* 选择 VGA CRT 控制器的“光标位置高字节”寄存器。 */
    outb(0x3D4, 14);

    /* 写入光标位置的高 8 位。 */
    outb(0x3D5, (uint8_t)(position >> 8));

    /* 选择 VGA CRT 控制器的“光标位置低字节”寄存器。 */
    outb(0x3D4, 15);

    /* 写入光标位置的低 8 位。 */
    outb(0x3D5, (uint8_t)position);
}

/* 将整个 80 × 25 的 VGA 文本屏幕填充为空格。 */
static void terminal_clear(void) {
    /* 逐行处理屏幕。 */
    for (uint16_t row = 0; row < VGA_HEIGHT; ++row) {
        /* 逐列处理当前行。 */
        for (uint16_t column = 0; column < VGA_WIDTH; ++column) {
            /* 取得 VGA 中当前字符单元的线性编号。 */
            const uint16_t index = row * VGA_WIDTH + column;

            /* 将当前单元写成浅灰色空格。 */
            ((volatile uint16_t *)VGA_BUFFER)[index] = vga_entry(' ', COLOR_LIGHT_GREY);
        }
    }

    /* 清屏后将下一次输出位置重置到屏幕左上角。 */
    terminal_row = 0;

    /* 清屏后将下一次输出位置重置到第 0 列。 */
    terminal_column = 0;

    /* 将硬件光标移动到清屏后的左上角。 */
    terminal_update_cursor();
}

/* 将屏幕第 2 到第 25 行向上复制一行，并清空新的最后一行。 */
static void terminal_scroll(void) {
    /* 从第 1 行开始处理，因为第 0 行将被下一行覆盖。 */
    for (uint16_t row = 1; row < VGA_HEIGHT; ++row) {
        /* 逐列复制当前行的每一个字符单元。 */
        for (uint16_t column = 0; column < VGA_WIDTH; ++column) {
            /* 计算当前行字符单元的线性编号。 */
            const uint16_t source_index = row * VGA_WIDTH + column;

            /* 计算上一行对应字符单元的线性编号。 */
            const uint16_t target_index = (row - 1) * VGA_WIDTH + column;

            /* 将当前行字符和颜色完整复制到上一行。 */
            ((volatile uint16_t *)VGA_BUFFER)[target_index] = ((volatile uint16_t *)VGA_BUFFER)[source_index];
        }
    }

    /* 逐列清空滚屏后露出的最后一行。 */
    for (uint16_t column = 0; column < VGA_WIDTH; ++column) {
        /* 计算最后一行当前字符单元的线性编号。 */
        const uint16_t index = (VGA_HEIGHT - 1) * VGA_WIDTH + column;

        /* 将最后一行的当前单元写成浅灰色空格。 */
        ((volatile uint16_t *)VGA_BUFFER)[index] = vga_entry(' ', COLOR_LIGHT_GREY);
    }
}

/* 让光标移到下一行的第 0 列；到达屏幕底部时执行滚屏。 */
static void terminal_newline(void) {
    /* 将列位置重置为一行的开头。 */
    terminal_column = 0;

    /* 将行位置移动到下一行。 */
    ++terminal_row;

    /* 如果已经超过最后一行，滚动屏幕以腾出新的最后一行。 */
    if (terminal_row == VGA_HEIGHT) {
        /* 将当前屏幕内容向上滚动一行。 */
        terminal_scroll();

        /* 将光标放在滚屏后空出的最后一行。 */
        terminal_row = VGA_HEIGHT - 1;
    }

    /* 将新的行列位置同步到 VGA 硬件光标。 */
    terminal_update_cursor();
}

/* 以给定颜色输出一个字符，并自动更新终端光标位置。 */
static void terminal_putchar(char character, uint8_t color) {
    /* 如果字符是换行符，不写显存，直接移动到下一行。 */
    if (character == '\n') {
        /* 执行换行操作。 */
        terminal_newline();

        /* 换行符已经处理完毕，因此从函数返回。 */
        return;
    }

    /* 计算当前光标在 80 × 25 VGA 显存中的线性编号。 */
    const uint16_t index = terminal_row * VGA_WIDTH + terminal_column;

    /* 将当前字符和颜色写入 VGA 显存。 */
    ((volatile uint16_t *)VGA_BUFFER)[index] = vga_entry(character, color);

    /* 将光标移动到当前行的下一列。 */
    ++terminal_column;

    /* 如果光标已经越过第 80 列，自动换到下一行。 */
    if (terminal_column == VGA_WIDTH) {
        /* 执行自动换行操作。 */
        terminal_newline();

        /* 自动换行已经同步了硬件光标，因此直接返回。 */
        return;
    }

    /* 将同一行内移动后的列位置同步到 VGA 硬件光标。 */
    terminal_update_cursor();
}

/* 从当前光标位置开始，以给定颜色输出一个以 0 结尾的字符串。 */
static void terminal_write(const char *text, uint8_t color) {
    /* 从字符串的第 0 个字符开始读取。 */
    uint16_t index = 0;

    /* 逐字符循环，直到遇到 C 字符串的结束标记 0。 */
    while (text[index] != '\0') {
        /* 将当前字符交给终端字符输出函数处理。 */
        terminal_putchar(text[index], color);

        /* 移动到字符串的下一个字符。 */
        ++index;
    }
}

/* 以十六进制格式输出一个 32 位无符号整数，便于阅读异常号和错误码。 */
static void terminal_write_hex32(uint32_t value, uint8_t color) {
    /* 定义十六进制数字到 ASCII 字符的映射表。 */
    const char *digits = "0123456789ABCDEF";

    /* 输出十六进制数的常用前缀。 */
    terminal_write("0x", color);

    /* 从最高的第 7 个十六进制数字开始处理。 */
    for (int shift = 28; shift >= 0; shift -= 4) {
        /* 取出当前 4 个二进制位，得到一个 0 到 15 的数字。 */
        const uint8_t digit = (uint8_t)((value >> shift) & 0x0F);

        /* 输出该数字对应的十六进制 ASCII 字符。 */
        terminal_putchar(digits[digit], color);
    }
}

/* 在 IDT 的指定向量位置安装一个 32 位中断门。 */
static void idt_set_gate(uint8_t vector, void (*handler)(void)) {
    /* 将函数指针转换为可拆分的 32 位处理函数地址。 */
    const uint32_t address = (uint32_t)handler;

    /* 将处理函数地址的低 16 位写入门描述符。 */
    idt[vector].offset_low = (uint16_t)address;

    /* 使用 GDT 中索引为 1 的内核代码段选择子 0x08。 */
    idt[vector].selector = 0x08;

    /* 按 x86 规范将保留字节写为 0。 */
    idt[vector].zero = 0;

    /* 设置存在位、特权级 0 与 32 位中断门类型，数值为 0x8E。 */
    idt[vector].type_attributes = 0x8E;

    /* 将处理函数地址的高 16 位写入门描述符。 */
    idt[vector].offset_high = (uint16_t)(address >> 16);
}

/* 将 IDT 描述符地址加载到 CPU 的 IDTR 专用寄存器。 */
static void idt_load(const struct idt_pointer *pointer) {
    /* 执行 x86 lidt 指令；它从 pointer 指向的 6 字节结构读取 limit 和 base。 */
    __asm__ volatile ("lidtl (%0)" : : "r"(pointer) : "memory");
}

/* 建立并加载异常向量 0 到 31 的最小 IDT。 */
static void idt_initialize(void) {
    /* 创建 lidt 所需的 IDT 描述符，base 指向 idt 数组。 */
    const struct idt_pointer pointer = { (uint16_t)(sizeof(idt) - 1), (uint32_t)idt };

    /* 逐项将 32 个 CPU 异常向量连接到对应的汇编异常入口。 */
    for (uint8_t vector = 0; vector < 32; ++vector) {
        /* 将当前向量的门描述符写入 IDT。 */
        idt_set_gate(vector, exception_stub_table[vector]);
    }

    /* 将描述符写入 CPU 的 IDTR，使异常向量 0 到 31 从此由我们的 IDT 处理。 */
    idt_load(&pointer);
}

/* 这是所有汇编异常入口调用的 C 函数；当前任意异常都显示信息后停机。 */
void exception_handler(uint32_t vector, uint32_t error_code) {
    /* 在终端中留下通用异常标题。 */
    terminal_write("\nEXCEPTION\n", COLOR_LIGHT_RED);

    /* 输出当前异常向量编号。 */
    terminal_write("vector: ", COLOR_LIGHT_RED);

    /* 以十六进制形式输出当前异常向量编号。 */
    terminal_write_hex32(vector, COLOR_LIGHT_RED);

    /* 在输出错误码前换行。 */
    terminal_write("\nerror:  ", COLOR_LIGHT_RED);

    /* 以十六进制形式输出 CPU 或异常入口提供的错误码。 */
    terminal_write_hex32(error_code, COLOR_LIGHT_RED);

    /* 异常诊断信息输出完成后换行。 */
    terminal_write("\n", COLOR_LIGHT_RED);

    /* 因异常现场尚未实现恢复逻辑，永久停止 CPU。 */
    for (;;) {
        /* 让 CPU 休眠，避免在异常状态下继续执行未知代码。 */
        __asm__ volatile ("hlt");
    }
}

/* 用 x86 的 div 指令故意触发向量 0“除以零”异常，以验证 IDT。 */
static void trigger_divide_by_zero(void) {
    /* 将 EDX 设为 0，使它成为除法指令的除数。 */
    __asm__ volatile ("xor %%edx, %%edx" : : : "edx");

    /* 将 EAX 设为 1，作为被除数的一部分。 */
    __asm__ volatile ("mov $1, %%eax" : : : "eax");

    /* 执行 32 位无符号除法；除数 EDX 为 0，因此 CPU 触发异常向量 0。 */
    __asm__ volatile ("div %%edx" : : : "eax", "edx");
}

/* 将 kernel_main 放入专用的 .text.entry 段，确保它位于内核映像开头。 */
__attribute__((section(".text.entry")))

/* 这是 Stage 2 进入 C 内核时调用的第一个函数。 */
void kernel_main(void) {
    /* 清除 BIOS、Stage 1 和 Stage 2 留在屏幕上的旧内容。 */
    terminal_clear();

    /* 用绿色显示内核已经开始运行，并让字符串末尾自动换行。 */
    terminal_write("C kernel is running!\n", COLOR_LIGHT_GREEN);

    /* 额外输出一个换行符，在两条状态信息之间留下空行。 */
    terminal_write("\n", COLOR_LIGHT_GREY);

    /* 用浅灰色说明当前实现的功能，并在结尾换行。 */
    terminal_write("Stage 3: VGA terminal driver\n", COLOR_LIGHT_GREY);

    /* 输出一条说明，提示终端现在可以处理滚屏。 */
    terminal_write("Newline and scrolling are ready.\n", COLOR_LIGHT_GREY);

    /* 建立并加载异常 IDT，使 CPU 能找到向量 0 到 31 的异常处理函数。 */
    idt_initialize();

    /* 提示接下来会主动触发一个受控异常。 */
    terminal_write("IDT loaded; triggering divide by zero...\n", COLOR_LIGHT_GREY);

    /* 故意执行除以零，以验证 CPU 是否跳进我们的异常入口。 */
    trigger_divide_by_zero();

    /* 内核不能返回到不存在的操作系统，因此永久停在这里。 */
    for (;;) {
        /* 让 CPU 暂停，直到不可屏蔽事件唤醒它。 */
        __asm__ volatile ("hlt");
    }
}
