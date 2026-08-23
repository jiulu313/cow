; 这是第 2 阶段代码；第 1 阶段会从磁盘第 2 扇区读取它。
; 它不会被 BIOS 直接加载，而是被放到物理内存地址 0x7E00。

bits 16                      ; 告诉 NASM：以下指令按 16 位实模式编码。
org 0x7E00                   ; 告诉 NASM：本文件运行时的起始地址是 0x7E00。

start_stage2:                ; 第 1 阶段执行 jmp 0x0000:0x7E00 后从这里开始运行。
    xor ax, ax               ; AX = 0；准备将数据段初始化为 0。
    mov ds, ax               ; DS = 0；让 DS:SI 能正确指向本文件中的字符串。
    mov si, message          ; SI 保存 message 的地址，指向待输出字符串的第一个字符。

.print_next:                 ; 循环标签：每次循环输出一个字符。
    lodsb                    ; 将 DS:SI 的一个字节读入 AL，并把 SI 自动加 1。
    test al, al              ; 检查 AL 是否为 0；AL 本身不会被改变。
    jz .halt                 ; 如果 AL 为 0，说明到达字符串末尾，跳转到停止代码。
    mov ah, 0x0E             ; AH = 0x0E，选择 BIOS int 0x10 的字符输出功能。
    mov bh, 0x00             ; BH = 0，选择第 0 个显示页。
    mov bl, 0x07             ; BL = 7，指定浅灰色文本属性。
    int 0x10                 ; 调用 BIOS 显示服务，输出 AL 中的当前字符。
    jmp .print_next          ; 返回循环开头，读取下一个字符。

.halt:                       ; 字符串显示结束后到达这里。
    cli                      ; 关闭可屏蔽中断，避免没有中断处理程序时发生意外。
    hlt                      ; 停止 CPU 执行。
    jmp .halt                ; 若 CPU 被唤醒，继续停止，不会执行到后方数据。

message db 'Stage 2: loaded from disk!', 13, 10, 0 ; 13、10 是换行，0 是字符串结束标记。

times 512 - ($ - $$) db 0   ; 用 0 填满本扇区，使本文件严格等于 512 字节。
