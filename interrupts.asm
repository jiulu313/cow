bits 32                                      ; 告诉 NASM：以下异常入口按 32 位保护模式指令编码。

section .text                                ; 将异常入口放入可执行代码段。

global isr_divide_by_zero                    ; 让 C 链接器能够把这个异常入口名称导出。

extern divide_by_zero_handler                ; 声明由 kernel.c 提供的 C 异常处理函数。

isr_divide_by_zero:                          ; CPU 遇到除以零异常（向量 0）时跳入这里。
    pusha                                    ; 保存所有通用寄存器，保护异常发生时的寄存器现场。
    call divide_by_zero_handler              ; 调用 C 函数显示异常信息；该函数不会返回。

.halt:                                       ; 即使 C 函数意外返回，也不能执行 iret，因为现场恢复尚未实现。
    cli                                      ; 关闭可屏蔽中断。
    hlt                                      ; 停止 CPU 执行。
    jmp .halt                                ; 若 CPU 被唤醒，继续停在这里。
