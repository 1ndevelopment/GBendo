#ifndef ERROR_HANDLING_H
#define ERROR_HANDLING_H

#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

/* Forward declarations to avoid circular dependencies */
struct SM83_CPU;
struct Memory;
enum MBC_Type;

/* Error codes for the emulator */
typedef enum {
    GB_SUCCESS = 0,
    GB_ERROR_NULL_POINTER,
    GB_ERROR_INVALID_ARGUMENT,
    GB_ERROR_MEMORY_ALLOCATION,
    GB_ERROR_FILE_NOT_FOUND,
    GB_ERROR_FILE_READ,
    GB_ERROR_FILE_WRITE,
    GB_ERROR_ROM_INVALID,
    GB_ERROR_ROM_SIZE_INVALID,
    GB_ERROR_MBC_UNSUPPORTED,
    GB_ERROR_SAVE_STATE_INVALID,
    GB_ERROR_AUDIO_INIT_FAILED,
    GB_ERROR_VIDEO_INIT_FAILED,
    GB_ERROR_CPU_FAULT,
    GB_ERROR_MEMORY_CORRUPT
} GBError;

/* Error handling context */
typedef struct {
    GBError last_error;
    char error_message[256];
    const char* error_file;
    int error_line;
    const char* error_function;
} ErrorContext;

extern ErrorContext g_error_context;

/* Error reporting macros */
#define SET_ERROR(code, msg) \
    do { \
        g_error_context.last_error = (code); \
        snprintf(g_error_context.error_message, sizeof(g_error_context.error_message), \
                 "%s", (msg)); \
        g_error_context.error_file = __FILE__; \
        g_error_context.error_line = __LINE__; \
        g_error_context.error_function = __func__; \
        fprintf(stderr, "[ERROR] %s:%d in %s(): %s\n", \
                __FILE__, __LINE__, __func__, (msg)); \
    } while (0)

#define SET_ERROR_ERRNO(code, msg) \
    do { \
        g_error_context.last_error = (code); \
        snprintf(g_error_context.error_message, sizeof(g_error_context.error_message), \
                 "%s: %s", (msg), strerror(errno)); \
        g_error_context.error_file = __FILE__; \
        g_error_context.error_line = __LINE__; \
        g_error_context.error_function = __func__; \
        fprintf(stderr, "[ERROR] %s:%d in %s(): %s: %s\n", \
                __FILE__, __LINE__, __func__, (msg), strerror(errno)); \
    } while (0)

/* Input validation macros */
#define VALIDATE_NOT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            SET_ERROR(GB_ERROR_NULL_POINTER, "Null pointer passed to " #ptr); \
            return false; \
        } \
    } while (0)

#define VALIDATE_NOT_NULL_RET(ptr, ret) \
    do { \
        if ((ptr) == NULL) { \
            SET_ERROR(GB_ERROR_NULL_POINTER, "Null pointer passed to " #ptr); \
            return (ret); \
        } \
    } while (0)

#define VALIDATE_RANGE(value, min, max) \
    do { \
        if ((value) < (min) || (value) > (max)) { \
            SET_ERROR(GB_ERROR_INVALID_ARGUMENT, \
                      #value " out of valid range [" #min ", " #max "]"); \
            return false; \
        } \
    } while (0)

#define VALIDATE_RANGE_RET(value, min, max, ret) \
    do { \
        if ((value) < (min) || (value) > (max)) { \
            SET_ERROR(GB_ERROR_INVALID_ARGUMENT, \
                      #value " out of valid range [" #min ", " #max "]"); \
            return (ret); \
        } \
    } while (0)

/* Memory allocation with error checking */
#define SAFE_MALLOC(ptr, size) \
    do { \
        (ptr) = malloc(size); \
        if ((ptr) == NULL) { \
            SET_ERROR_ERRNO(GB_ERROR_MEMORY_ALLOCATION, \
                           "Failed to allocate " #size " bytes for " #ptr); \
            return false; \
        } \
        memset((ptr), 0, (size)); \
    } while (0)

#define SAFE_MALLOC_RET(ptr, size, ret) \
    do { \
        (ptr) = malloc(size); \
        if ((ptr) == NULL) { \
            SET_ERROR_ERRNO(GB_ERROR_MEMORY_ALLOCATION, \
                           "Failed to allocate " #size " bytes for " #ptr); \
            return (ret); \
        } \
        memset((ptr), 0, (size)); \
    } while (0)

/* File operations with error checking */
#define SAFE_FOPEN(fp, filename, mode) \
    do { \
        (fp) = fopen((filename), (mode)); \
        if ((fp) == NULL) { \
            SET_ERROR_ERRNO(GB_ERROR_FILE_NOT_FOUND, \
                           "Failed to open file: " filename); \
            return false; \
        } \
    } while (0)

#define SAFE_FOPEN_RET(fp, filename, mode, ret) \
    do { \
        (fp) = fopen((filename), (mode)); \
        if ((fp) == NULL) { \
            SET_ERROR_ERRNO(GB_ERROR_FILE_NOT_FOUND, \
                           "Failed to open file: " filename); \
            return (ret); \
        } \
    } while (0)

/* Error handling functions */
void error_init(void);
void error_clear(void);
GBError error_get_last(void);
const char* error_get_message(void);
const char* error_code_to_string(GBError code);
void error_print_backtrace(void);

/* ROM validation */
bool validate_rom_header(const uint8_t* rom_data, size_t rom_size);
bool validate_rom_checksum(const uint8_t* rom_data, size_t rom_size);
int detect_mbc_type(uint8_t cartridge_type);

/* Memory bounds checking */
bool is_valid_address(uint16_t addr, bool for_write);
bool is_valid_rom_bank(uint8_t bank, size_t rom_size);
bool is_valid_ram_bank(uint8_t bank, size_t ram_size);

/* CPU state validation */  
bool validate_cpu_state(const void* cpu);
bool validate_memory_state(const void* mem);

#endif /* ERROR_HANDLING_H */
