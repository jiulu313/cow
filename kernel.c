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

/* 保存终端下一次输出所在的屏幕行。 */
static uint16_t terminal_row;

/* 保存终端下一次输出所在的屏幕列。 */
static uint16_t terminal_column;

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

    /* 清屏后将下一次输出位置重置到屏幕左上角。 */
    terminal_row = 0;

    /* 清屏后将下一次输出位置重置到第 0 列。 */
    terminal_column = 0;
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
    }
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

    /* 连续输出 30 行，以触发超过 25 行时的滚屏逻辑。 */
    for (uint16_t line = 0; line < 30; ++line) {
        /* 输出一行滚屏测试文本。 */
        terminal_write("scroll test line\n", COLOR_LIGHT_GREY);
    }

    /* 内核不能返回到不存在的操作系统，因此永久停在这里。 */
    for (;;) {
        /* 让 CPU 暂停，直到不可屏蔽事件唤醒它。 */
        __asm__ volatile ("hlt");
    }
}
