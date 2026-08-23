/* 定义 8 位无符号整数类型，避免依赖 C 标准库头文件。 */
typedef unsigned char uint8_t;

/* 定义 16 位无符号整数类型，用来写 VGA 字符单元。 */
typedef unsigned short uint16_t;

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

/* 将一个 ASCII 字符与一个 VGA 颜色属性合成为一个 16 位字符单元。 */
static uint16_t vga_entry(char character, uint8_t color) {
    /* 字符放在低 8 位，颜色属性放在高 8 位。 */
    return (uint16_t)character | ((uint16_t)color << 8);
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
}

/* 从指定行的第 0 列开始，以给定颜色输出一个以 0 结尾的字符串。 */
static void terminal_write(const char *text, uint16_t row, uint8_t color) {
    /* 从当前行的第 0 列开始输出。 */
    uint16_t column = 0;

    /* 逐字符循环，直到遇到 C 字符串的结束标记 0。 */
    while (text[column] != '\0') {
        /* 计算当前字符在 80 × 25 VGA 显存中的线性编号。 */
        const uint16_t index = row * VGA_WIDTH + column;

        /* 将当前字符和颜色写入 VGA 显存。 */
        ((volatile uint16_t *)VGA_BUFFER)[index] = vga_entry(text[column], color);

        /* 移动到字符串与屏幕的下一列。 */
        ++column;
    }
}

/* 这是 Stage 2 进入 C 内核时调用的第一个函数。 */
void kernel_main(void) {
    /* 清除 BIOS、Stage 1 和 Stage 2 留在屏幕上的旧内容。 */
    terminal_clear();

    /* 用绿色显示内核已经开始运行。 */
    terminal_write("C kernel is running!", 2, COLOR_LIGHT_GREEN);

    /* 用浅灰色说明当前实现的功能。 */
    terminal_write("Stage 3: VGA terminal driver", 4, COLOR_LIGHT_GREY);

    /* 内核不能返回到不存在的操作系统，因此永久停在这里。 */
    for (;;) {
        /* 让 CPU 暂停，直到不可屏蔽事件唤醒它。 */
        __asm__ volatile ("hlt");
    }
}
