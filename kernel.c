#include "kernel.h"
#include "common.h"

typedef unsigned char uint8_t; // 8-bit unsigned integer 기존 자료형(int, float, struct 등)에 새로운 별칭(alias)을 붙여주는 키워드
typedef unsigned int uint32_t; // 32-bit unsigned integer
typedef uint32_t size_t; // Size type

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

void *memset(void *buf, char c, size_t n) {
    uint8_t *p = (uint8_t *) buf;
    while (n--) {
        *p++ = c;
    }
    return buf;
}

void kernel_main(void) {
    printf("\n\nHello %s\n", "World!");
    printf("1 + 2 = %d, %x\n", 1 + 2, 0x1234abcd);

    for (;;) {
        __asm__ __volatile__("wfi");
    }
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