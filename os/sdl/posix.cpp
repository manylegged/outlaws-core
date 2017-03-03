
#include "StdAfx.h"

#include <ftw.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <libgen.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>
#include <execinfo.h>

#if __APPLE__
#include <sys/ucontext.h>
#else
#include <ucontext.h>
#endif

#include <dlfcn.h>

#include "posix.h"

static int g_signaldepth = 0;

// don't go through ReportMessagef/ReportMessage!
static void ReportPOSIX(const char *format, ...)
{
    va_list vl;
    va_start(vl, format);
    const string buf = "\n[POSIX] " + str_vformat(format, vl);
    OL_ReportMessage(buf.c_str());
    va_end(vl);
}

static void ReportPOSIX(const string& str)
{
    ReportPOSIX("%s", str.c_str());
}

// from http://spin.atomicobject.com/2013/01/13/exceptions-stack-traces-c/
static const char* signal_to_string(int sig, siginfo_t *siginfo)
{
    switch(sig)
    {
    case SIGSEGV:
        switch(siginfo->si_code)
        {
        case SEGV_MAPERR: return "Caught SIGSEGV: Segmentation Fault (address not mapped to object)";
        case SEGV_ACCERR: return "Caught SIGSEGV: Segmentation Fault (invalid permissions for mapped object)";
        default: return ("Caught SIGSEGV: Segmentation Fault");
        }
    case SIGINT: return ("Caught SIGINT: Interactive attention signal, (usually ctrl+c)");
    case SIGFPE:
        switch(siginfo->si_code)
        {
        case FPE_INTDIV: return ("Caught SIGFPE: (integer divide by zero)");
        case FPE_INTOVF: return ("Caught SIGFPE: (integer overflow)");
        case FPE_FLTDIV: return ("Caught SIGFPE: (floating-point divide by zero)");
        case FPE_FLTOVF: return ("Caught SIGFPE: (floating-point overflow)");
        case FPE_FLTUND: return ("Caught SIGFPE: (floating-point underflow)");
        case FPE_FLTRES: return ("Caught SIGFPE: (floating-point inexact result)");
        case FPE_FLTINV: return ("Caught SIGFPE: (floating-point invalid operation)");
        case FPE_FLTSUB: return ("Caught SIGFPE: (subscript out of range)");
        default: return ("Caught SIGFPE: Arithmetic Exception");
        }
    case SIGILL:
        switch(siginfo->si_code)
        {
        case ILL_ILLOPC: return ("Caught SIGILL: (illegal opcode)");
        case ILL_ILLOPN: return ("Caught SIGILL: (illegal operand)");
        case ILL_ILLADR: return ("Caught SIGILL: (illegal addressing mode)");
        case ILL_ILLTRP: return ("Caught SIGILL: (illegal trap)");
        case ILL_PRVOPC: return ("Caught SIGILL: (privileged opcode)");
        case ILL_PRVREG: return ("Caught SIGILL: (privileged register)");
        case ILL_COPROC: return ("Caught SIGILL: (coprocessor error)");
        case ILL_BADSTK: return ("Caught SIGILL: (internal stack error)");
        default: return ("Caught SIGILL: Illegal Instruction");
        }
    case SIGTERM: return ("Caught SIGTERM: a termination request was sent to the program");
    case SIGABRT: return ("Caught SIGABRT: usually caused by an abort() or assert()");
    default: return "Caught UNKNOWN";
    }
}

static uint64 get_current_tid()
{
#if __APPLE__
    uint64 tid = 0;
    pthread_threadid_np(pthread_self(), &tid);
    return tid;
#else
    return (uint64) pthread_self();
#endif
}

static void print_backtrace()
{
    static const int maxbuf = 128;
    void* buffer[maxbuf];
    memset(buffer, 0, sizeof(buffer));
    const int count = backtrace(buffer, maxbuf);

    char **strings = get_backtrace_symbols(buffer, count);

    const uint64 current_tid = get_current_tid();
    ReportPOSIX("Dumping stack for thread %#llx '%s'", current_tid,
                map_get(_thread_name_map(), current_tid));
    for (int i=0; i<count; i++) {
        string func;
        
        if (strings && strings[i]) {
#if __APPLE__
            func = strings[i];
#else
            vector<string> fields = str_split(' ', strings[i]);
            for (int j=0; j<fields.size(); ) {
                if (fields[j].size() == 0)
                    fields.erase(fields.begin() + j);
                else
                    j++;
            }
            if (fields.size() == 2) {
                // Linux / GCC
                const int start = fields[0].find('(') + 1;
                const int end = fields[0].find('+', start);
                if (start > 0 && end > start)
                    func = str_demangle(fields[0].substr(start, end - start).c_str());
            } else if (fields.size() >= 3) {
                // OSX/ Clang
                func = (i == 1) ? fields[3] : str_demangle(fields[3].c_str());
                if (fields.size() > 4)
                    func += " " + str_join(' ', fields.begin() + 4, fields.end());
                func += str_format(" (%s)", fields[1].c_str());
            } else {
                func = strings[i];
            }
#endif
        }
        ReportPOSIX("%2d. Called from %p %s", i, buffer[i], func.c_str());
    }

    free(strings);
}

void posix_print_stacktrace()
{
    const int saved_depth = g_signaldepth;
    if (g_signaldepth == 0) {
        g_signaldepth = 1;
    }

    dump_loaded_shared_objects();
    
    print_backtrace();
    fflush(NULL);

    const uint64 current_tid = get_current_tid();
    ReportPOSIX("Observed %d threads from %#llx '%s'",
                (int)_thread_name_map().size(), current_tid, 
                map_get(_thread_name_map(), current_tid));
#if !__APPLE__
    foreach (const auto &x, _thread_name_map())
    {
        if (!x.first || x.first == current_tid)
            continue;
        ReportPOSIX("Sending SIGTERM to thread %#llx, '%s'", x.first, x.second);
        int status;
        if ((status = pthread_kill((pthread_t) x.first, SIGTERM)))
            ReportPOSIX("pthread_kill(%#llx, SIGTERM) failed: %s", x.first, strerror(status));
    }
    sleep(1);                   // wait for other threads to print
    ReportPOSIX("Handling thread done backtracing");
#endif
    g_signaldepth = saved_depth;
}

static void posix_signal_handler(int sig, siginfo_t *siginfo, void *context)
{
    if (g_signaldepth == 0)
        OLG_OnTerminate();

    g_signaldepth++;
    if (g_signaldepth > 2)
    {
        if (g_signaldepth == 3)
            ReportPOSIX("Recursive Signal Handler detected - returning");
        return;
    }
    else if (g_signaldepth == 2)
    {
        print_backtrace();
        return;
    }

    puts("\nsignal handler called");

    string message = str_format("%s (signal %d)", signal_to_string(sig, siginfo), sig);
    ReportPOSIX(message);

    const ucontext_t *ctx = (ucontext_t*)context;
    const mcontext_t &mcontext = ctx->uc_mcontext;

    if (sig == SIGILL || sig == SIGFPE || sig == SIGSEGV || sig == SIGBUS || sig == SIGTRAP)
    {
#if __APPLE__
        const uint ecode = mcontext->__es.__err;
#else
        const greg_t ecode = mcontext.gregs[REG_ERR];
#endif
        string msg0 = str_format("Invalid %s to %p", (ecode&4) ? "Exec" : (ecode&2) ? "Write" : "Read", siginfo->si_addr);
        ReportPOSIX(msg0);
        message += msg0 + "\n";
    }

    string mmsg;
#if __APPLE__
#ifdef __LP64__
    mmsg = str_format("PC/RIP: %#llx SP/RSP: %#llx, FP/RBP: %#llx",
                      mcontext->__ss.__rip, mcontext->__ss.__rsp, mcontext->__ss.__rbp);
#else
    mmsg = str_format("PC/EIP: %#x SP/ESP: %#x, FP/EBP: %#x",
                      mcontext->__ss.__eip, mcontext->__ss.__esp, mcontext->__ss.__ebp);
#endif
#else
#ifdef __LP64__
    mmsg = str_format("PC/RIP: %#llx SP/RSP: %#llx, FP/RBP: %#llx",
                      mcontext.gregs[REG_RIP], mcontext.gregs[REG_RSP], mcontext.gregs[REG_RBP]);
#else
    mmsg = str_format("PC/EIP: %#x SP/ESP: %#x, FP/EBP: %#x",
                      mcontext.gregs[REG_EIP], mcontext.gregs[REG_ESP], mcontext.gregs[REG_EBP]);
#endif
#endif
    ReportPOSIX(mmsg);
    message += mmsg + "\n";
    
    if (sig == SIGTERM || sig == SIGINT)
    {
        if (OLG_DoClose()) {
            ReportPOSIX("Caught Control-C and already closing - Calling exit()\n");
            std::_Exit(1);
        }
        g_signaldepth--;
        return;
    }

    posix_print_stacktrace();
    g_signaldepth--;

    posix_oncrash(str_format("Reassembly Caught Signal\n%s", message.c_str()).c_str());
    std::_Exit(1);
}

void posix_set_signal_handler()
{
    if (!OLG_EnableCrashHandler())
        return;
#if 0
    /* setup alternate stack */
    {
        static uint8_t alternate_stack[SIGSTKSZ];

        stack_t ss = {};
        /* malloc is usually used here, I'm not 100% sure my static allocation
           is valid but it seems to work just fine. */
        ss.ss_sp = (void*)alternate_stack;
        ss.ss_size = SIGSTKSZ;
        ss.ss_flags = 0;
 
        if (sigaltstack(&ss, NULL) != 0) {
            ReportPOSIX("signalstack failed: %s", strerror(errno));
        }
    }
#endif
 
    /* register our signal handlers */
    {
        struct sigaction sig_action = {};
        sig_action.sa_sigaction = posix_signal_handler;
        sigemptyset(&sig_action.sa_mask);
 
#ifdef __APPLE__
        /* for some reason we backtrace() doesn't work on osx
           when we use an alternate stack */
        sig_action.sa_flags = SA_SIGINFO;
#else
        sig_action.sa_flags = SA_SIGINFO;// | SA_ONSTACK;
#endif
 
        const int signals[] = { SIGSEGV, SIGFPE, SIGINT, SIGILL, SIGTERM, SIGABRT };
        foreach (int sig, signals) {
            if (sigaction(sig, &sig_action, NULL) != 0) {
                ReportPOSIX("sigaction failed: %s", strerror(errno));
            }
        }
    }
}

void OL_Terminate(const char* message)
{
    if (!OLG_EnableCrashHandler())
        return;
    if (!OLG_OnTerminate())
        return;

    posix_print_stacktrace();
    posix_oncrash(str_format("Terminated: %s\n", message).c_str());
    std::_Exit(1);
}
