#include "template.h"

#include "../core/tld-core.h"
#include "../alloc/tld-alloc.h"
#include "../string/str.h"

#include <stdio.h>

#define TEMPLATE_OK 0
#define TEMPLATE_MISMATCH_DELIM 1
#define TEMPLATE_NOREP 2

tld_internal int find_open(tld_strbuf *txt,int offset, tld_strbuf *delim, int*pos);
tld_internal int find_end(tld_strbuf *txt,int offset, tld_strbuf* delim, int*pos);

int tld_template_apply(tld_strbuf *txt, tld_template_map *map)
{
        /* loop until there are no more valid reps  */
        tld_strbuf* out = NULL;
        tld_strbuf* pattern = NULL;
        int offset = 0;
        int start;
        int stop;
        int read_header = 0;

        map->status = TEMPLATE_OK;
        map->n_rep = 0;

        RUN(tld_strbuf_alloc(&out, 32));
        RUN(tld_strbuf_alloc(&pattern, 32));

        while(1){
                start = -1;
                stop  = -1;
                find_open(txt, offset, map->delim_start, &start);
                if(start != -1){
                        find_end(txt, start, map->delim_end, &stop);
                        if(stop != -1){
                                offset = stop;
                                pattern->len = 0;
                                for(int i = start; i < stop;i++){
                                        RUN(tld_append_char(pattern, txt->str[i]));
                                        /* fprintf(stdout,"%c",txt->str[i]); */
                                }
                                int internal_start = -1;
                                find_open(pattern, 0, map->delim_start, &internal_start);
                                if(internal_start != -1){
                                        map->status = TEMPLATE_MISMATCH_DELIM;
                                }

                                /* fprintf(stdout,"%s (%d - %d)\n", TLD_STR(pattern), start , stop); */
                                int idx = -1;
                                for(int i = 0; i < map->n;i++){
                                        if(strncmp(TLD_STR(pattern), TLD_STR(map->identifier[i]), MACRO_MIN(TLD_STRLEN(pattern), TLD_STRLEN(map->identifier[i]))) == 0){
                                                idx = i;
                                                break;
                                        }
                                }
                                if(idx != -1){
                                        /* fprintf(stdout,"%s -> %s \n", TLD_STR(map->identifier[idx]), TLD_STR(map->replacement[idx])); */

                                        for(int i = read_header; i < start-TLD_STRLEN(map->delim_start);i++){
                                                RUN(tld_append_char(out, txt->str[i]));
                                        }
                                        RUN(tld_append(out,TLD_STR(map->replacement[idx])));
                                        read_header = stop + TLD_STRLEN(map->delim_end);
                                        map->n_rep++;
                                }else{
                                        map->status = TEMPLATE_NOREP;
                                        /* WARNING_MSG("No replacement found"); */
                                }
                        }else{
                                map->status = TEMPLATE_MISMATCH_DELIM;
                                /* break; */
                        }
                }else{
                        break;
                }
        }
        if(read_header < TLD_STRLEN(txt)){
                RUN(tld_append(out, TLD_STR(txt) + read_header));
        }
        /* LOG_MSG("%s", TLD_STR(out)); */
        /* LOG_MSG("READ HEADER: %d", read_header); */
        RUN(tld_strbuf_copy(txt, out));

        tld_strbuf_free(pattern);
        tld_strbuf_free(out);
        return OK;
ERROR:
        if(pattern){
                tld_strbuf_free(pattern);
        }
        if(out){
                tld_strbuf_free(out);
        }
        return FAIL;

}

int tld_template_chk(tld_template_map *map, int verbose)
{
        if(verbose){
                LOG_MSG("%d replacement made", map->n_rep);
        }
        switch (map->status) {
        case TEMPLATE_OK:
                break;
        case TEMPLATE_NOREP:

                ERROR_MSG("tag found in text but no matching replacement!");
                break;
        case TEMPLATE_MISMATCH_DELIM:
                ERROR_MSG("Mismatched deliminters!");
                break;
        }
        return OK;
ERROR:
        return FAIL;
}

#undef TEMPLATE_OK
#undef TEMPLATE_MISMATCH_DELIM
#undef TEMPLATE_NOREP


int find_open(tld_strbuf *txt, int offset, tld_strbuf *delim,int*pos)
{
        int d_len = -1;
        int t_len = -1;

        t_len = TLD_STRLEN(txt);
        d_len = TLD_STRLEN(delim);
        int p = -1;
        if(offset >= TLD_STRLEN(txt)){
                return OK;
        }

        for(int i = offset; i < t_len-1;i++){
                int m = 0;
                for(int j = 0; j < d_len;j++){
                        if(txt->str[i+j] != delim->str[j]){
                                break;
                        }
                        m++;
                }
                if(m == d_len){
                        p = i+m;
                        break;
                }
        }
        *pos = p;
        return OK;
}

int find_end(tld_strbuf *txt, int offset, tld_strbuf *delim,int*pos)
{
        int d_len = -1;
        int t_len = -1;

        t_len = TLD_STRLEN(txt);
        d_len = TLD_STRLEN(delim);
        int p = -1;
        if(offset >= TLD_STRLEN(txt)){
                return OK;
        }

        for(int i = offset; i < t_len-1;i++){
                int m = 0;
                for(int j = 0; j < d_len;j++){
                        if(txt->str[i+j] != delim->str[j]){
                                break;
                        }
                        m++;
                }
                if(m == d_len){
                        p = i;
                        break;
                }
        }
        *pos = p;
        return OK;
}


int tld_template_init(tld_template_map **map, char **id, char **rep, int n)
{
        tld_template_map* m = NULL;
        MMALLOC(m, sizeof(tld_template_map));
        m->identifier = NULL;
        m->replacement = NULL;
        m->delim_start = NULL;
        m->delim_end = NULL;
        m->n = 0;
        m->n_alloc = n;
        m->status = 0;
        m->n_rep = 0;
        MMALLOC(m->identifier, sizeof(tld_strbuf*) * m->n_alloc);
        MMALLOC(m->replacement, sizeof(tld_strbuf*) * m->n_alloc);

        m->n = 0;
        for(int i = 0; i < n;i++){
                m->identifier[i] = NULL;
                m->replacement[i] = NULL;
                tld_strbuf_alloc(&m->identifier[i], 16);
                tld_strbuf_alloc(&m->replacement[i], 16);

                tld_append(m->identifier[i], id[i]);
                tld_append(m->replacement[i], rep[i]);
                m->n++;
        }
        /* Default delim = {{ and }} */
        tld_strbuf_alloc(&m->delim_start, 3);
        tld_strbuf_alloc(&m->delim_end, 3);

        tld_append(m->delim_start, "{{");

        tld_append(m->delim_end,"}}");

        *map = m;

        return OK;
ERROR:
        tld_template_free(m);
        return FAIL;
}

void tld_template_free(tld_template_map *m)
{
        if(m){
                if(m->n){
                        for(int i = 0; i < m->n;i++){
                                tld_strbuf_free(m->identifier[i]);
                                tld_strbuf_free(m->replacement[i]);

                        }
                        MFREE(m->identifier);
                        MFREE(m->replacement);
                }
                if(m->delim_start){
                        tld_strbuf_free(m->delim_start);
                }
                if(m->delim_end){
                        tld_strbuf_free(m->delim_end);
                }
                MFREE(m);
        }
}
