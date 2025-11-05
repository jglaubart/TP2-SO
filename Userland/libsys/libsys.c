#include <sys.h>
#include <syscalls.h>

void startBeep(uint32_t nFrequence) {
    sys_start_beep(nFrequence);
}

void stopBeep(void) {
    sys_stop_beep();
}

void setTextColor(uint32_t color) {
    sys_fonts_text_color(color);
}

void setBackgroundColor(uint32_t color) {
    sys_fonts_background_color(color);
}

uint8_t increaseFontSize(void) {
    return sys_fonts_increase_size();
}

uint8_t decreaseFontSize(void) {
    return sys_fonts_decrease_size();
}

uint8_t setFontSize(uint8_t size) {
    return sys_fonts_set_size(size);
}

void getDate(int * hour, int * minute, int * second) {
    sys_hour(hour);
    sys_minute(minute);
    sys_second(second);
}

void clearScreen(void) {
    sys_clear_screen();
}

void clearInputBuffer(void) {
    sys_clear_input_buffer();
}

void drawCircle(uint32_t color, long long int topleftX, long long int topLefyY, long long int diameter) {
    sys_circle(color, topleftX, topLefyY, diameter);
}

void drawRectangle(uint32_t color, long long int width_pixels, long long int height_pixels, long long int initial_pos_x, long long int initial_pos_y){
    sys_rectangle(color, width_pixels, height_pixels, initial_pos_x, initial_pos_y);
}

void fillVideoMemory(uint32_t hexColor) {
    sys_fill_video_memory(hexColor);
}

int32_t exec(int32_t (*fnPtr)(void)) {
    return sys_exec(fnPtr);
}

void registerKey(enum REGISTERABLE_KEYS scancode, void (*fn)(enum REGISTERABLE_KEYS scancode)) {
    sys_register_key(scancode, fn);
}


int getWindowWidth(void) {
    return sys_window_width();
}

int getWindowHeight(void) {
    return sys_window_height();
}

void sleep(uint32_t miliseconds) {
    sys_sleep_milis(miliseconds);
}

int32_t getRegisterSnapshot(int64_t * registers) {
    return sys_get_register_snapshot(registers);
}

int32_t getCharacterWithoutDisplay(void) {
    return sys_get_character_without_display();
}

// Memory management syscall prototypes
/* 0x80000100 */
void * myMalloc(int size){
    return sys_malloc(size);
}
/* 0x80000101 */
int32_t myFree(void * ptr){
    return sys_free(ptr);
}
/* 0x80000102 */
int32_t mem(int * total, int * used, int * available){
    return sys_memstats(total, used, available) ;
}

// Process management syscall prototypes
/* 0x80000200 */
int32_t getPid(void){
    return sys_getpid();
}
/* 0x80000201 */
int32_t createProcess(void * function, uint64_t argc, uint8_t ** argv, uint8_t is_background){
    return sys_create_process(function, argc, argv, is_background);
}
/* 0x80000202 */
int32_t unblock(int pid){
    return sys_unblock(pid);
}
/* 0x80000203 */
int32_t block(int pid){
    return sys_block(pid);
}
/* 0x80000204 */
int32_t kill(int pid){
    return sys_kill(pid);
}
int32_t ps(ProcessInformation * processInfoTable){
    return sys_ps(processInfoTable);
}
/* 0x80000206 */
int32_t nice(int pid, int newPriority){
    return sys_nice(pid, newPriority);
}
/* 0x80000207 */
int32_t waitPid(int pid){
    return sys_wait_pid(pid);
}
/* 0x80000208 */
int32_t yield(void){
    return sys_yield();
}
/* 0x80000209 */
int32_t waitChildren(void){
    return sys_wait_children();
}

// Semaphore management syscall prototypes
/* 0x80000300 */
void * semInit(const char *name, uint32_t initial_count){
    return (void *)sys_sem_init(name, initial_count);
}
/* 0x80000301 */
int32_t semPost(void * sem){
    return sys_sem_post(sem);
}
/* 0x80000302 */
int32_t semWait(void * sem){
    return sys_sem_wait(sem);
}
/* 0x80000303 */
int32_t semDestroy(void * sem){
    return sys_sem_destroy(sem);
}
