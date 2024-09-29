#ifndef TEMPLATE_H
#define TEMPLATE_H

#include "../core/tld-core.h"

#include "../string/str.h"

typedef struct tld_template_map {
        tld_strbuf** identifier;
        tld_strbuf** replacement;
        tld_strbuf* delim_start;
        tld_strbuf* delim_end;
        tld_strbuf* if_delim_start;
        tld_strbuf* if_delim_end;
        int n;
        int n_alloc;
        int n_rep;
        int status;
} tld_template_map;


/* tld_external  int parse_template(tld_strbuf *txt); */
/* tld_external  int parse_content(tld_strbuf *txt); */
tld_external  int tld_template_apply2(tld_strbuf *txt, tld_template_map *map);
tld_external  int tld_template_apply(tld_strbuf *txt, tld_template_map *map);
tld_external  int tld_template_chk(tld_template_map *map, int verbose);
tld_external  int tld_template_add(tld_template_map **map, char *id, char *rep);
tld_external  int tld_template_get(tld_template_map *m, char *key, char** val);
tld_external  int tld_template_init(tld_template_map **map, char **id, char **rep, int n);
tld_external void tld_template_free(tld_template_map *m);

#endif
