#include "kernel.h"
#include "common.h"

// 링커 스크립트에 정의한 심볼(__bss, __bss_end, __stack_top)을 extern char __bss[], ...;형태로 선언
extern char __bss[], __bss_end[], __stack_top[];
// 페이지 크기 정의
extern char __free_ram[], __free_ram_end[];

extern char _binary_shell_bin_start[], _binary_shell_bin_size[];

#define PROCS_MAX 8       // 최대 프로세스 개수

#define PROC_UNUSED   0   // 사용되지 않는 프로세스 구조체
#define PROC_RUNNABLE 1   // 실행 가능한(runnable) 프로세스

extern char __kernel_base[];

__attribute__((naked)) void switch_context(uint32_t *prev_sp,
                                           uint32_t *next_sp) {
    __asm__ __volatile__(
        // 현재 프로세스의 스택에 callee-saved 레지스터를 저장
        "addi sp, sp, -13 * 4\n" // 13개(4바이트씩) 레지스터 공간 확보
        "sw ra,  0  * 4(sp)\n"   // callee-saved 레지스터만 저장
        "sw s0,  1  * 4(sp)\n"
        "sw s1,  2  * 4(sp)\n"
        "sw s2,  3  * 4(sp)\n"
        "sw s3,  4  * 4(sp)\n"
        "sw s4,  5  * 4(sp)\n"
        "sw s5,  6  * 4(sp)\n"
        "sw s6,  7  * 4(sp)\n"
        "sw s7,  8  * 4(sp)\n"
        "sw s8,  9  * 4(sp)\n"
        "sw s9,  10 * 4(sp)\n"
        "sw s10, 11 * 4(sp)\n"
        "sw s11, 12 * 4(sp)\n"

        // 스택 포인터 교체
        "sw sp, (a0)\n"         // *prev_sp = sp
        "lw sp, (a1)\n"         // sp를 다음 프로세스의 값으로 변경

        // 다음 프로세스 스택에서 callee-saved 레지스터 복원
        "lw ra,  0  * 4(sp)\n"  
        "lw s0,  1  * 4(sp)\n"
        "lw s1,  2  * 4(sp)\n"
        "lw s2,  3  * 4(sp)\n"
        "lw s3,  4  * 4(sp)\n"
        "lw s4,  5  * 4(sp)\n"
        "lw s5,  6  * 4(sp)\n"
        "lw s6,  7  * 4(sp)\n"
        "lw s7,  8  * 4(sp)\n"
        "lw s8,  9  * 4(sp)\n"
        "lw s9,  10 * 4(sp)\n"
        "lw s10, 11 * 4(sp)\n"
        "lw s11, 12 * 4(sp)\n"
        "addi sp, sp, 13 * 4\n" 
        "ret\n"
    );
}

__attribute__((naked)) void user_entry(void) {
    __asm__ __volatile__(
        "csrw sepc, %[sepc]        \n"
        "csrw sstatus, %[sstatus]  \n"
        // S-Mode에서 U-Mode로의 전환은 sret 명령어를 사용하여 이루어집니다
        "sret                      \n"
        :
        : [sepc] "r" (USER_BASE),
          [sstatus] "r" (SSTATUS_SPIE | SSTATUS_SUM) // updated
    );
}

struct process procs[PROCS_MAX]; // 모든 프로세스 제어 구조체 배열

paddr_t alloc_pages(uint32_t n) {
    // next_paddr은 static 변수로 선언되었기 때문에, 로컬 변수와 달리 함수 호출이 끝나도 값을 계속 유지합니다(글로벌 변수처럼 동작).
    // next_paddr는 “새로 할당할 메모리 영역의 시작 주소"
    // next_paddr의 초깃값은 __free_ram 주소입니다. 즉, 메모리는 __free_ram 이후부터 순차적으로 할당
    // 이 알고리즘은 아주 단순하지만, 반환(메모리 해제) 이 불가능하다는 문제
    static paddr_t next_paddr = (paddr_t) __free_ram;
    paddr_t paddr = next_paddr;
    next_paddr += n * PAGE_SIZE;

    if (next_paddr > (paddr_t) __free_ram_end)
        PANIC("out of memory");

    memset((void *) paddr, 0, n * PAGE_SIZE);
    return paddr;
}

uint32_t virtio_reg_read32(unsigned offset) {
    return *((volatile uint32_t *) (VIRTIO_BLK_PADDR + offset));
}

uint64_t virtio_reg_read64(unsigned offset) {
    return *((volatile uint64_t *) (VIRTIO_BLK_PADDR + offset));
}

void virtio_reg_write32(unsigned offset, uint32_t value) {
    *((volatile uint32_t *) (VIRTIO_BLK_PADDR + offset)) = value;
}

void virtio_reg_fetch_and_or32(unsigned offset, uint32_t value) {
    virtio_reg_write32(offset, virtio_reg_read32(offset) | value);
}

struct virtio_virtq *blk_request_vq;
struct virtio_blk_req *blk_req;
paddr_t blk_req_paddr;
uint64_t blk_capacity;

struct virtio_virtq *virtq_init(unsigned index) {
    paddr_t virtq_paddr = alloc_pages(align_up(sizeof(struct virtio_virtq), PAGE_SIZE) / PAGE_SIZE);
    struct virtio_virtq *vq = (struct virtio_virtq *) virtq_paddr;
    vq->queue_index = index;
    vq->used_index = (volatile uint16_t *) &vq->used.index;
    // 큐 선택: virtqueue 인덱스를 기록 (첫 번째 큐는 0)
    virtio_reg_write32(VIRTIO_REG_QUEUE_SEL, index);
    // 큐 크기 지정: 사용할 디스크립터 개수를 기록
    virtio_reg_write32(VIRTIO_REG_QUEUE_NUM, VIRTQ_ENTRY_NUM);
    // 큐의 페이지 프레임 번호 (물리 주소가 아님!) 를 기록
    virtio_reg_write32(VIRTIO_REG_QUEUE_PFN, virtq_paddr / PAGE_SIZE);
    return vq;
}

void virtio_blk_init(void) {
    if (virtio_reg_read32(VIRTIO_REG_MAGIC) != 0x74726976)
        PANIC("virtio: invalid magic value");
    if (virtio_reg_read32(VIRTIO_REG_VERSION) != 1)
        PANIC("virtio: invalid version");
    if (virtio_reg_read32(VIRTIO_REG_DEVICE_ID) != VIRTIO_DEVICE_BLK)
        PANIC("virtio: invalid device id");

    // 1. 장치를 리셋
    virtio_reg_write32(VIRTIO_REG_DEVICE_STATUS, 0);
    // 2. ACKNOWLEDGE 상태 비트 설정: 장치를 발견함
    virtio_reg_fetch_and_or32(VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_ACK);
    // 3. DRIVER 상태 비트 설정: 장치 사용 방법을 알고 있음
    virtio_reg_fetch_and_or32(VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_DRIVER);
    // 페이지 크기 설정: 4KB 페이지 사용. PFN (페이지 프레임 번호) 계산에 사용됨
    virtio_reg_write32(VIRTIO_REG_PAGE_SIZE, PAGE_SIZE);
    // 디스크 읽기/쓰기 요청용 큐 초기화
    blk_request_vq = virtq_init(0);
    // 6. DRIVER_OK 상태 비트 설정: 이제 장치를 사용할 수 있음
    virtio_reg_write32(VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_DRIVER_OK);

    // 디스크 용량을 가져옵니다.
    blk_capacity = virtio_reg_read64(VIRTIO_REG_DEVICE_CONFIG + 0) * SECTOR_SIZE;
    printf("virtio-blk: capacity is %d bytes\n", (int)blk_capacity);

    // 장치에 요청(request)을 저장할 영역을 할당합니다.
    blk_req_paddr = alloc_pages(align_up(sizeof(*blk_req), PAGE_SIZE) / PAGE_SIZE);
    blk_req = (struct virtio_blk_req *) blk_req_paddr;
}

// desc_index는 새로운 요청의 디스크립터 체인의 헤드 디스크립터 인덱스입니다.
// 장치에 새로운 요청이 있음을 알립니다.
void virtq_kick(struct virtio_virtq *vq, int desc_index) {
    vq->avail.ring[vq->avail.index % VIRTQ_ENTRY_NUM] = desc_index;
    vq->avail.index++;
    __sync_synchronize();
    virtio_reg_write32(VIRTIO_REG_QUEUE_NOTIFY, vq->queue_index);
    vq->last_used_index++;
}

// 장치가 요청을 처리 중인지 확인합니다.
bool virtq_is_busy(struct virtio_virtq *vq) {
    return vq->last_used_index != *vq->used_index;
}

// virtio-blk 장치로부터 읽기/쓰기를 수행합니다.
void read_write_disk(void *buf, unsigned sector, int is_write) {
    if (sector >= blk_capacity / SECTOR_SIZE) {
        printf("virtio: tried to read/write sector=%d, but capacity is %d\n",
              sector, blk_capacity / SECTOR_SIZE);
        return;
    }

    // virtio-blk 사양에 따라 요청을 구성합니다.
    blk_req->sector = sector;
    blk_req->type = is_write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
    if (is_write)
        memcpy(blk_req->data, buf, SECTOR_SIZE);

    // virtqueue 디스크립터를 구성합니다 (3개의 디스크립터 사용).
    struct virtio_virtq *vq = blk_request_vq;
    vq->descs[0].addr = blk_req_paddr;
    vq->descs[0].len = sizeof(uint32_t) * 2 + sizeof(uint64_t);
    vq->descs[0].flags = VIRTQ_DESC_F_NEXT;
    vq->descs[0].next = 1;

    vq->descs[1].addr = blk_req_paddr + offsetof(struct virtio_blk_req, data);
    vq->descs[1].len = SECTOR_SIZE;
    vq->descs[1].flags = VIRTQ_DESC_F_NEXT | (is_write ? 0 : VIRTQ_DESC_F_WRITE);
    vq->descs[1].next = 2;

    vq->descs[2].addr = blk_req_paddr + offsetof(struct virtio_blk_req, status);
    vq->descs[2].len = sizeof(uint8_t);
    vq->descs[2].flags = VIRTQ_DESC_F_WRITE;

    // 장치에 새로운 요청이 있음을 알림.
    virtq_kick(vq, 0);

    // 장치가 요청 처리를 마칠 때까지 대기(바쁜 대기; busy-wait).
    while (virtq_is_busy(vq))
        ;

    // virtio-blk: 0이 아닌 값이 반환되면 에러입니다.
    if (blk_req->status != 0) {
        printf("virtio: warn: failed to read/write sector=%d status=%d\n",
               sector, blk_req->status);
        return;
    }

    // 읽기 작업의 경우, 데이터를 버퍼에 복사합니다.
    if (!is_write)
        memcpy(buf, blk_req->data, SECTOR_SIZE);
}

struct file files[FILES_MAX];
uint8_t disk[DISK_MAX_SIZE];

int oct2int(char *oct, int len) {
    int dec = 0;
    for (int i = 0; i < len; i++) {
        if (oct[i] < '0' || oct[i] > '7')
            break;

        dec = dec * 8 + (oct[i] - '0');
    }
    return dec;
}

void fs_init(void) {
    for (unsigned sector = 0; sector < sizeof(disk) / SECTOR_SIZE; sector++)
        read_write_disk(&disk[sector * SECTOR_SIZE], sector, false);

    unsigned off = 0;
    for (int i = 0; i < FILES_MAX; i++) {
        struct tar_header *header = (struct tar_header *) &disk[off];
        if (header->name[0] == '\0')
            break;

        if (strcmp(header->magic, "ustar") != 0)
            PANIC("invalid tar header: magic=\"%s\"", header->magic);

        int filesz = oct2int(header->size, sizeof(header->size));
        struct file *file = &files[i];
        file->in_use = true;
        strcpy(file->name, header->name);
        memcpy(file->data, header->data, filesz);
        file->size = filesz;
        printf("file: %s, size=%d\n", file->name, file->size);

        off += align_up(sizeof(struct tar_header) + filesz, SECTOR_SIZE);
    }
}

void fs_flush(void) {
    // Copy all file contents into `disk` buffer.
    memset(disk, 0, sizeof(disk));
    unsigned off = 0;
    for (int file_i = 0; file_i < FILES_MAX; file_i++) {
        struct file *file = &files[file_i];
        if (!file->in_use)
            continue;

        struct tar_header *header = (struct tar_header *) &disk[off];
        memset(header, 0, sizeof(*header));
        strcpy(header->name, file->name);
        strcpy(header->mode, "000644");
        strcpy(header->magic, "ustar");
        strcpy(header->version, "00");
        header->type = '0';

        // Turn the file size into an octal string.
        int filesz = file->size;
        for (int i = sizeof(header->size); i > 0; i--) {
            header->size[i - 1] = (filesz % 8) + '0';
            filesz /= 8;
        }

        // Calculate the checksum.
        int checksum = ' ' * sizeof(header->checksum);
        for (unsigned i = 0; i < sizeof(struct tar_header); i++)
            checksum += (unsigned char) disk[off + i];

        for (int i = 5; i >= 0; i--) {
            header->checksum[i] = (checksum % 8) + '0';
            checksum /= 8;
        }

        // Copy file data.
        memcpy(header->data, file->data, file->size);
        off += align_up(sizeof(struct tar_header) + file->size, SECTOR_SIZE);
    }

    // Write `disk` buffer into the virtio-blk.
    for (unsigned sector = 0; sector < sizeof(disk) / SECTOR_SIZE; sector++)
        read_write_disk(&disk[sector * SECTOR_SIZE], sector, true);

    printf("wrote %d bytes to disk\n", sizeof(disk));
}

struct file *fs_lookup(const char *filename) {
    for (int i = 0; i < FILES_MAX; i++) {
        struct file *file = &files[i];
        if (!strcmp(file->name, filename))
            return file;
    }

    return NULL;
}

void map_page(uint32_t *table1, uint32_t vaddr, paddr_t paddr, uint32_t flags) {
    if (!is_aligned(vaddr, PAGE_SIZE))
        PANIC("unaligned vaddr %x", vaddr);

    if (!is_aligned(paddr, PAGE_SIZE))
        PANIC("unaligned paddr %x", paddr);

    uint32_t vpn1 = (vaddr >> 22) & 0x3ff;
    if ((table1[vpn1] & PAGE_V) == 0) {
        // 1단계 페이지 테이블 엔트리가 존재하지 않으면 2단계 페이지 테이블을 생성합니다.
        uint32_t pt_paddr = alloc_pages(1);
        // paddr를 PAGE_SIZE로 나누는 이유는 엔트리에 물리 주소 자체가 아니라 "물리 페이지 번호(physical page number)"를 저장해야 하기 때문
        table1[vpn1] = ((pt_paddr / PAGE_SIZE) << 10) | PAGE_V;
    }

    // Set the 2nd level page table entry to map the physical page.
    uint32_t vpn0 = (vaddr >> 12) & 0x3ff;
    uint32_t *table0 = (uint32_t *) ((table1[vpn1] >> 10) * PAGE_SIZE);
    table0[vpn0] = ((paddr / PAGE_SIZE) << 10) | flags | PAGE_V;
}

struct process *create_process(const void *image, size_t image_size) {
    // 미사용(UNUSED) 상태의 프로세스 구조체 찾기
    struct process *proc = NULL;
    int i;
    for (i = 0; i < PROCS_MAX; i++) {
        if (procs[i].state == PROC_UNUSED) {
            proc = &procs[i];
            break;
        }
    }

    if (!proc)
        PANIC("no free process slots");

    // 커널 스택에 callee-saved 레지스터 공간을 미리 준비
    // 첫 컨텍스트 스위치 시, switch_context에서 이 값들을 복원함
    uint32_t *sp = (uint32_t *) &proc->stack[sizeof(proc->stack)];
    *--sp = 0;                      // s11
    *--sp = 0;                      // s10
    *--sp = 0;                      // s9
    *--sp = 0;                      // s8
    *--sp = 0;                      // s7
    *--sp = 0;                      // s6
    *--sp = 0;                      // s5
    *--sp = 0;                      // s4
    *--sp = 0;                      // s3
    *--sp = 0;                      // s2
    *--sp = 0;                      // s1
    *--sp = 0;                      // s0
    *--sp = (uint32_t) user_entry;  // ra (changed!)

    // Map kernel pages.
    uint32_t *page_table = (uint32_t *) alloc_pages(1);
    for (paddr_t paddr = (paddr_t) __kernel_base; paddr < (paddr_t) __free_ram_end; paddr += PAGE_SIZE)
        map_page(page_table, paddr, paddr, PAGE_R | PAGE_W | PAGE_X);

    map_page(page_table, VIRTIO_BLK_PADDR, VIRTIO_BLK_PADDR, PAGE_R | PAGE_W); // new - Map virtio-blk device

    // Map user pages.
    for (uint32_t off = 0; off < image_size; off += PAGE_SIZE) {
        paddr_t page = alloc_pages(1);

        // Handle the case where the data to be copied is smaller than the
        // page size.
        size_t remaining = image_size - off;
        size_t copy_size = PAGE_SIZE <= remaining ? PAGE_SIZE : remaining;

        // Fill and map the page.
        memcpy((void *) page, image + off, copy_size);
        map_page(page_table, USER_BASE + off, page,
                 PAGE_U | PAGE_R | PAGE_W | PAGE_X);
    }

    
    // 구조체 필드 초기화
    proc->pid = i + 1;
    proc->state = PROC_RUNNABLE;
    proc->page_table = page_table;
    proc->sp = (uint32_t) sp;
    return proc;
}

struct process *current_proc; // 현재 실행 중인 프로세스
struct process *idle_proc;    // Idle 프로세스

void yield(void) {
    // 실행 가능한 프로세스를 탐색
    struct process *next = idle_proc;
    for (int i = 0; i < PROCS_MAX; i++) {
        struct process *proc = &procs[(current_proc->pid + i) % PROCS_MAX];
        if (proc->state == PROC_RUNNABLE && proc->pid > 0) {
            next = proc;
            break;
        }
    }

    // 현재 프로세스 말고는 실행 가능한 프로세스가 없으면, 그냥 리턴
    if (next == current_proc)
        return;

    // 스위칭 시 sscratch에 커널 스택 초기값을 설정 합니다. 아래와 같이 스케줄러(yield)에서 프로세스 전환 직전에 sscratch 레지스터를 적절히 설정
    __asm__ __volatile__(
        "sfence.vma\n"
        "csrw satp, %[satp]\n"
        "sfence.vma\n"
        "csrw sscratch, %[sscratch]\n"
        :
        : [satp] "r" (SATP_SV32 | ((uint32_t) next->page_table / PAGE_SIZE)), [sscratch] "r" ((uint32_t) &next->stack[sizeof(next->stack)])
    );

    // 컨텍스트 스위칭
    struct process *prev = current_proc;
    current_proc = next;
    switch_context(&prev->sp, &next->sp);
}

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

long getchar(void) {
    struct sbiret ret = sbi_call(0, 0, 0, 0, 0, 0, 0, 2);
    return ret.error;
}

void delay(void) {
    for (int i = 0; i < 30000000; i++)
        __asm__ __volatile__("nop"); // do nothing
}

struct process *proc_a;
struct process *proc_b;

void proc_a_entry(void) {
    printf("starting process A\n");
    while (1) {
        putchar('A');
        // switch_context(&proc_a->sp, &proc_b->sp);
        // delay();
        yield();
    }
}

void proc_b_entry(void) {
    printf("starting process B\n");
    while (1) {
        putchar('B');
        // switch_context(&proc_b->sp, &proc_a->sp);
        // delay();
        yield();
    }
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
        // Retrieve the kernel stack of the running process from sscratch.
        // csrrw sp, sscratch, sp는 사실상 스왑(swap) 연산으로 보면 됩니다:
        // 예외가 발생했을 때의 스택 포인터를 신뢰할 수 없기 때문에, sscratch에 미리 저장해둔 커널 스택 포인터로 교체합니다.
        "csrrw sp, sscratch, sp\n"
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

        // Retrieve and save the sp at the time of exception.
        "csrr a0, sscratch\n"
        "sw a0, 4 * 30(sp)\n"

        // Reset the kernel stack.
        "addi a0, sp, 4 * 31\n"
        "csrw sscratch, a0\n"

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

void handle_syscall(struct trap_frame *f) {
    switch (f->a3) {
        case SYS_GETCHAR:
            while (1) {
                long ch = getchar();
                if (ch >= 0) {
                    f->a0 = ch;
                    break;
                }

                yield();
            }
            break;
        case SYS_PUTCHAR:
            putchar(f->a0);
            break;
        case SYS_EXIT:
            printf("process %d exited\n", current_proc->pid);
            current_proc->state = PROC_EXITED;
            yield();
            PANIC("unreachable");
        case SYS_READFILE:
        case SYS_WRITEFILE: {
            const char *filename = (const char *) f->a0;
            char *buf = (char *) f->a1;
            int len = f->a2;
            struct file *file = fs_lookup(filename);
            if (!file) {
                printf("file not found: %s\n", filename);
                f->a0 = -1;
                break;
            }

            if (len > (int) sizeof(file->data))
                len = file->size;

            if (f->a3 == SYS_WRITEFILE) {
                memcpy(file->data, buf, len);
                file->size = len;
                fs_flush();
            } else {
                memcpy(buf, file->data, len);
            }

            f->a0 = len;
            break;
        }
        default:
            PANIC("unexpected syscall a3=%x\n", f->a3);
    }
}

void handle_trap(struct trap_frame *f) {
    uint32_t scause = READ_CSR(scause);
    uint32_t stval = READ_CSR(stval);
    uint32_t user_pc = READ_CSR(sepc);

    if (scause == SCAUSE_ECALL) {
        handle_syscall(f);
        user_pc += 4;
    } else {
        PANIC("unexpected trap scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);
    }

    WRITE_CSR(sepc, user_pc);
}

void kernel_main(void) {    
    memset(__bss, 0, (size_t) __bss_end - (size_t) __bss);

    paddr_t paddr0 = alloc_pages(2);
    paddr_t paddr1 = alloc_pages(1);
    printf("alloc_pages test: paddr0=%x\n", paddr0);
    printf("alloc_pages test: paddr1=%x\n", paddr1);

    WRITE_CSR(stvec, (uint32_t) kernel_entry);

    virtio_blk_init(); // new
    fs_init();

    char buf[SECTOR_SIZE];
    read_write_disk(buf, 0, false /* read from the disk */);
    printf("first sector: %s\n", buf);

    strcpy(buf, "hello from kernel!!!\n");
    read_write_disk(buf, 0, true /* write to the disk */);

    idle_proc = create_process(NULL, 0); // updated!
    idle_proc->pid = 0; // idle
    current_proc = idle_proc;

    // new!
    create_process(_binary_shell_bin_start, (size_t) _binary_shell_bin_size);

    yield();
    PANIC("switched to idle process");

    // stvec를 설정한 뒤, unimp 명령어(illegal instruction 로 간주됨)를 실행해 일부러 예외를 일으키는 코드입니다.
    // unimp 는 “의사(pseudo) 명령어”.
    // __asm__ __volatile__("unimp"); // new
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