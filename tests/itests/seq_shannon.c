#include "tld.h"
#include <stdio.h>

static int calculate_seq_shannon(struct tl_seq* s,double* ret);
int main(int argc, char *argv[])
{
        struct file_handler* f = NULL;
        struct tl_seq_buffer* sb = NULL;
        double shannon = 0.0;
        if(argc > 1){
                RUN(open_fasta_fastq_file(&f, argv[1], TLSEQIO_READ));
                while(1){
                        RUN(read_fasta_fastq_file(f, &sb, 1000));
                        //detect_format(sb);
                        //total_r+= sb->num_seq;
                        LOG_MSG("Finished reading chunk: found %d ",sb->num_seq);
                        if(sb->num_seq == 0){
                                break;
                        }
                        for(int i = 0; i < sb->num_seq;i++){
                                calculate_seq_shannon(sb->sequences[i],&shannon);
                                fprintf(stdout,"%f\n", shannon);
                        }
                }
        }
        free_tl_seq_buffer(sb);
        RUN(close_seq_file(&f));
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}

int calculate_seq_shannon(struct tl_seq* s,double* ret)
{

        double dist[256];
        double shannon = 0.0;
        uint8_t* seq = s->seq->str;
        for(int i = 0; i < 256;i++){
                dist[i] = 0.0;
        }
        for(int i = 0; i < s->len;i++){
                dist[seq[i]]++;
        }

        tld_shannon(dist,256,&shannon);

        *ret = shannon;
        return OK;
}
