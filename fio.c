#include <string.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include <unistd.h>
#include <stdarg.h>
#include "fio.h"
#include "filesystem.h"
#include "osdebug.h"
#include "hash-djb2.h"

static struct fddef_t fio_fds[MAX_FDS];

static ssize_t stdin_read(void * opaque, void * buf, size_t count) {
    return 0;
}

static ssize_t stdout_write(void * opaque, const void * buf, size_t count) {
    int i;
    const char * data = (const char *) buf;
    
    for (i = 0; i < count; i++)
        send_byte(data[i]);
    
    return count;
}

static xSemaphoreHandle fio_sem = NULL;

__attribute__((constructor)) void fio_init() {
    memset(fio_fds, 0, sizeof(fio_fds));
    fio_fds[0].fdread = stdin_read;
    fio_fds[1].fdwrite = stdout_write;
    fio_fds[2].fdwrite = stdout_write;
    fio_sem = xSemaphoreCreateMutex();
}

struct fddef_t * fio_getfd(int fd) {
    if ((fd < 0) || (fd >= MAX_FDS))
        return NULL;
    return fio_fds + fd;
}

static int fio_is_open_int(int fd) {
    if ((fd < 0) || (fd >= MAX_FDS))
        return 0;
    int r = !((fio_fds[fd].fdread == NULL) &&
              (fio_fds[fd].fdwrite == NULL) &&
              (fio_fds[fd].fdseek == NULL) &&
              (fio_fds[fd].fdclose == NULL) &&
              (fio_fds[fd].opaque == NULL));
    return r;
}

static int fio_findfd() {
    int i;
    
    for (i = 0; i < MAX_FDS; i++) {
        if (!fio_is_open_int(i))
            return i;
    }
    
    return -1;
}

int fio_is_open(int fd) {
    int r = 0;
    xSemaphoreTake(fio_sem, portMAX_DELAY);
    r = fio_is_open_int(fd);
    xSemaphoreGive(fio_sem);
    return r;
}

int fio_open(fdread_t fdread, fdwrite_t fdwrite, fdseek_t fdseek, fdclose_t fdclose, void * opaque) {
    int fd;
//    DBGOUT("fio_open(%p, %p, %p, %p, %p)\r\n", fdread, fdwrite, fdseek, fdclose, opaque);
    xSemaphoreTake(fio_sem, portMAX_DELAY);
    fd = fio_findfd();
    
    if (fd >= 0) {
        fio_fds[fd].fdread = fdread;
        fio_fds[fd].fdwrite = fdwrite;
        fio_fds[fd].fdseek = fdseek;
        fio_fds[fd].fdclose = fdclose;
        fio_fds[fd].opaque = opaque;
    }
    xSemaphoreGive(fio_sem);
    
    return fd;
}

ssize_t fio_read(int fd, void * buf, size_t count) {
    ssize_t r = 0;
//    DBGOUT("fio_read(%i, %p, %i)\r\n", fd, buf, count);
    if (fio_is_open_int(fd)) {
        if (fio_fds[fd].fdread) {
            r = fio_fds[fd].fdread(fio_fds[fd].opaque, buf, count);
        } else {
            r = -3;
        }
    } else {
        r = -2;
    }
    return r;
}

ssize_t fio_write(int fd, const void * buf, size_t count) {
    ssize_t r = 0;
//    DBGOUT("fio_write(%i, %p, %i)\r\n", fd, buf, count);
    if (fio_is_open_int(fd)) {
        if (fio_fds[fd].fdwrite) {
            r = fio_fds[fd].fdwrite(fio_fds[fd].opaque, buf, count);
        } else {
            r = -3;
        }
    } else {
        r = -2;
    }
    return r;
}

off_t fio_seek(int fd, off_t offset, int whence) {
    off_t r = 0;
//    DBGOUT("fio_seek(%i, %i, %i)\r\n", fd, offset, whence);
    if (fio_is_open_int(fd)) {
        if (fio_fds[fd].fdseek) {
            r = fio_fds[fd].fdseek(fio_fds[fd].opaque, offset, whence);
        } else {
            r = -3;
        }
    } else {
        r = -2;
    }
    return r;
}

int fio_close(int fd) {
    int r = 0;
//    DBGOUT("fio_close(%i)\r\n", fd);
    if (fio_is_open_int(fd)) {
        if (fio_fds[fd].fdclose)
            r = fio_fds[fd].fdclose(fio_fds[fd].opaque);
        xSemaphoreTake(fio_sem, portMAX_DELAY);
        memset(fio_fds + fd, 0, sizeof(struct fddef_t));
        xSemaphoreGive(fio_sem);
    } else {
        r = -2;
    }
    return r;
}

void fio_set_opaque(int fd, void * opaque) {
    if (fio_is_open_int(fd))
        fio_fds[fd].opaque = opaque;
}

#define stdin_hash 0x0BA00421
#define stdout_hash 0x7FA08308
#define stderr_hash 0x7FA058A3

static int devfs_open(void * opaque, const char * path, int flags, int mode) {
    uint32_t h = hash_djb2((const uint8_t *) path, -1);
//    DBGOUT("devfs_open(%p, \"%s\", %i, %i)\r\n", opaque, path, flags, mode);
    switch (h) {
    case stdin_hash:
        if (flags & (O_WRONLY | O_RDWR))
            return -1;
        return fio_open(stdin_read, NULL, NULL, NULL, NULL);
        break;
    case stdout_hash:
        if (flags & O_RDONLY)
            return -1;
        return fio_open(NULL, stdout_write, NULL, NULL, NULL);
        break;
    case stderr_hash:
        if (flags & O_RDONLY)
            return -1;
        return fio_open(NULL, stdout_write, NULL, NULL, NULL);
        break;
    }
    return -1;
}

void register_devfs() {
    DBGOUT("Registering devfs.\r\n");
    register_fs("dev", devfs_open, NULL);
}

int sprintf ( char * str, const char * format, ... )//only support %s (string), %c (charater) and %i(%d) (integer)
{
    va_list para;
    va_start(para,format);
    int curr_pos=0;
    char ch[]={'0','\0'};
    char integer[11];
    str[0]='\0';
    while(format[curr_pos]!='\0')
    {
        if(format[curr_pos]!='%')
        {
            ch[0]=format[curr_pos];
            strcat(str,ch);
        }
        else
        {
            switch(format[++curr_pos])
            {
                case 's':
                    strcat(str,va_arg(para,char*));
                    break;
                case 'c':
                    ch[0]=(char)va_arg(para,int);
                    strcat(str,ch);
                    break;
                case 'i':
                case 'd':
                    strcat(str,itoa(va_arg(para,int),integer));
                    break;
                case 'u':
                    strcat(str,itoa(va_arg(para,unsigned),integer));
                    break;
                default:
                    break;
            }
        }
        curr_pos++;
    }
    va_end(para);
    return strlen(str);
}

int printf ( const char * format, ... )
{
    char str[128];
    va_list para;
    va_start(para,format);
    int curr_pos=0;
    char ch[]={'0','\0'};
    char integer[11];
    str[0]='\0';
    while(format[curr_pos]!='\0')
    {
        if(format[curr_pos]!='%')
        {
            ch[0]=format[curr_pos];
            strcat(str,ch);
        }
        else
        {
            switch(format[++curr_pos])
            {
                case 's':
                    strcat(str,va_arg(para,char*));
                    break;
                case 'c':
                    ch[0]=(char)va_arg(para,int);
                    strcat(str,ch);
                    break;
                case 'i':
                case 'd':
                    strcat(str,itoa(va_arg(para,int),integer));
                    break;
                case 'u':
                    strcat(str,itoa(va_arg(para,unsigned),integer));
                    break;
                default:
                    break;
            }
        }
        curr_pos++;
    }
    va_end(para);
    return fio_write(1,str,strlen(str));
}


void Puts(char *msg)
{
	if(!msg)return;
	fio_write(1,msg,strlen(msg));
}

void Print(char *msg)
{
	if(!msg)return;
	char newLine[]="\n\r";
	fio_write(1,msg,strlen(msg));
	fio_write(1,newLine,strlen(newLine));
}

void Print_nextLine()
{
	char newLine[]="\n\r";
	fio_write(1,newLine,strlen(newLine));
}


/* Ref andy79923 mmtest */
#define MIN_ALLOC_SIZE 256
#define CIRCBUFSIZE (configTOTAL_HEAP_SIZE/MIN_ALLOC_SIZE)
#define MMTEST_NUM 200

struct slot {
    void *pointer;
    unsigned int size;
    unsigned int lfsr;
};

static struct slot slots[CIRCBUFSIZE];

static unsigned int circbuf_size(unsigned int write_pointer, unsigned int read_pointer)
{
    return (write_pointer + CIRCBUFSIZE - read_pointer) % CIRCBUFSIZE;
}

static unsigned int lfsr = 0xACE1;
// Get a pseudorandom number generator from Wikipedia
static int prng(void)
{
    /*static unsigned int bit;
    // taps: 16 14 13 11; characteristic polynomial: x^16 + x^14 + x^13 + x^11 + 1
    bit  = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5) ) & 1;
    lfsr =  (lfsr >> 1) | (bit << 15);*/


    __asm__ (
             "mov r0, %1             \n" // r0=lfsr
             "eor r1, r0, r0, lsr #2 \n" // r1 = (lfsr >> 0) ^ (lfsr >> 2)
             "eor r1, r1, r0, lsr #3 \n" // r1 = r1 ^ (lfsr >> 3)
             "eor r1, r1, r0, lsr #5 \n" // r1 = r1 ^ (lfsr >> 5)
             "and r1, #1             \n" // r1 = r1 & 1
             "lsl r1, #15            \n" // r1 = r1 << 15
             "orr r1, r1, r0, lsr #1 \n" // r1 = (lfsr >> 1) | (bit << 15);
             "mov %0, r1             \n" // lfsr = r1
             :"=r"(lfsr)
             :"r"(lfsr)
             :"r0","r1"
    );
    return lfsr & 0xffff;

}

void mmtest_fio_function(char *str)
{
    int i,j, size;
    char *p;
    unsigned int write_pointer = 0;
    unsigned int read_pointer = 0;

    for(j=0; j<MMTEST_NUM; j++)
    {
        do{
            size = prng() &  0x7FF;
        }while(size<MIN_ALLOC_SIZE);

        printf("try to allocate %d bytes\r\n", size);
        p = (char *) pvPortMalloc(size);
        printf("malloc returned %d\r\n", p);

        if (p == NULL || (write_pointer+1)%CIRCBUFSIZE == read_pointer) {
            // can't do new allocations until we free some older ones
            while (circbuf_size(write_pointer,read_pointer) > 0) {
                // confirm that data didn't get trampled before freeing
                p = slots[read_pointer].pointer;
                lfsr = slots[read_pointer].lfsr;  // reset the PRNG to its earlier state
                size = slots[read_pointer].size;
                read_pointer++;
                read_pointer %= CIRCBUFSIZE;
                printf("free a block, size %d\r\n", size);
                for (i = 0; i < size; i++) {
                    unsigned char u = p[i];
                    unsigned char v = (unsigned char) prng();
                    if (u != v) {
                        printf("OUCH: u=%02X, v=%02X\r\n", u, v);
                        return;
                    }
                }
                vPortFree(p);
                if ((prng() & 1) == 0) break;
            }
            send_byte('\r');
            send_byte('\n');
        } else {
            printf("allocate a block, size %d\r\n\r\n", size);
            if (circbuf_size(write_pointer, read_pointer) == CIRCBUFSIZE - 1) {
                fio_write(1,"circular buffer overflow\r\n",24);
                return;
            }
            slots[write_pointer].pointer=p;
            slots[write_pointer].size=size;
            slots[write_pointer].lfsr=lfsr;
            write_pointer++;
            write_pointer %= CIRCBUFSIZE;
            for (i = 0; i < size; i++) {
                p[i] = (unsigned char) prng();
            }
        }
    }
    do{
        p = slots[read_pointer].pointer;
        lfsr = slots[read_pointer].lfsr;  // reset the PRNG to its earlier state
        size = slots[read_pointer].size;
        read_pointer++;
        read_pointer %= CIRCBUFSIZE;
        printf("free a block, size %d\r\n", size);
        for (i = 0; i < size; i++) {
            unsigned char u = p[i];
            unsigned char v = (unsigned char) prng();
            if (u != v) {
                printf("OUCH: u=%02X, v=%02X\r\n", u, v);
                return;
            }
        }
        vPortFree(p);
    }while(read_pointer!=write_pointer);
}


void Read_Input(char *str,int MAX_SERIAL_STR)
{
	char ch;
	char newLine[] = "\n\r";
	int curr_char, done;
	curr_char = 0;
	done = 0;
	str[curr_char] = '\0';
	do{
		/* Receive a byte from the RS232 port (this call will block). */
         	ch=receive_byte();

		if (curr_char >= MAX_SERIAL_STR-1 || (ch == '\r') || (ch == '\n')){
			str[curr_char] = '\0';
			done = -1;
			/* Otherwise, add the character to the response string. */
		}
		else if(ch == 127){ //press the backspace key
                	if(curr_char!=0){
                    		curr_char--;
                    		fio_write(1,"\b \b", 3);
              		}
            	}
            	else if(ch == 27){  //press up, down, left, right, home, page up, delete, end, page down
                	ch=receive_byte();
                	if(ch != '['){
                    		str[curr_char++] = ch;
                    		fio_write(1, &ch, 1);
                	}
                	else{
                    		ch=receive_byte();
                   		 if(ch >= '1' && ch <= '6'){
                        		ch=receive_byte();
                    		}
                	}
            	}
		else{
			str[curr_char++] = ch;
			fio_write(1, &ch, 1);
		}
	} while (!done);

        Print_nextLine();
	return str;
}
