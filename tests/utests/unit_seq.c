#include "tld.h"
#include <stdio.h>

int main(int argc, char *argv[])
{
        struct file_handler* f = NULL;
        struct file_handler* f_out = NULL;
        struct file_handler* f_out_bam = NULL;
        struct tl_seq_buffer* sb = NULL;
        char* aux;
        int i;
        if(argc > 1){
                RUN(open_fasta_fastq_file(&f, argv[1], TLSEQIO_READ));

//#endif
                //int total_r = 0;
                //int total_w = 0;
                while(1){
                        RUN(read_fasta_fastq_file(f, &sb, 1000));
                        detect_format(sb);
                        //total_r+= sb->num_seq;
                        LOG_MSG("Finished reading chunk: found %d ",sb->num_seq);
                        if(sb->num_seq == 0){
                                break;
                        }
                }
        }
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}

