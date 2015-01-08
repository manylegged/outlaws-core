
#ifndef POSIX_H
#define POSIX_H

#ifdef __cplusplus
extern "C"
{
#endif

// implemented in posix.cpp
void posix_set_signal_handler(void);
void posix_print_stacktrace(void);

// implemented in per-os file
void dump_loaded_shared_objects(void);
void posix_oncrash(const char* message);

#ifdef __cplusplus
}
#endif

#endif // POSIX_H
