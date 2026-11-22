#include "tld-core.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define TYPE_MARGIN 8
#define MESSAGE_MARGIN 22

#define AT __FILE__ " line " tld_tostring(__LINE__)

/* #define AT __FILE__ " line " tld_tostring(__LINE__) */

tld_internal void vlog(uint8_t type,FILE* f_ptr,const char *file, int line,const char *format,  va_list argp);

tld_internal int get_time(char* time_ptr, int size);

void vlog( uint8_t type, FILE* f_ptr,const char *file, int line,const char *format,  va_list argp)
{
        char time_string[BUFSIZ];
        int col = (f_ptr == stderr) && tld_term_color();
        if(type != MESSAGE_TYPE_ERROR && tld_verbosity() < 1){
                return;
        }

        if(get_time(time_string,BUFSIZ) != OK){
                fprintf(stderr,"notime");
        }
        fprintf(f_ptr,"%*s: ",MESSAGE_MARGIN,time_string);
        switch (type) {
        case MESSAGE_TYPE_MSG: {
                fprintf(f_ptr,"%*s: ",TYPE_MARGIN,"LOG ");
                vfprintf(f_ptr, format, argp);
                fprintf(f_ptr,"\n");
                break;
        }
        case MESSAGE_TYPE_WARNING: {
                fprintf(f_ptr,col ? "\033[1;33m%*s\033[0m: " : "%*s: ",TYPE_MARGIN,"WARNING ");
                vfprintf(f_ptr, format, argp);
                fprintf(f_ptr,"\n");
                break;
        }
        case MESSAGE_TYPE_ERROR: {
                fprintf(f_ptr,col ? "\033[1;31m%*s\033[0m: " : "%*s: ",TYPE_MARGIN,"ERROR ");
                vfprintf(f_ptr, format, argp);
                if(file){
                        fprintf(f_ptr," (%s line %d)\n",file,line);
                }else{
                        fprintf(f_ptr,"\n");
                }
                break;
        }
        default:
                fprintf(f_ptr,"Unrecognized Message type (in tld) - this should never happen");
                break;
        }

        fflush(f_ptr);
}

int get_time(char* time_ptr, int size)
{
        /* time_t timer = 0; */
        /* struct tm* tm_info = NULL; */

        /* timer = time(NULL); */
        /* localtime_r(&timer, tm_info); */

        struct tm newtime;
        time_t ltime = 0;
        /* char buf[50]; */

        ltime=time(&ltime);
        localtime_r(&ltime, &newtime);


        if(!strftime(time_ptr, size, "[%F %H:%M:%S] ", &newtime)){
                /* ERROR_MSG("write failed"); */
                fprintf(stderr,"%*s: ",TYPE_MARGIN,"ERROR ");
                fprintf(stderr, "Could not get the time");
                fprintf(stderr," (%s)\n",AT);
                goto ERROR;
        }
        return OK;
ERROR:
        return FAIL;
}

void tld_message(uint8_t type, const char *format, ...)
{
        va_list argp;
        va_start(argp, format);
        vlog(type,stderr,NULL,0,format, argp);
        va_end(argp);
}

void tld_message_at(uint8_t type, const char *file, int line, const char *format, ...)
{
        va_list argp;
        va_start(argp, format);
        vlog(type,stderr,file,line,format, argp);
        va_end(argp);
}

int tld_verbosity(void)
{
        const char *v = getenv("TLD_VERBOSE");
        if(v && v[0] >= '0' && v[0] <= '9' && v[1] == 0){
                return v[0] - '0';
        }
        return 1;
}

int tld_term_color(void)
{
        const char *nc = getenv("NO_COLOR");
        const char *term = getenv("TERM");
        if(nc && *nc){
                return 0;
        }
        if(term && strcmp(term, "dumb") == 0){
                return 0;
        }
        return isatty(fileno(stderr));
}

void tld_section(const char *format, ...)
{
        char title[256];
        va_list argp;
        int len;
        int col = tld_term_color();
        if(tld_verbosity() < 1){
                return;
        }
        va_start(argp, format);
        len = vsnprintf(title, sizeof title, format, argp);
        va_end(argp);
        if(len < 0){
                return;
        }
        if(len >= (int)sizeof title){
                len = (int)sizeof title - 1;
        }
        if(col){
                fprintf(stderr, "\033[1;36m── %s ", title);
                for(int i = len + 4; i < 60; i++){
                        fputs("─", stderr);
                }
                fputs("\033[0m\n", stderr);
        }else{
                fprintf(stderr, "== %s ==\n", title);
        }
        fflush(stderr);
}

void tld_kv(const char *key, const char *format, ...)
{
        va_list argp;
        if(tld_verbosity() < 1){
                return;
        }
        if(tld_term_color()){
                fprintf(stderr, "   \033[2m%-9s\033[0m ", key);
        }else{
                fprintf(stderr, "   %-9s ", key);
        }
        va_start(argp, format);
        vfprintf(stderr, format, argp);
        va_end(argp);
        fputc('\n', stderr);
        fflush(stderr);
}

void tld_ok(const char *format, ...)
{
        va_list argp;
        if(tld_verbosity() < 1){
                return;
        }
        fputs(tld_term_color() ? " \033[1;32m✓\033[0m " : " OK ", stderr);
        va_start(argp, format);
        vfprintf(stderr, format, argp);
        va_end(argp);
        fputc('\n', stderr);
        fflush(stderr);
}

void tld_debug(const char *format, ...)
{
        va_list argp;
        int col;
        if(tld_verbosity() < 2){
                return;
        }
        col = tld_term_color();
        fputs(col ? " \033[2m· " : " . ", stderr);
        va_start(argp, format);
        vfprintf(stderr, format, argp);
        va_end(argp);
        fputs(col ? "\033[0m\n" : "\n", stderr);
        fflush(stderr);
}

void tld_progress(long done, long total, const char *label)
{
        static int last = -1;
        int pct;
        int fill;
        if(tld_verbosity() < 1 || total <= 0){
                return;
        }
        if(done < 0){
                done = 0;
        }
        if(done > total){
                done = total;
        }
        pct = (int)(100.0 * (double)done / (double)total);
        if(!isatty(fileno(stderr))){
                if(done == total){
                        fprintf(stderr, "   %s %ld/%ld done\n", label ? label : "", done, total);
                }
                return;
        }
        if(pct == last && done != total){
                return;
        }
        last = (done == total) ? -1 : pct;
        fill = pct * 30 / 100;
        fprintf(stderr, "\r   %s [", label ? label : "");
        for(int i = 0; i < 30; i++){
                fputs(i < fill ? "#" : ".", stderr);
        }
        fprintf(stderr, "] %3d%% %ld/%ld\033[K", pct, done, total);
        if(done == total){
                fputc('\n', stderr);
        }
        fflush(stderr);
}
