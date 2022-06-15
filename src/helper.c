#include "seq/tld-seq.h"
#include "string/str.h"
#include "tld.h"
#include <stdio.h>
#include <stdlib.h>


int main(void)
{
        char buffer[1024];
        tld_strbuf* b;

        uint8_t rev[256];
        for(int i = 0; i < 256;i++){
                rev[i] = i;
        }
        rev['A'] = 'T';
        rev['a'] = 't';

        rev['C'] = 'G';
        rev['c'] = 'g';

        rev['G'] = 'C';
        rev['g'] = 'c';

        rev['T'] = 'A';
        rev['t'] = 'a';

        tld_strbuf_alloc(&b,1024);

        tld_append(b, "const uint8_t comp_table[256] = {\n");

        for(int i = 0; i < 256;i++){
                snprintf(buffer, 1024,"%3d,", rev[i]);
                tld_append(b,buffer);
                if(i != 0 && (i +1) % 16 == 0){
                               tld_append_char(b, '\n');
                }else  if(i != 0 && (i+1) %4 == 0){
                        tld_append_char(b, ' ');
                }
        }
        b->len--;
        tld_append(b, "\n};\n");

        fprintf(stdout,"%s", TLD_STR(b));


        tld_strbuf_free(b);

        return EXIT_SUCCESS;

}
