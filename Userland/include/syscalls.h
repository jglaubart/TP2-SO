#ifndef _LIBC_SYSCALLS_H_
#define _LIBC_SYSCALLS_H_

#include <stdint.h>
#include <stddef.h>

// Enum of registerable keys.
// Note: Does not include TAB or RETURN
enum REGISTERABLE_KEYS {
    ESCAPE_KEY        = 0x01,
    KEY_1             = 0x02,
    KEY_2             = 0x03,
    KEY_3             = 0x04,
    KEY_4             = 0x05,
    KEY_5             = 0x06,
    KEY_6             = 0x07,
    KEY_7             = 0x08,
    KEY_8             = 0x09,
    KEY_9             = 0x0A,
    KEY_0             = 0x0B,
    MINUS_KEY         = 0x0C,
    EQUALS_KEY        = 0x0D,
    BACKSPACE_KEY     = 0x0E,
    TABULATOR_KEY     = 0x0F,
    Q_KEY             = 0x10,
    W_KEY             = 0x11,
    E_KEY             = 0x12,
    R_KEY             = 0x13,
    T_KEY             = 0x14,
    Y_KEY             = 0x15,
    U_KEY             = 0x16,
    I_KEY             = 0x17,
    O_KEY             = 0x18,
    P_KEY             = 0x19,
    LEFT_BRACKET_KEY  = 0x1A,
    RIGHT_BRACKET_KEY = 0x1B,
    RETURN_KEY        = 0x1C,
    CONTROL_KEY_L     = 0x1D,
    A_KEY             = 0x1E,
    S_KEY             = 0x1F,
    D_KEY             = 0x20,
    F_KEY             = 0x21,
    G_KEY             = 0x22,
    H_KEY             = 0x23,
    J_KEY             = 0x24,
    K_KEY             = 0x25,
    L_KEY             = 0x26,
    SEMICOLON_KEY     = 0x27,
    APOSTROPHE_KEY    = 0x28,
    GRAVE_KEY         = 0x29,
    SHIFT_KEY_L       = 0x2A,
    BACKSLASH_KEY     = 0x2B,
    Z_KEY             = 0x2C,
    X_KEY             = 0x2D,
    C_KEY             = 0x2E,
    V_KEY             = 0x2F,
    B_KEY             = 0x30,
    N_KEY             = 0x31,
    M_KEY             = 0x32,
    COMMA_KEY         = 0x33,
    PERIOD_KEY        = 0x34,
    SLASH_KEY         = 0x35,
    SHIFT_KEY_R       = 0x36,
    KP_ASTERISK_KEY   = 0x37,
    ALT_KEY_L         = 0x38,
    SPACE_KEY         = 0x39,
    CAPS_LOCK_KEY     = 0x3A,
    F1_KEY            = 0x3B,
    F2_KEY            = 0x3C,
    F3_KEY            = 0x3D,
    F4_KEY            = 0x3E,
    F5_KEY            = 0x3F,
    F6_KEY            = 0x40,
    F7_KEY            = 0x41,
    F8_KEY            = 0x42,
    F9_KEY            = 0x43,
    F10_KEY           = 0x44,
    NUM_LOCK_KEY      = 0x45,
    SCROLL_LOCK_KEY   = 0x46,
    KP_HOME_KEY       = 0x47,
    KP_UP_KEY         = 0x48,
    KP_PAGE_UP_KEY    = 0x49,
    KP_MINUS_KEY      = 0x4A,
    KP_LEFT_KEY       = 0x4B,
    KP_BEGIN_KEY      = 0x4C,
    KP_RIGHT_KEY      = 0x4D,
    KP_PLUS_KEY       = 0x4E,
    KP_END_KEY        = 0x4F,
    KP_DOWN_KEY       = 0x50,
    KP_PAGE_DOWN_KEY  = 0x51,
    KP_INSERT_KEY     = 0x52,
    KP_DELETE_KEY     = 0x53,
    F11_KEY           = 0x57,
    F12_KEY           = 0x58
};

// Linux syscall prototypes
int32_t sys_write(int64_t fd, const void * buf, int64_t count);
int32_t sys_read(int64_t fd, void * buf, int64_t count);

// Custom syscall prototypes
/* 0x80000000 */
int32_t sys_start_beep(uint32_t nFrequence);
/* 0x80000001 */
int32_t sys_stop_beep(void);
/* 0x80000002 */
int32_t sys_fonts_text_color(uint32_t color);
/* 0x80000003 */
int32_t sys_fonts_background_color(uint32_t color);
/* 0x80000007 */
int32_t sys_fonts_decrease_size(void);
/* 0x80000008 */
int32_t sys_fonts_increase_size(void);
/* 0x80000009 */
int32_t sys_fonts_set_size(uint8_t size);
/* 0x8000000A */
int32_t sys_clear_screen(void);
/* 0x8000000B */
int32_t sys_clear_input_buffer(void);

// Date syscall prototypes
/* 0x80000010 */
int32_t sys_hour(int * hour);
/* 0x80000011 */
int32_t sys_minute(int * minute);
/* 0x80000012 */
int32_t sys_second(int * second);

int32_t sys_circle(int color, long long int topleftX, long long int topLefyY, long long int diameter);

int32_t sys_rectangle(int color, long long int width_pixels, long long int height_pixels, long long int initial_pos_x, long long int initial_pos_y);

int32_t sys_fill_video_memory(uint32_t hexColor);

int32_t sys_exec(int32_t (*fnPtr)(void));

int32_t sys_register_key(uint8_t scancode, void (*fn)(enum REGISTERABLE_KEYS scancode));

int32_t sys_window_width(void);

int32_t sys_window_height(void);

int32_t sys_sleep_milis(uint32_t milis);

int32_t sys_get_register_snapshot(int64_t * registers);

int32_t sys_get_character_without_display(void);


// ================== Memory management syscall prototypes =================
/* 0x80000100 */
void * sys_malloc(int size);
/* 0x80000101 */
int32_t sys_free(void * ptr);
/* 0x80000102 */
int32_t sys_memstats(int * total, int * used, int * available);
// =========================================================================

// ================== Process management syscall prototypes =================
typedef enum ProcessState {
    PROCESS_STATE_READY,
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_BLOCKED,
    PROCESS_STATE_TERMINATED
} ProcessState;

typedef struct ProcessInformation{
    int pid;
    char * name;
    int priority;
    ProcessState state;
    uint8_t * rsp;
    uint8_t * stack_base;
} ProcessInformation;

/* 0x80000200 */
int32_t sys_getpid(void);
/* 0x80000201 */
int32_t sys_create_process(void * function, uint64_t argc, uint8_t ** argv, uint8_t is_background);
/* 0x80000202 */
int32_t sys_unblock(int pid);
/* 0x80000203 */
int32_t sys_block(int pid);
/* 0x80000204 */
int32_t sys_kill(int pid);
/* 0x80000205 */
int32_t sys_ps(ProcessInformation * processInfoTable);
/* 0x80000206 */
int32_t sys_nice(int pid, int newPriority);
/* 0x80000207 */
int32_t sys_wait_pid(int pid);
/* 0x80000208 */
int32_t sys_yield(void);
// ==========================================================================

// ================== Semaphore management syscall prototypes =================

/* 0x80000300 */
void * sys_sem_init(const char *name, uint32_t initial_count);
/* 0x80000301 */
int32_t sys_sem_post(void * sem);
/* 0x80000302 */
int32_t sys_sem_wait(void * sem);
/* 0x80000303 */
int32_t sys_sem_destroy(void * sem);

#endif
