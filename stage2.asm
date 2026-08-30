; 这是第 2 阶段代码；第 1 阶段会从磁盘第 2 扇区读取它。
; 它不会被 BIOS 直接加载，而是被放到物理内存地址 0x7E00。

bits 16                      ; 告诉 NASM：以下指令按 16 位实模式编码。
org 0x7E00                   ; 告诉 NASM：本文件运行时的起始地址是 0x7E00。

start_stage2:                ; 第 1 阶段执行 jmp 0x0000:0x7E00 后从这里开始运行。
    xor ax, ax               ; AX = 0；准备将数据段初始化为 0。
    mov ds, ax               ; DS = 0；让 DS:SI 能正确指向本文件中的字符串。
    mov [boot_drive], dl     ; 保存启动盘编号；输出文字后还要用它读取 C 内核。
    mov si, message          ; SI 保存 message 的地址，指向待输出字符串的第一个字符。

.print_next:                 ; 循环标签：每次循环输出一个字符。
    lodsb                    ; 将 DS:SI 的一个字节读入 AL，并把 SI 自动加 1。
    test al, al              ; 检查 AL 是否为 0；AL 本身不会被改变。
    jz .load_kernel          ; 如果 AL 为 0，说明字符串结束，先从磁盘读取 C 内核。
    mov ah, 0x0E             ; AH = 0x0E，选择 BIOS int 0x10 的字符输出功能。
    mov bh, 0x00             ; BH = 0，选择第 0 个显示页。
    mov bl, 0x07             ; BL = 7，指定浅灰色文本属性。
    int 0x10                 ; 调用 BIOS 显示服务，输出 AL 中的当前字符。
    jmp .print_next          ; 返回循环开头，读取下一个字符。

.load_kernel:                ; 开始读取磁盘第 3 扇区中的 C 内核。
    mov ax, 0x1000           ; AX = 0x1000；该段的物理起始地址是 0x1000 × 16 = 0x10000。
    mov es, ax               ; ES = 0x1000；读取目标地址的段部分。
    xor bx, bx               ; BX = 0；读取目标地址 ES:BX 就是 0x1000:0x0000，即 0x10000。
    mov ah, 0x02             ; AH = 2；选择 BIOS int 0x13 的“读取扇区”功能。
    mov al, 0x08             ; AL = 8；读取第 3 到第 10 扇区，为内核代码和 256 项 IDT 预留 4 KiB 空间。
    mov ch, 0x00             ; CH = 0；读取第 0 个柱面。
    mov cl, 0x03             ; CL = 3；读取第 3 个扇区，前两个扇区分别是 Stage 1 和 Stage 2。
    mov dh, 0x00             ; DH = 0；读取第 0 个磁头。
    mov dl, [boot_drive]     ; DL 恢复为 BIOS 提供的启动盘编号。
    int 0x13                 ; 将第 3 扇区读入内存地址 0x10000。
    jc .disk_error           ; 若进位标志 CF=1，说明读取失败，跳到停止代码。
    jmp .enter_protected_mode ; 读取成功后，开始切换到保护模式。

.disk_error:                 ; 读取 C 内核失败时到达这里。
    cli                      ; 关闭可屏蔽中断。
    hlt                      ; 停止 CPU 执行。
    jmp .disk_error          ; 若 CPU 被唤醒，继续停止。

.enter_protected_mode:       ; 字符串显示完成；以下代码将 CPU 切换为 32 位保护模式。
    cli                      ; 关闭可屏蔽中断；我们尚未建立保护模式下的中断表 IDT。
    lgdt [gdt_descriptor]    ; 将 GDT 描述符载入 GDTR 寄存器，使 CPU 知道 GDT 在哪里。
    mov eax, cr0             ; 从控制寄存器 CR0 读取当前模式控制位到 EAX。
    or eax, 0x00000001       ; 把 CR0 的第 0 位 PE 置为 1，表示请求进入保护模式。
    mov cr0, eax             ; 将修改后的值写回 CR0；CPU 此刻已经处于保护模式。
    jmp CODE_SEG:protected_mode_start ; 远跳转：加载新的代码段选择子，并刷新 CPU 的指令队列。

bits 32                      ; 告诉 NASM：从这里开始按 32 位保护模式指令编码。

protected_mode_start:        ; 远跳转完成后，CPU 会在这个 32 位代码入口继续执行。
    mov ax, DATA_SEG          ; AX = 数据段选择子 0x10；段寄存器不能直接写立即数。
    mov ds, ax                ; DS 使用 32 位平坦数据段。
    mov es, ax                ; ES 使用 32 位平坦数据段。
    mov fs, ax                ; FS 使用 32 位平坦数据段。
    mov gs, ax                ; GS 使用 32 位平坦数据段。
    mov ss, ax                ; SS 使用 32 位平坦数据段。
    mov esp, 0x90000          ; 将 32 位栈顶放在 0x90000，远离当前引导代码。
    mov word [0xB8280], 0x0A50 ; 往 VGA 第 5 行第 1 列写入绿色字母 P。
    mov word [0xB8282], 0x0A4D ; 往 VGA 第 5 行第 2 列写入绿色字母 M。
    call KERNEL_ENTRY         ; 调用已加载到 0x10000 的 C 函数 kernel_main。

.halt:                        ; 显示 PM 后到达这里。
    cli                       ; 继续保持中断关闭状态。
    hlt                       ; 停止 CPU 执行。
    jmp .halt                 ; 如果 CPU 被唤醒，继续停止。

message db 'Stage 2: loaded from disk!', 13, 10, 0 ; 13、10 是换行，0 是字符串结束标记。
boot_drive db 0               ; 为启动盘编号预留 1 字节；启动时由 DL 写入这里。

align 8, db 0                ; 将 GDT 起始地址按 8 字节对齐；每个 GDT 描述符正好也是 8 字节。
gdt_start:                   ; GDT 的起始标签。
gdt_null:                    ; GDT 的第 0 项必须是空描述符。
    dq 0                     ; 写入 8 个值为 0 的字节，形成空描述符。
gdt_code:                    ; GDT 的第 1 项：覆盖整个 4 GiB 地址空间的代码段。
    dw 0xFFFF                ; 段界限的低 16 位。
    dw 0x0000                ; 段基址的低 16 位。
    db 0x00                  ; 段基址的中间 8 位。
    db 10011010b             ; 访问字节：存在、特权级 0、可执行代码段、可读。
    db 11001111b             ; 高 4 位是段界限；粒度为 4 KiB，默认操作数大小为 32 位。
    db 0x00                  ; 段基址的高 8 位。
gdt_data:                    ; GDT 的第 2 项：覆盖整个 4 GiB 地址空间的数据段。
    dw 0xFFFF                ; 段界限的低 16 位。
    dw 0x0000                ; 段基址的低 16 位。
    db 0x00                  ; 段基址的中间 8 位。
    db 10010010b             ; 访问字节：存在、特权级 0、可读写数据段。
    db 11001111b             ; 高 4 位是段界限；粒度为 4 KiB，默认操作数大小为 32 位。
    db 0x00                  ; 段基址的高 8 位。
gdt_end:                     ; GDT 的结束标签，用于计算 GDT 的总长度。

gdt_descriptor:              ; lgdt 指令需要的 6 字节 GDT 描述符。
    dw gdt_end - gdt_start - 1 ; 写入 GDT 界限，即总字节数减 1。
    dd gdt_start             ; 写入 GDT 在内存中的线性地址。

CODE_SEG equ gdt_code - gdt_start ; 代码段在 GDT 中的偏移为 8，即代码段选择子 0x08。
DATA_SEG equ gdt_data - gdt_start ; 数据段在 GDT 中的偏移为 16，即数据段选择子 0x10。
KERNEL_ENTRY equ 0x10000      ; C 内核的链接地址，也是 BIOS int 0x13 读取它的物理地址。

times 512 - ($ - $$) db 0   ; 用 0 填满本扇区，使本文件严格等于 512 字节。
