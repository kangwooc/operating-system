#pragma once
#include "common.h"

// 커널 관련 정의와 함수 선언을 포함하는 헤더 파일
// 이 파일은 여러 번 포함되는 것을 방지하기 위해 #pragma once 지시문을 사용합니다.
// do-while(0) 구조 : while (0)이므로 이 루프는 한 번만 실행됩니다.
// 그리고 각 줄 끝의 역슬래시(\)에 주의하세요. 매크로가 여러 줄에 걸쳐서 정의되어 있어도, 매크로 전개 시에는 줄바꿈이 무시됩니다.
// __VA_ARGS__: 매개변수가 가변적인 매크로를 정의할 때 유용한 확장 기능
#define PANIC(fmt, ...)                                                        \
    do {                                                                       \
        printf("PANIC: %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);  \
        while (1) {}                                                           \
    } while (0)

struct sbiret {
    long error;
    long value;
};

struct trap_frame {
    uint32_t ra;
    uint32_t gp;
    uint32_t tp;
    uint32_t t0;
    uint32_t t1;
    uint32_t t2;
    uint32_t t3;
    uint32_t t4;
    uint32_t t5;
    uint32_t t6;
    uint32_t a0;
    uint32_t a1;
    uint32_t a2;
    uint32_t a3;
    uint32_t a4;
    uint32_t a5;
    uint32_t a6;
    uint32_t a7;
    uint32_t s0;
    uint32_t s1;
    uint32_t s2;
    uint32_t s3;
    uint32_t s4;
    uint32_t s5;
    uint32_t s6;
    uint32_t s7;
    uint32_t s8;
    uint32_t s9;
    uint32_t s10;
    uint32_t s11;
    uint32_t sp;
} __attribute__((packed));

#define READ_CSR(reg)                                                          \
    ({                                                                         \
        unsigned long __tmp;                                                   \
        __asm__ __volatile__("csrr %0, " #reg : "=r"(__tmp));                  \
        __tmp;                                                                 \
    })

#define WRITE_CSR(reg, value)                                                  \
    do {                                                                       \
        uint32_t __tmp = (value);                                              \
        __asm__ __volatile__("csrw " #reg ", %0" ::"r"(__tmp));                \
    } while (0)

#define SATP_SV32 (1u << 31)
#define PAGE_V    (1 << 0)   // "Valid" 비트 (엔트리가 유효함을 의미)
#define PAGE_R    (1 << 1)   // 읽기 가능
#define PAGE_W    (1 << 2)   // 쓰기 가능
#define PAGE_X    (1 << 3)   // 실행 가능
#define PAGE_U    (1 << 4)   // 사용자 모드 접근 가능

struct process {
    int pid;             // 프로세스 ID
    int state;           // 프로세스 상태: PROC_UNUSED 또는 PROC_RUNNABLE
    vaddr_t sp;          // 스택 포인터
    uint32_t *page_table;
    uint8_t stack[8192]; // 커널 스택
};

// 애플리케이션 이미지의 기본 가상 주소입니다. 이는 `user.ld`에 정의된 시작 주소와 일치해야 합니다.
#define USER_BASE 0x1000000

#define SSTATUS_SPIE (1 << 5)
#define SCAUSE_ECALL 8
#define PROC_EXITED 2
