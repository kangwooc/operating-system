#include "kernel.h"
#include "common.h"

// 링커 스크립트에 정의한 심볼(__bss, __bss_end, __stack_top)을 extern char __bss[], ...;형태로 선언
extern char __bss[], __bss_end[], __stack_top[];

struct sbiret sbi_call(long arg0, long arg1, long arg2, long arg3, long arg4,
                       long arg5, long fid, long eid) {
    register long a0 __asm__("a0") = arg0;
    register long a1 __asm__("a1") = arg1;
    register long a2 __asm__("a2") = arg2;
    register long a3 __asm__("a3") = arg3;
    register long a4 __asm__("a4") = arg4;
    register long a5 __asm__("a5") = arg5;
    register long a6 __asm__("a6") = fid;
    register long a7 __asm__("a7") = eid;
    
    // RISC-V 어셈블리어에서 ecall 명령어는 환경 호출을 트리거하는 데 사용됩니다.
    // 환경 호출은 운영 체제 커널이나 하이퍼바이저와 같은 특권 모드에서 실행되는 소프트웨어 구성 요소에 서비스를 요청하는 메커니즘입니다.
    __asm__ __volatile__("ecall"
                         : "=r"(a0), "=r"(a1)
                         : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5),
                           "r"(a6), "r"(a7)
                         : "memory");
    return (struct sbiret){.error = a0, .value = a1};
}

void putchar(char ch) {
    sbi_call(ch, 0, 0, 0, 0, 0, 0, 1 /* Console Putchar */);
}

// 커널 진입점(entry point) 함수
// 이 함수는 어셈블리어로 작성되어 있으며, 트랩이 발생했을 때 호출됩니다.
// sscratch 레지스터를 임시 저장소로 이용해 예외 발생 시점의 스택 포인터를 저장해두고, 나중에 복구합니다.
// 커널에서는 부동소수점(FPU) 레지스터를 사용하지 않으므로(일반적으로 쓰레드 스위칭 시에만 저장/복원), 여기서는 저장하지 않았습니다.
// 스택 포인터(sp) 값을 a0 레지스터에 넘겨 handle_trap 함수를 C 코드로 호출합니다. 이때 sp가 가리키는 곳에는 조금 뒤에 소개할 trap_frame 구조체와 동일한 형태로 레지스터들이 저장되어 있습니다.
// __attribute__((aligned(4)))는 함수 시작 주소를 4바이트 경계에 맞추기 위함입니다. stvec 레지스터는 예외 핸들러 주소뿐 아니라 하위 2비트를 모드 정보 플래그로 사용하기 때문에, 핸들러 주소가 4바이트 정렬이 되어 있어야 합니다.
__attribute__((naked))
__attribute__((aligned(4)))
void kernel_entry(void) {
    __asm__ __volatile__(
        "csrw sscratch, sp\n"
        "addi sp, sp, -4 * 31\n"
        "sw ra,  4 * 0(sp)\n"
        "sw gp,  4 * 1(sp)\n"
        "sw tp,  4 * 2(sp)\n"
        "sw t0,  4 * 3(sp)\n"
        "sw t1,  4 * 4(sp)\n"
        "sw t2,  4 * 5(sp)\n"
        "sw t3,  4 * 6(sp)\n"
        "sw t4,  4 * 7(sp)\n"
        "sw t5,  4 * 8(sp)\n"
        "sw t6,  4 * 9(sp)\n"
        "sw a0,  4 * 10(sp)\n"
        "sw a1,  4 * 11(sp)\n"
        "sw a2,  4 * 12(sp)\n"
        "sw a3,  4 * 13(sp)\n"
        "sw a4,  4 * 14(sp)\n"
        "sw a5,  4 * 15(sp)\n"
        "sw a6,  4 * 16(sp)\n"
        "sw a7,  4 * 17(sp)\n"
        "sw s0,  4 * 18(sp)\n"
        "sw s1,  4 * 19(sp)\n"
        "sw s2,  4 * 20(sp)\n"
        "sw s3,  4 * 21(sp)\n"
        "sw s4,  4 * 22(sp)\n"
        "sw s5,  4 * 23(sp)\n"
        "sw s6,  4 * 24(sp)\n"
        "sw s7,  4 * 25(sp)\n"
        "sw s8,  4 * 26(sp)\n"
        "sw s9,  4 * 27(sp)\n"
        "sw s10, 4 * 28(sp)\n"
        "sw s11, 4 * 29(sp)\n"

        "csrr a0, sscratch\n"
        "sw a0, 4 * 30(sp)\n"

        "mv a0, sp\n"
        "call handle_trap\n"

        "lw ra,  4 * 0(sp)\n"
        "lw gp,  4 * 1(sp)\n"
        "lw tp,  4 * 2(sp)\n"
        "lw t0,  4 * 3(sp)\n"
        "lw t1,  4 * 4(sp)\n"
        "lw t2,  4 * 5(sp)\n"
        "lw t3,  4 * 6(sp)\n"
        "lw t4,  4 * 7(sp)\n"
        "lw t5,  4 * 8(sp)\n"
        "lw t6,  4 * 9(sp)\n"
        "lw a0,  4 * 10(sp)\n"
        "lw a1,  4 * 11(sp)\n"
        "lw a2,  4 * 12(sp)\n"
        "lw a3,  4 * 13(sp)\n"
        "lw a4,  4 * 14(sp)\n"
        "lw a5,  4 * 15(sp)\n"
        "lw a6,  4 * 16(sp)\n"
        "lw a7,  4 * 17(sp)\n"
        "lw s0,  4 * 18(sp)\n"
        "lw s1,  4 * 19(sp)\n"
        "lw s2,  4 * 20(sp)\n"
        "lw s3,  4 * 21(sp)\n"
        "lw s4,  4 * 22(sp)\n"
        "lw s5,  4 * 23(sp)\n"
        "lw s6,  4 * 24(sp)\n"
        "lw s7,  4 * 25(sp)\n"
        "lw s8,  4 * 26(sp)\n"
        "lw s9,  4 * 27(sp)\n"
        "lw s10, 4 * 28(sp)\n"
        "lw s11, 4 * 29(sp)\n"
        "lw sp,  4 * 30(sp)\n"
        "sret\n"
    );
}

void handle_trap(struct trap_frame *f) {
    uint32_t scause = READ_CSR(scause);
    uint32_t stval = READ_CSR(stval);
    uint32_t user_pc = READ_CSR(sepc);

    PANIC("unexpected trap scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);
}

void kernel_main(void) {    
    memset(__bss, 0, (size_t) __bss_end - (size_t) __bss);

    WRITE_CSR(stvec, (uint32_t) kernel_entry); // new

    // stvec를 설정한 뒤, unimp 명령어(illegal instruction 로 간주됨)를 실행해 일부러 예외를 일으키는 코드입니다.
    // unimp 는 “의사(pseudo) 명령어”.
    __asm__ __volatile__("unimp"); // new
}

// __attribute__는 함수나 변수에 특별한 속성을 부여하는 GCC 컴파일러의 확장 기능입니다.
// section(".text.boot")는 이 함수를 "text" 세그먼트의 "boot" 섹션에 배치하도록 지정합니다.
// naked는 함수의 시작과 끝에 자동으로 생성되는 프로로그(prologue)와 에필로그(epilogue)를 생략함 
// 프롤로그는 함수의 영역을 설정해줌
// 에필로그는 함수가 끝날 때 호출자에게 제어를 반환하기 전에 수행되는 정리 작업
__attribute__((section(".text.boot")))
__attribute__((naked))
void boot(void) {
    // 스택 포인터를 __stack_top으로 설정
    // __volatile__는 컴파일러가 이 명령어를 최적화하지 않도록 지시
    // RISC-V 아키텍처에서는 스택이 내려가는 방향으로 성장하므로, 스택의 최상위(끝) 주소를 설정해야 합니다.
    __asm__ __volatile__(
        "mv sp, %[stack_top]\n" // 스택 포인터 설정
        "j kernel_main\n" // kernel_main 함수로 점프
        :
        : [stack_top] "r" (__stack_top) // Stack top 주소를 레지스터에 로드
    );
}


// Hello World 메시지가 화면에 출력되는 과정은 다음과 같습니다:

// SBI 호출 시, 문자는 다음과 같이 표시됩니다:

// 커널에서 ecall 명령어를 실행합니다. CPU는 OpenSBI가 부팅 시점에 설정해둔 M-Mode 트랩 핸들러(mtvec 레지스터)로 점프합니다.
// 레지스터를 저장한 뒤, C로 작성된 트랩 핸들러가 호출됩니다.
// eid에 따라, 해당 SBI 기능을 처리하는 함수가 실행됩니다.
// 8250 UART용 디바이스 드라이버가 문자를 QEMU에 전송합니다.
// QEMU의 8250 UART 에뮬레이션이 이 문자를 받아서 표준 출력으로 보냅니다.
// 터미널 에뮬레이터가 문자를 화면에 표시합니다.
// 즉 Console Putchar 함수를 부르는 것은 어떤 마법이 아니라, 단지 OpenSBI에 구현된 디바이스 드라이버를 호출하는 것일 뿐입니다!
