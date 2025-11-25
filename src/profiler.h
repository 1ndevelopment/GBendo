#ifndef PROFILER_H
#define PROFILER_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* Performance profiler for the emulator */

typedef struct {
    uint64_t total_time_ns;
    uint64_t call_count;
    uint64_t min_time_ns;
    uint64_t max_time_ns;
} ProfilerStats;

typedef enum {
    PROFILE_CPU_STEP,
    PROFILE_PPU_STEP, 
    PROFILE_APU_STEP,
    PROFILE_MEMORY_READ,
    PROFILE_MEMORY_WRITE,
    PROFILE_FRAME_RENDER,
    PROFILE_DMA_TRANSFER,
    PROFILE_INTERRUPT_HANDLER,
    PROFILE_ROM_ACCESS,
    PROFILE_COUNT  /* Keep this last */
} ProfilerPoint;

/* Global profiler state */
extern ProfilerStats g_profiler_stats[PROFILE_COUNT];
extern bool g_profiler_enabled;

/* Profiler control */
void profiler_init(void);
void profiler_enable(bool enable);
void profiler_reset(void);
void profiler_print_report(void);

/* High-precision timing */
static inline uint64_t profiler_get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* Profiling macros for easy instrumentation */
#define PROFILE_START(point) \
    uint64_t _profile_start_##point = g_profiler_enabled ? profiler_get_time_ns() : 0

#define PROFILE_END(point) \
    do { \
        if (g_profiler_enabled) { \
            uint64_t end_time = profiler_get_time_ns(); \
            uint64_t duration = end_time - _profile_start_##point; \
            ProfilerStats* stats = &g_profiler_stats[PROFILE_##point]; \
            stats->total_time_ns += duration; \
            stats->call_count++; \
            if (stats->call_count == 1 || duration < stats->min_time_ns) { \
                stats->min_time_ns = duration; \
            } \
            if (stats->call_count == 1 || duration > stats->max_time_ns) { \
                stats->max_time_ns = duration; \
            } \
        } \
    } while (0)

/* Automatic scope-based profiling */
typedef struct {
    ProfilerPoint point;
    uint64_t start_time;
} ScopeProfiler;

static inline ScopeProfiler profiler_scope_start(ProfilerPoint point) {
    ScopeProfiler prof = {point, 0};
    if (g_profiler_enabled) {
        prof.start_time = profiler_get_time_ns();
    }
    return prof;
}

static inline void profiler_scope_end(ScopeProfiler* prof) {
    if (g_profiler_enabled && prof->start_time != 0) {
        uint64_t duration = profiler_get_time_ns() - prof->start_time;
        ProfilerStats* stats = &g_profiler_stats[prof->point];
        stats->total_time_ns += duration;
        stats->call_count++;
        if (stats->call_count == 1 || duration < stats->min_time_ns) {
            stats->min_time_ns = duration;
        }
        if (stats->call_count == 1 || duration > stats->max_time_ns) {
            stats->max_time_ns = duration;
        }
    }
}

#define PROFILE_SCOPE(point) \
    ScopeProfiler _scope_prof = profiler_scope_start(PROFILE_##point); \
    __attribute__((cleanup(profiler_scope_end))) ScopeProfiler* _cleanup_prof = &_scope_prof

/* Memory allocation profiler */
extern uint64_t g_total_memory_allocated;
extern uint64_t g_peak_memory_usage;
extern uint64_t g_current_memory_usage;

void profiler_track_allocation(size_t size);
void profiler_track_deallocation(size_t size);
void profiler_print_memory_stats(void);

/* Performance counters */
extern uint64_t g_frame_count;
extern uint64_t g_instruction_count;
extern uint64_t g_memory_access_count;

void profiler_increment_frame_count(void);
void profiler_increment_instruction_count(void);
void profiler_increment_memory_access_count(void);

/* Real-time performance monitoring */
typedef struct {
    float fps;
    float cpu_usage;
    float memory_bandwidth_mb_s;
    uint64_t instructions_per_second;
} PerformanceMetrics;

PerformanceMetrics profiler_get_current_metrics(void);
void profiler_update_metrics(void);

#endif /* PROFILER_H */
