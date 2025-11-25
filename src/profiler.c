#include "profiler.h"
#include <stdio.h>
#include <string.h>

/* Global profiler state */
ProfilerStats g_profiler_stats[PROFILE_COUNT] = {0};
bool g_profiler_enabled = false;

/* Memory tracking */
uint64_t g_total_memory_allocated = 0;
uint64_t g_peak_memory_usage = 0;
uint64_t g_current_memory_usage = 0;

/* Performance counters */
uint64_t g_frame_count = 0;
uint64_t g_instruction_count = 0;
uint64_t g_memory_access_count = 0;

/* Real-time metrics */
static PerformanceMetrics g_current_metrics = {0};
static uint64_t g_last_update_time = 0;
static uint64_t g_last_frame_count = 0;
static uint64_t g_last_instruction_count = 0;

/* Profile point names */
static const char* profile_point_names[PROFILE_COUNT] = {
    "CPU Step",
    "PPU Step",
    "APU Step", 
    "Memory Read",
    "Memory Write",
    "Frame Render",
    "DMA Transfer",
    "Interrupt Handler",
    "ROM Access"
};

void profiler_init(void) {
    memset(g_profiler_stats, 0, sizeof(g_profiler_stats));
    g_profiler_enabled = false;
    g_total_memory_allocated = 0;
    g_peak_memory_usage = 0;
    g_current_memory_usage = 0;
    g_frame_count = 0;
    g_instruction_count = 0;
    g_memory_access_count = 0;
}

void profiler_enable(bool enable) {
    g_profiler_enabled = enable;
    if (enable) {
        g_last_update_time = profiler_get_time_ns();
    }
}

void profiler_reset(void) {
    memset(g_profiler_stats, 0, sizeof(g_profiler_stats));
    g_frame_count = 0;
    g_instruction_count = 0;
    g_memory_access_count = 0;
    g_last_update_time = profiler_get_time_ns();
    g_last_frame_count = 0;
    g_last_instruction_count = 0;
}

void profiler_print_report(void) {
    if (!g_profiler_enabled) {
        printf("Profiler is disabled\n");
        return;
    }
    
    printf("\n=== Performance Profile Report ===\n");
    printf("%-20s %10s %15s %10s %10s %10s\n", 
           "Function", "Calls", "Total (ms)", "Avg (µs)", "Min (µs)", "Max (µs)");
    printf("%-20s %10s %15s %10s %10s %10s\n", 
           "--------", "-----", "----------", "--------", "--------", "--------");
    
    for (int i = 0; i < PROFILE_COUNT; i++) {
        ProfilerStats* stats = &g_profiler_stats[i];
        if (stats->call_count > 0) {
            double total_ms = stats->total_time_ns / 1000000.0;
            double avg_us = (stats->total_time_ns / (double)stats->call_count) / 1000.0;
            double min_us = stats->min_time_ns / 1000.0;
            double max_us = stats->max_time_ns / 1000.0;
            
            printf("%-20s %10llu %15.3f %10.2f %10.2f %10.2f\n",
                   profile_point_names[i],
                   (unsigned long long)stats->call_count,
                   total_ms, avg_us, min_us, max_us);
        }
    }
    
    printf("\n=== Performance Counters ===\n");
    printf("Frames rendered:     %llu\n", (unsigned long long)g_frame_count);
    printf("Instructions executed: %llu\n", (unsigned long long)g_instruction_count);
    printf("Memory accesses:     %llu\n", (unsigned long long)g_memory_access_count);
    
    if (g_frame_count > 0) {
        printf("Instructions/frame:  %.0f\n", (double)g_instruction_count / g_frame_count);
        printf("Memory accesses/frame: %.0f\n", (double)g_memory_access_count / g_frame_count);
    }
    
    printf("\n=== Current Metrics ===\n");
    printf("FPS:                 %.2f\n", g_current_metrics.fps);
    printf("CPU usage:           %.1f%%\n", g_current_metrics.cpu_usage);
    printf("Memory bandwidth:    %.2f MB/s\n", g_current_metrics.memory_bandwidth_mb_s);
    printf("Instructions/sec:    %llu\n", (unsigned long long)g_current_metrics.instructions_per_second);
}

void profiler_track_allocation(size_t size) {
    g_total_memory_allocated += size;
    g_current_memory_usage += size;
    if (g_current_memory_usage > g_peak_memory_usage) {
        g_peak_memory_usage = g_current_memory_usage;
    }
}

void profiler_track_deallocation(size_t size) {
    if (g_current_memory_usage >= size) {
        g_current_memory_usage -= size;
    }
}

void profiler_print_memory_stats(void) {
    printf("\n=== Memory Usage Statistics ===\n");
    printf("Total allocated:     %llu bytes\n", (unsigned long long)g_total_memory_allocated);
    printf("Current usage:       %llu bytes (%.2f MB)\n", 
           (unsigned long long)g_current_memory_usage,
           g_current_memory_usage / (1024.0 * 1024.0));
    printf("Peak usage:          %llu bytes (%.2f MB)\n",
           (unsigned long long)g_peak_memory_usage,
           g_peak_memory_usage / (1024.0 * 1024.0));
}

void profiler_increment_frame_count(void) {
    g_frame_count++;
}

void profiler_increment_instruction_count(void) {
    g_instruction_count++;
}

void profiler_increment_memory_access_count(void) {
    g_memory_access_count++;
}

PerformanceMetrics profiler_get_current_metrics(void) {
    return g_current_metrics;
}

void profiler_update_metrics(void) {
    if (!g_profiler_enabled) return;
    
    uint64_t current_time = profiler_get_time_ns();
    uint64_t time_delta = current_time - g_last_update_time;
    
    if (time_delta >= 1000000000ULL) { /* Update every second */
        uint64_t frame_delta = g_frame_count - g_last_frame_count;
        uint64_t instruction_delta = g_instruction_count - g_last_instruction_count;
        
        double seconds = time_delta / 1000000000.0;
        
        g_current_metrics.fps = frame_delta / seconds;
        g_current_metrics.instructions_per_second = instruction_delta / seconds;
        g_current_metrics.memory_bandwidth_mb_s = (g_memory_access_count * 8.0) / (seconds * 1024.0 * 1024.0);
        
        /* CPU usage estimation based on instruction execution rate */
        double target_ips = 4194304.0; /* 4.19 MHz target */
        g_current_metrics.cpu_usage = (g_current_metrics.instructions_per_second / target_ips) * 100.0;
        if (g_current_metrics.cpu_usage > 100.0) g_current_metrics.cpu_usage = 100.0;
        
        g_last_update_time = current_time;
        g_last_frame_count = g_frame_count;
        g_last_instruction_count = g_instruction_count;
    }
}
