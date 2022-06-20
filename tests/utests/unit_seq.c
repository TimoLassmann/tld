#include "tld.h"
#include <stdio.h>

int main(int argc, char *argv[])
{
        struct file_handler* f = NULL;
        struct tl_seq_buffer* sb = NULL;

        if(argc > 1){
                RUN(open_fasta_fastq_file(&f, argv[1], TLSEQIO_READ));
                while(1){
                        RUN(read_fasta_fastq_file(f, &sb, 100000));
                        //detect_format(sb);
                        //total_r+= sb->num_seq;
                        LOG_MSG("Finished reading chunk: found %d ",sb->num_seq);
                        if(sb->num_seq == 0){
                                break;
                        }
                        for(int i = 0; i < sb->num_seq;i++){
                                /* fprintf(stdout, "%s %d\n%s\n",  sb->sequences[i]->name->str, sb->sequences[i]->seq->len,TLD_STR(sb->sequences[i]->seq)); */
                        }
                }
        }
        free_tl_seq_buffer(sb);
        RUN(close_seq_file(&f));
        return EXIT_SUCCESS;
ERROR:
        if(sb){
                free_tl_seq_buffer(sb);
        }
        if(f){
                RUN(close_seq_file(&f));
        }
        return EXIT_FAILURE;
}

