typedef unsigned char uint8_t; // 8-bit unsigned integer 기존 자료형(int, float, struct 등)에 새로운 별칭(alias)을 붙여주는 키워드
typedef unsigned int uint32_t; // 32-bit unsigned integer
typedef uint32_t size_t; // Size type

// 링커 스크립트에 정의한 심볼(__bss, __bss_end, __stack_top)을 extern char __bss[], ...;형태로 선언
extern char __bss[], __bss_end[], __stack_top[];

void *memset(void *buf, char c, size_t n) {
    uint8_t *p = (uint8_t *) buf;
    while (n--) {
        *p++ = c;
    }
    return buf;
}

void kernel_main() {
    // 여러 환경에서 확실히 동작하게 하려면 이렇게 수동으로 초기화하는 것이 안전합니다
    memset(__bss, 0, (size_t) __bss_end - (size_t) __bss);

    for (;;);
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
