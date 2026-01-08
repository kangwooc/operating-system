#pragma once // 헤더 파일이 여러 번 포함되는 것을 방지하는 전처리기 지시문

#define va_list  __builtin_va_list // 가변 인자 목록을 다루기 위한 타입 정의
#define va_start __builtin_va_start // 가변 인자 목록 초기화 매크로
#define va_end   __builtin_va_end // 가변 인자 목록 정리 매크로
#define va_arg   __builtin_va_arg // 가변 인자에서 다음 인자 추출 매크로

void printf(const char *fmt, ...);