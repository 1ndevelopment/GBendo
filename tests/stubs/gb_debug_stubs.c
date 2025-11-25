#include <stdbool.h>
#include <stdarg.h>
#include "../../src/gbendo.h"
#include "../../src/ui/ui.h"

static GBEmulator* s_dbg = NULL;

bool gb_is_debug_enabled(void) { return false; }
GBEmulator* gb_get_debug_gb(void) { return s_dbg; }

/* UI debug function stubs */
bool ui_is_debug_enabled(UIDebugComponent component) { 
    (void)component; /* Suppress unused parameter warning */
    return false; 
}

void ui_debug_log(UIDebugComponent component, const char* format, ...) {
    (void)component; /* Suppress unused parameter warning */
    (void)format;    /* Suppress unused parameter warning */
    /* Do nothing in tests */
}
