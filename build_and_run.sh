#!/usr/bin/env bash

# 遇到任何命令执行失败时立即退出，避免继续启动一个不完整的镜像。
set -euo pipefail

# 确保构建产物目录存在。
mkdir -p build

# 将第 1 阶段汇编为原始二进制；结果必须是带有 55 AA 签名的 512 字节启动扇区。
nasm -f bin booter_sector.asm -o build/boot_sector.bin

# 将第 2 阶段汇编为原始二进制；结果必须是 512 字节。
nasm -f bin stage2.asm -o build/stage2.bin

# 将 C 内核编译为 32 位、无标准库依赖且以体积为优先的目标文件。
gcc -m32 -Os -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -c kernel.c -o build/kernel.o

# 按 linker.ld 指定的运行地址 0x10000 链接 C 内核，生成 32 位 ELF 文件。
ld -m elf_i386 -T linker.ld -o build/kernel.elf build/kernel.o

# 去掉 ELF 文件头，只保留将被读入第 3 扇区的原始机器码。
objcopy -O binary build/kernel.elf build/kernel.bin

# 读取原始 C 内核的实际大小，避免它超过当前只读取一个扇区的限制。
kernel_size=$(stat -c '%s' build/kernel.bin)

# 如果 C 内核大于 512 字节，停止构建，防止镜像中的数据被截断。
if [ "$kernel_size" -gt 512 ]; then
    echo "错误：kernel.bin 为 ${kernel_size} 字节，当前第 3 扇区最多只能容纳 512 字节。" >&2
    exit 1
fi

# 将不足 512 字节的 C 内核末尾补 0，使它恰好占满第 3 个磁盘扇区。
truncate -s 512 build/kernel.bin

# 按磁盘扇区顺序拼接：第 1 扇区是启动程序，第 2 扇区是加载器，第 3 扇区是 C 内核。
cat build/boot_sector.bin build/stage2.bin build/kernel.bin > build/cow.img

# 显示最终镜像大小；当前三个扇区应为 1536 字节。
stat -c '%s bytes' build/cow.img

# 将镜像作为虚拟软盘插入 QEMU，并由模拟 BIOS 启动。
qemu-system-i386 -drive format=raw,file=build/cow.img,if=floppy
