#include <stddef.h>
#include "../time/profile.h"

void log_init(const char *dest_file);
void log_quit(void);
void log_write(char *data);
void log_write2(char *data, size_t size);
void log_set_dest_file(const char *dest_file);
void log_set_flush_timeout(int timeout_ms);
profile_results_t log_profile_results(void);