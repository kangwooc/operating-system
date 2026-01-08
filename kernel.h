#pragma once

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
