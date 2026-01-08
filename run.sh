#!/bin/bash
set -xue # -e: 오류 발생 시 스크립트 종료, -u: 정의되지 않은 변수 사용 시 오류 발생, -x: 실행되는 명령어 출력

QEMU=qemu-system-riscv32 # QEMU 실행 파일 지정

# clang 경로와 컴파일 옵션
CC=/opt/homebrew/opt/llvm/bin/clang  
CFLAGS="-std=c11 -O2 -g3 -Wall -Wextra --target=riscv32-unknown-elf -fuse-ld=lld -fno-stack-protector -ffreestanding -nostdlib"

# 커널 빌드
$CC $CFLAGS -Wl,-Tkernel.ld -Wl,-Map=kernel.map -o kernel.elf \
    kernel.c common.c

# QEMU 실행, RISC-V 32비트 가상 머신 생성, 기본 BIOS 사용, 그래픽 출력 비활성화, 시리얼 출력을 표준 입출력으로 설정, 재부팅 방지
$QEMU -machine virt -bios default -nographic -serial mon:stdio --no-reboot -kernel kernel.elf

