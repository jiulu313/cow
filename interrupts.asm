bits 32                                      ; 告诉 NASM：以下异常入口按 32 位保护模式指令编码。

section .text                                ; 将异常入口与公共处理代码放入可执行代码段。

extern exception_handler                     ; 声明由 kernel.c 提供的通用 C 异常处理函数。

extern timer_handler                         ; 声明由 kernel.c 提供的 PIT 定时器 IRQ0 处理函数。

extern keyboard_handler                      ; 声明由 kernel.c 提供的键盘 IRQ1 处理函数。

%macro ISR_NO_ERROR_CODE 1                   ; 定义一个“CPU 不自动压入错误码”的异常入口模板。
global isr%1                                 ; 导出当前异常向量的入口符号，例如 isr0。
isr%1:                                        ; 定义当前异常向量的入口标签。
    push dword 0                             ; 手动补一个值为 0 的错误码，使所有异常的栈布局一致。
    push dword %1                            ; 将当前异常向量号压栈。
    jmp isr_common                           ; 跳转到所有异常共享的处理代码。
%endmacro                                    ; 结束无错误码异常入口模板。

%macro ISR_WITH_ERROR_CODE 1                 ; 定义一个“CPU 已自动压入错误码”的异常入口模板。
global isr%1                                 ; 导出当前异常向量的入口符号，例如 isr14。
isr%1:                                        ; 定义当前异常向量的入口标签。
    push dword %1                            ; 仅压入异常向量号；CPU 错误码已经在栈中。
    jmp isr_common                           ; 跳转到所有异常共享的处理代码。
%endmacro                                    ; 结束带错误码异常入口模板。

ISR_NO_ERROR_CODE 0                          ; 向量 0：除以零异常。
ISR_NO_ERROR_CODE 1                          ; 向量 1：调试异常。
ISR_NO_ERROR_CODE 2                          ; 向量 2：不可屏蔽中断。
ISR_NO_ERROR_CODE 3                          ; 向量 3：断点异常。
ISR_NO_ERROR_CODE 4                          ; 向量 4：溢出异常。
ISR_NO_ERROR_CODE 5                          ; 向量 5：越界异常。
ISR_NO_ERROR_CODE 6                          ; 向量 6：无效操作码异常。
ISR_NO_ERROR_CODE 7                          ; 向量 7：设备不可用异常。
ISR_WITH_ERROR_CODE 8                        ; 向量 8：双重故障异常。
ISR_NO_ERROR_CODE 9                          ; 向量 9：协处理器段超限异常（旧式保留）。
ISR_WITH_ERROR_CODE 10                       ; 向量 10：无效 TSS 异常。
ISR_WITH_ERROR_CODE 11                       ; 向量 11：段不存在异常。
ISR_WITH_ERROR_CODE 12                       ; 向量 12：栈段故障异常。
ISR_WITH_ERROR_CODE 13                       ; 向量 13：通用保护异常。
ISR_WITH_ERROR_CODE 14                       ; 向量 14：缺页异常。
ISR_NO_ERROR_CODE 15                         ; 向量 15：保留。
ISR_NO_ERROR_CODE 16                         ; 向量 16：x87 浮点异常。
ISR_WITH_ERROR_CODE 17                       ; 向量 17：对齐检查异常。
ISR_NO_ERROR_CODE 18                         ; 向量 18：机器检查异常。
ISR_NO_ERROR_CODE 19                         ; 向量 19：SIMD 浮点异常。
ISR_NO_ERROR_CODE 20                         ; 向量 20：虚拟化异常。
ISR_WITH_ERROR_CODE 21                       ; 向量 21：控制保护异常。
ISR_NO_ERROR_CODE 22                         ; 向量 22：保留。
ISR_NO_ERROR_CODE 23                         ; 向量 23：保留。
ISR_NO_ERROR_CODE 24                         ; 向量 24：保留。
ISR_NO_ERROR_CODE 25                         ; 向量 25：保留。
ISR_NO_ERROR_CODE 26                         ; 向量 26：保留。
ISR_NO_ERROR_CODE 27                         ; 向量 27：保留。
ISR_NO_ERROR_CODE 28                         ; 向量 28：Hypervisor 注入异常。
ISR_WITH_ERROR_CODE 29                       ; 向量 29：VMM 通信异常。
ISR_WITH_ERROR_CODE 30                       ; 向量 30：安全异常。
ISR_NO_ERROR_CODE 31                         ; 向量 31：保留。

isr_common:                                   ; 所有异常入口最终都会跳到这里。
    pusha                                     ; 保存 EAX、ECX、EDX、EBX、ESP、EBP、ESI、EDI。
    push dword [esp + 36]                     ; 先按 cdecl 从右到左压入错误码参数。
    push dword [esp + 36]                     ; 再压入异常向量号参数；前一次 push 后偏移已自动调整。
    call exception_handler                    ; 调用 C 函数 exception_handler(vector, error_code)。
    add esp, 8                                ; 清理传给 C 函数的两个 32 位参数。
    popa                                      ; 恢复异常发生前保存的通用寄存器。
    add esp, 8                                ; 移除规范化后的异常向量号与错误码。
    iret                                      ; 若 C 函数返回，使用中断返回指令恢复 EIP、CS 和 EFLAGS。

global irq0                                  ; 导出 PIC 重映射后位于 IDT 向量 32 的定时器 IRQ0 入口。

irq0:                                         ; PIT 定时器触发 IRQ0 时，CPU 通过 IDT[32] 跳入这里。
    pusha                                     ; 保存通用寄存器，保护被中断的 C 代码现场。
    call timer_handler                        ; 调用 C 定时器处理函数，更新时钟计数并按需输出信息。
    popa                                      ; 恢复中断发生前的通用寄存器。
    mov al, 0x20                              ; 准备 PIC 的“中断结束命令”EOI。
    out 0x20, al                              ; 通知主 PIC：IRQ0 已处理完成，可以继续发送下一次中断。
    iret                                      ; 从硬件中断返回，恢复被中断代码的 EIP、CS 和 EFLAGS。

global irq1                                  ; 导出 PIC 重映射后位于 IDT 向量 33 的键盘 IRQ1 入口。

irq1:                                         ; 键盘控制器触发 IRQ1 时，CPU 通过 IDT[33] 跳入这里。
    pusha                                     ; 保存通用寄存器，保护被中断的 C 代码现场。
    call keyboard_handler                     ; 调用 C 键盘处理函数，读取扫描码并显示可识别字符。
    popa                                      ; 恢复中断发生前的通用寄存器。
    mov al, 0x20                              ; 准备 PIC 的“中断结束命令”EOI。
    out 0x20, al                              ; 通知主 PIC：IRQ1 已处理完成，可以继续接收后续按键。
    iret                                      ; 从硬件中断返回，恢复被中断代码的 EIP、CS 和 EFLAGS。

section .rodata                              ; 将异常入口地址表放入只读数据段。

global exception_stub_table                  ; 导出表名称，供 C 代码取得所有异常入口地址。

exception_stub_table:                        ; 表中第 n 项保存 isrn 的地址。
    dd isr0, isr1, isr2, isr3                ; 写入异常向量 0 到 3 的入口地址。
    dd isr4, isr5, isr6, isr7                ; 写入异常向量 4 到 7 的入口地址。
    dd isr8, isr9, isr10, isr11              ; 写入异常向量 8 到 11 的入口地址。
    dd isr12, isr13, isr14, isr15            ; 写入异常向量 12 到 15 的入口地址。
    dd isr16, isr17, isr18, isr19            ; 写入异常向量 16 到 19 的入口地址。
    dd isr20, isr21, isr22, isr23            ; 写入异常向量 20 到 23 的入口地址。
    dd isr24, isr25, isr26, isr27            ; 写入异常向量 24 到 27 的入口地址。
    dd isr28, isr29, isr30, isr31            ; 写入异常向量 28 到 31 的入口地址。
