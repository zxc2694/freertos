#ifndef __HOST_H__
#define __HOST_H__
#include <stddef.h>

/* Refer to SYS_OPEN ARM document */
enum File_Type_t {
    OPEN_RD = 0,
    OPEN_RD_BIN, 
    OPEN_RD_ONLY,
    OPEN_RD_ONLY_BIN,
    OPEN_WR,
    OPEN_WR_BIN,
    OPEN_WR_ONLY,
    OPEN_WR_ONLY_BIN,
    OPEN_APPEND,
    OPEN_APPEND_BIN,
    OPEN_APPEND_ONLY,
    OPEN_APPEND_ONLY_BIN
};

/* Referred to:
 * ARM documents */
enum HOST_SYSCALL {
    HOSTCALL_OPEN        = 0x01,
    HOSTCALL_CLOSE       = 0x02,
    HOSTCALL_WRITEC      = 0x03,
    HOSTCALL_WRITE0      = 0x04,
    HOSTCALL_WRITE       = 0x05,
    HOSTCALL_READ        = 0x06,
    HOSTCALL_READC       = 0x07,
    HOSTCALL_ISERROR     = 0x08,
    HOSTCALL_ISTTY       = 0x09,
    HOSTCALL_SEEK        = 0x0A,
    HOSTCALL_FLEN        = 0x0C,
    HOSTCALL_REMOVE      = 0x0E,
    HOSTCALL_TMPNAM      = 0x0D,
    HOSTCALL_RENAME      = 0x0F,
    HOSTCALL_CLOCK       = 0x10,
    HOSTCALL_TIME        = 0x11,
    HOSTCALL_SYSTEM      = 0x12,
    HOSTCALL_ERRNO       = 0x13,
    HOSTCALL_GET_CMDLINE = 0x15,
    HOSTCALL_HEAPINFO    = 0x16,
    HOSTCALL_ELAPSED     = 0x30,
    HOSTCALL_TICKFREQ    = 0x31
};

size_t host_read(int fd, void *buf, size_t count);
size_t host_write(int fd, const void *buf, size_t count);

int host_open(const char *pathname, int flags);
int host_close(int fd);
int host_system(char *cmd, int str_len);
#endif
