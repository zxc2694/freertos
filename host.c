#include "host.h"
#include "string.h"

/* Semihost system call parameters */
union param_t
{
    int   pdInt;
    void *pdPtr;
    char *pdChrPtr;
};

typedef union param_t param;

static int host_call(enum HOST_SYSCALL action, void *arg) __attribute__ ((naked));
static int host_call(enum HOST_SYSCALL action, void *arg)
{
    /* For Thumb-2 code use the BKPT instruction instead of SWI.
     * Refer to:
     * http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0471c/Bgbjhiea.html
     * http://en.wikipedia.org/wiki/ARM_Cortex-M#Cortex-M4 */
    __asm__( \
      "bkpt 0xAB\n"\
      "nop\n" \
      "bx lr\n"\
        :::\
    );
}

/* Detailed parameters please refer to
 * http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0471c/Bgbjhiea.html */
int host_open(const char *pathname, int flags)
{
    param semi_param[3] = {
        { .pdChrPtr = (char *) pathname },
        { .pdInt    = flags },
        { .pdInt    = strlen(pathname) }
    };

    return host_call(HOSTCALL_OPEN, semi_param);
}

size_t host_read(int fd, void *buf, size_t count)
{
    param semi_param[3] = {
        { .pdInt = fd },
        { .pdPtr = buf },
        { .pdInt = count }
    };

    return host_call(HOSTCALL_READ, semi_param);
}

size_t host_write(int fd, const void *buf, size_t count)
{
    param semi_param[3] = {
        { .pdInt = fd },
        { .pdPtr = (void *) buf },
        { .pdInt = count }
    };

    return host_call(HOSTCALL_WRITE, semi_param);
}

int host_close(int fd)
{
    return host_call(HOSTCALL_CLOSE, (void *)&fd);
}

int host_system(char *cmd, int str_len)
{
    param semi_param[3] = {
        { .pdPtr = (void *) cmd },
        { .pdInt = str_len }
    };
    return host_call(HOSTCALL_SYSTEM, semi_param);
}
