#pragma once // 헤더 파일이 여러 번 포함되는 것을 방지하는 전처리기 지시문

typedef int bool;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef uint32_t size_t;
typedef uint32_t paddr_t;
typedef uint32_t vaddr_t;

#define true  1
#define false 0
#define NULL  ((void *) 0)
#define align_up(value, align)   __builtin_align_up(value, align)
#define is_aligned(value, align) __builtin_is_aligned(value, align)
#define offsetof(type, member)   __builtin_offsetof(type, member)
#define va_list  __builtin_va_list // 가변 인자 목록을 다루기 위한 타입 정의
#define va_start __builtin_va_start // 가변 인자 목록 초기화 매크로
#define va_end   __builtin_va_end // 가변 인자 목록 정리 매크로
#define va_arg   __builtin_va_arg // 가변 인자에서 다음 인자 추출 매크로
#define PAGE_SIZE 4096 // 페이지 크기 정의
#define SYS_PUTCHAR 1

void *memset(void *buf, char c, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
char *strcpy(char *dst, const char *src);
int strcmp(const char *s1, const char *s2);
void printf(const char *fmt, ...);