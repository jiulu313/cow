; 这是启动磁盘的第一个扇区；BIOS 只会读取这里的 512 字节。
; BIOS 把它放入物理内存地址 0x7C00，然后从那里开始执行。

bits 16                      ; 告诉 NASM：以下指令按 16 位实模式编码。
org 0x7C00                   ; 告诉 NASM：本文件的第一个字节运行于地址 0x7C00。

start:                       ; BIOS 跳入我们的代码后执行的第一个位置。
    xor ax, ax               ; AX = 0；异或自身是得到 0 的常见短指令。
    mov ds, ax               ; 数据段 DS = 0；之后 [DS:SI] 的地址基准确定为 0。
    mov es, ax               ; 附加数据段 ES = 0；目前虽未使用，也先初始化。
    mov ss, ax               ; 栈段 SS = 0；设置栈前必须先设置它。
    mov sp, 0x7C00           ; 栈顶 SP = 0x7C00；栈向更小的地址增长。
    mov [boot_drive], dl     ; 保存 BIOS 传入的启动盘编号；int 0x13 读取时还要使用它。

    mov si, message          ; SI 指向下方字符串的第一个字符。
.print_next:                 ; 局部标签；每次循环输出一个字符。
    lodsb                    ; 把 DS:SI 的一个字节读入 AL，并令 SI 自动加 1。
    test al, al              ; 计算 AL 与 AL；AL 不变，但零标志 ZF 会反映 AL 是否为 0。
    jz .load_stage2          ; 如果 AL 是 0（字符串结尾），开始读取第 2 个扇区。

    mov ah, 0x0E             ; AH = 0x0E：选择 BIOS int 0x10 的“字符输出”功能。
    mov bh, 0x00             ; BH = 0：选择第 0 个显示页。
    mov bl, 0x07             ; BL = 7：文本属性为浅灰色（文本模式通常忽略它）。
    int 0x10                 ; 调用 BIOS 显示服务，输出 AL 中当前字符。
    jmp .print_next          ; 回到循环开头，读取并输出下一个字符。

.load_stage2:                ; 第 1 阶段显示完毕；现在从磁盘读取第 2 个扇区。
    xor ax, ax               ; AX = 0；我们要把目标内存段 ES 设为 0。
    mov es, ax               ; ES = 0；和 BX 组合成目的地址 ES:BX。
    mov bx, 0x7E00           ; BX = 0x7E00；第 2 阶段将被放在物理地址 0x7E00。
    mov ah, 0x02             ; AH = 2；选择 BIOS int 0x13 的“读取扇区”功能。
    mov al, 0x01             ; AL = 1；本次只读取一个扇区，即 512 字节。
    mov ch, 0x00             ; CH = 0；读取第 0 个柱面。
    mov cl, 0x02             ; CL = 2；读取第 2 个扇区（扇区编号从 1 开始）。
    mov dh, 0x00             ; DH = 0；读取第 0 个磁头。
    mov dl, [boot_drive]     ; DL 恢复为 BIOS 的启动盘编号，不能把它写死为软盘。
    int 0x13                 ; 调用 BIOS 磁盘服务，将第 2 扇区读到 ES:BX，即 0x7E00。
    jc .disk_error           ; 若进位标志 CF=1，说明读取失败，跳到停止代码。
    jmp 0x0000:0x7E00        ; 远跳转到刚读入的第 2 阶段代码；CS=0，IP=0x7E00。

.disk_error:                 ; BIOS 读取失败时到达这里；暂时不输出错误信息。
    cli                      ; 关闭可屏蔽中断，避免没有处理中断处理程序时发生意外。
    hlt                      ; 让 CPU 停止执行，直到出现中断。
    jmp .disk_error          ; 若 CPU 被唤醒，继续停机，不会落入后面的数据。

boot_drive db 0              ; 为启动盘编号预留 1 字节；启动时由 DL 写入这里。
message db 'Stage 1: boot sector', 13, 10, 0 ; 字符串；13、10 是换行，0 是结束标记 NUL。

times 510 - ($ - $$) db 0   ; 用 0 补齐，保证签名恰好从第 511 个字节开始。
dw 0xAA55                    ; 写入 16 位数 AA55；x86 小端序的磁盘字节为 55 AA。
