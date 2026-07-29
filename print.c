/*
 * (C) 2026, Cornell University
 * All rights reserved.
 *
 * Description: this file is the same as hello.c in P0.
 * Feel free to add your P0 solution code in this file.
 */

#include <stdlib.h>  // for itoa() and utoa()
#include <string.h>  // for strlen() and strcat()
#include <stdarg.h>  // for va_start(), va_end(), va_arg() and va_copy()


void terminal_write(const char *str, int len) {
    for (int i = 0; i < len; i++) {
        *(char*)(0x10000000) = str[i];
    }
}

extern char __heap_start, __heap_end;
static char* brk = &__heap_start;
char* _sbrk(int size) {
    if (brk + size > (char*)&__heap_end) {
        terminal_write("_sbrk: heap grows too large\r\n", 29);
        return NULL;
    }

    char* old_brk = brk;
    brk += size;
    return old_brk;
}


/* Uncomment the code block below when implementing formatted output.
 */


void ulltoa(unsigned long long val, char* buf,int radix)
{
    char temp[65];
    int i=0;
    
    if(val ==0)
    {
        strcpy(buf,"0");
        return;
    }

    while(val>0)
    {
        int rem = val%radix;
        if(rem<10)
            temp[i]='0'+rem;
        else
        {
            temp[i]= rem-10 + 'a';
        }    
        i++;
        val /=radix;
    }
    
    int j=0;
    i--;
    for(;i>=0;++j,--i)
        buf[j]=temp[i];
    
    buf[j] = '\0';
}


void format_to_str(char* out, const char* fmt, va_list args) {
    for(out[0] = 0; *fmt != '\0'; fmt++) {
        if (*fmt != '%') {
            strncat(out, fmt, 1);
        } else {
            fmt++;
            if (*fmt == 's') {
                strcat(out, va_arg(args, char*));
            } else if (*fmt == 'd') {
                itoa(va_arg(args, int), out + strlen(out), 10);
            } else if (*fmt == 'u'){
                utoa(va_arg(args,unsigned int),out+strlen(out),10);
            } else if (*fmt =='l' && *(fmt+1) =='l' && *(fmt+2) =='u')
            {
                ulltoa(va_arg(args,unsigned long long),out+strlen(out),10);
                fmt +=2;
            } else if (*fmt == 'c'){
                char c = (char)va_arg(args,int);
                int len = strlen(out);
                out[len] = c;
                out[len+1] = '\0';
                //utoa((char)va_arg(args,unsigned int),out+strlen(out),10); 这样会打印对应的
            } else if (*fmt=='x'){
                utoa(va_arg(args,unsigned int),out+strlen(out),16);
            } else if (*fmt == 'p'){
                strcat(out,"0x");
                utoa((unsigned int)va_arg(args, void*),out+strlen(out),16);
            }
        }
    }
}

int format_to_str_len(const char* fmt, va_list args) {
    int len = 0;
    for(; *fmt != '\0'; fmt++) {
        char tmp[65];
        if (*fmt != '%') {
            len++;
        } else {
            fmt++;
            if (*fmt == 's') {
                len += strlen(va_arg(args, char*));
            } else if (*fmt == 'd') {
                itoa(va_arg(args, int), tmp, 10);
                len+= strlen(tmp);
            } else if (*fmt == 'u'){
                utoa(va_arg(args,unsigned int),tmp,10);
                len+= strlen(tmp);
            } else if (*fmt =='l' && *(fmt+1) =='l' && *(fmt+2) =='u')
            {
                ulltoa(va_arg(args,unsigned long long),tmp,10);
                len+= strlen(tmp);
                fmt +=2;
            } else if (*fmt == 'c'){
                va_arg(args, int); // args类似于迭代器
                len++;
            } else if (*fmt=='x'){
                utoa(va_arg(args,unsigned int),tmp,16);
                len+= strlen(tmp);
            } else if (*fmt == 'p'){
                utoa((unsigned int)va_arg(args, void*),tmp,16);
                len+= 2+strlen(tmp);
            }
        }
    }
    return len;
}

int printf(const char* format, ...) {
    va_list args;
    va_start(args, format);

    va_list args_copy;
    va_copy(args_copy, args);
    unsigned int len = format_to_str_len(format, args_copy);
    char *buf = malloc(len);

    format_to_str(buf, format, args);
    va_end(args);
    terminal_write(buf, strlen(buf));

    va_end(args_copy);
    free(buf);

    return 0;
}

int main() {
    char* msg = "Hello, World!\n\r";
    terminal_write(msg, 15);

    /* Uncomment this line of code when implementing formatted output. */
    printf("%s-%d is awesome!\n\r", "egos", 2000); 

    /*Test*/
    printf("%c is character $\n\r", '$');
    printf("%c is character 0\n\r", (char)48);
    printf("%x is integer 1234 in hexadecimal\n\r", 1234);
    printf("%u is the maximum of unsigned int\n\r", (unsigned int)0xFFFFFFFF);
    printf("%p is the hexadecimal address of the hello-world string\n\r", msg);
    printf("%llu is the maximum of unsigned long long\n\r", 0xFFFFFFFFFFFFFFFFULL);

    return 0;
}
