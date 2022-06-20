#include "tld-seq.h"
#include "tld-seq-tables.h"
#include "../alloc/tld-alloc.h"
#include "../misc/misc.h"
#include "../string/str.h"
#include "../stats/basic.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <zlib.h>

#define TLSEQIO_FASTA 1
#define TLSEQIO_FASTQ 2
#define TLSEQIO_GZIPPED 4


#define TL_READ_BUFF_LEN 8388608
//#define TL_READ_BUFF_LEN 300
#define TL_OUT_LINE_LEN 70

#define TL_DEFAULT_COMPRESSION '1'

#define str(x)          # x
#define xstr(x)         str(x)


#define RUNP(EXP)                               \
        if((EXP) == NULL){                      \
                ERROR_MSG(ADDFAILED(EXP));      \
        }


struct file_handler{
        FILE* f_ptr;
        gzFile gz_f_ptr;
        uint8_t file_type;
        int trans[5][5];
        char start_symbol;
        char* read_buffer;
        int bytes_read;
        int pos;
        int read_state;
        int write_state;
        int gz;
        int n_parse_error;
};


/* static int detect_fasta_fastq(const char* b, int len, int* type); */
static int write_fasta_fastq(struct tl_seq_buffer* sb, struct file_handler* fh);
static int read_sequences(struct file_handler*fh, struct tl_seq_buffer* sb, int num);

static int parse_buf(struct file_handler *fh, struct tl_seq_buffer *sb, int num);
static int print_error_context(char* buf, int len, int point);
static int finalize_read(struct file_handler *fh, struct tl_seq_buffer *sb);
/* static int parse_buf_fasta(struct file_handler* fh, struct tl_seq_buffer* sb,int num); */
/* static int parse_buf_fastq(struct file_handler* fh, struct tl_seq_buffer* sb,int num); */

static int read_file_contents(struct file_handler* fh);

static int get_io_handler(struct file_handler** fh,const char* filename,int gz);
static void free_io_handler(struct file_handler* f);

static int write_fasta_to_buf(struct tl_seq* seq, char* buf, int* index,int* write_ok);
static int write_fastq_to_buf(struct tl_seq* seq, char* buf, int* index,int* write_ok);

static int read_until_eol(tld_strbuf *t, char *buf, int *pos);
static int skip_until_eol(char *buf, int *pos);

int write_seq_buf(struct tl_seq_buffer* sb, struct file_handler* fh)
{
        RUN(write_fasta_fastq(sb,fh));
        return OK;
ERROR:
        return FAIL;
}

int open_fasta_fastq_file(struct file_handler** fh,char* filename, int mode)
{
        struct file_handler* f = NULL;
        int type = 0;
        int status;


        if(mode == TLSEQIO_READ){
                ASSERT(tld_file_exists(filename) == OK,"File: %s does not exists",filename);
                RUN(get_io_handler(&f, filename, mode));
                RUN(read_file_contents(f));

                /* internal_detect_fasta_fastq(f->read_buffer , f->bytes_read, &type); */
                /* exit(0); */
                /* detect file type  */
                /* RUN(detect_fasta_fastq(f->read_buffer , f->bytes_read, &type)); */

                status = gzrewind(f->gz_f_ptr);
                if(status){
                        ERROR_MSG("gzrewind failed");
                }
                switch (type) {
                case FILE_TYPE_FASTA:
                        //LOG_MSG("Found fasta");
                        break;
                case FILE_TYPE_FASTQ:
                        //LOG_MSG("Found fastq");
                        break;
                case FILE_TYPE_UNDEFINED:
                        ERROR_MSG("Could not determine type of file: %s",filename);
                        break;

                default:
                        break;
                }
                f->file_type = type;
        }else{
                RUN(get_io_handler(&f, filename, mode));
        }
        *fh = f;
        return OK;
ERROR:
        free_io_handler(f);
        return FAIL;
}

int read_fasta_fastq_file(struct file_handler* fh, struct tl_seq_buffer** seq_buf, int num)
{
        struct tl_seq_buffer* sb = NULL;
        ASSERT(fh!= NULL, "No file handler");
        ASSERT(num > 0, "Need to read more than %d sequences",num);


        sb = *seq_buf;
        if(sb == NULL){
                //LOG_MSG("Allocating seqbuffer");
                RUN(alloc_tl_seq_buffer(&sb, num));
        }else{
                while(num > sb->malloc_num){
                        RUN(resize_tl_seq_buffer(sb));
                }
        }

        sb->is_fastq = 0;
        if(fh->file_type == FILE_TYPE_FASTQ){
                sb->is_fastq = 1;
        }

        RUN(reset_tl_seq_buffer(sb));

        /* reading  */
        RUN(read_sequences(fh,sb,num));

        /* if(sb->base_quality_offset == 0){ */
        /*         RUN(detect_format(sb)); */
        /* } */


        *seq_buf = sb;
        return OK;
ERROR:
        return FAIL;
}

int read_sequences(struct file_handler*fh, struct tl_seq_buffer* sb, int num)
{
        /* int i; */
        /* int max_len; */
        while(1){

                if(fh->pos){
                        //LOG_MSG("Finishing buffer");
                        /* if(fh->file_type == FILE_TYPE_FASTA){ */
                        /*         RUN(parse_buf_fasta(fh,sb,num)); */
                        /* }else if(fh->file_type == FILE_TYPE_FASTQ){ */
                        /*         RUN(parse_buf_fastq(fh,sb,num)); */
                        /* }else{ */
                        /*         ERROR_MSG("Unknown file type"); */
                        /* } */
                        RUN(parse_buf(fh,sb,num));

                        if(sb->num_seq == num){
                                break;
                        }
                }else{
                        //LOG_MSG("read new content");
                        if (gzeof (fh->gz_f_ptr)){
                                /* LOG_MSG("Nothing left"); */
                                break;
                        }
                        RUN(read_file_contents(fh));
                        if(!fh->bytes_read){
                                /* LOG_MSG("Nothing left"); */
                                break;
                        }
                        RUN(parse_buf(fh,sb,num));
                        /* if(fh->file_type == FILE_TYPE_FASTA){ */
                        /*         RUN(parse_buf_fasta(fh,sb,num)); */
                        /* }else if(fh->file_type == FILE_TYPE_FASTQ){ */
                        /*         RUN(parse_buf_fastq(fh,sb,num)); */
                        /* }else{ */
                        /*         ERROR_MSG("Unknown file type"); */
                        /* } */
                        if(sb->num_seq == num){
                                break;
                        }
                }
        }
        RUN(finalize_read(fh, sb));

        return OK;
ERROR:
        return FAIL;
}

int print_error_context(char* buf, int len, int point)
{
        tld_strbuf* tmp = NULL;
        int start;
        int end;
        tld_strbuf_alloc(&tmp, 1024);

        start = point - 20;
        end = point + 20;

        start = MACRO_MAX(start, 0);
        end = MACRO_MIN(end, len);

        for(int i = start; i < end; i++){
                tld_append_char(tmp, buf[i]);
        }
        LOG_MSG("%s", tmp->str);
        return OK;
}

int parse_buf(struct file_handler *fh, struct tl_seq_buffer *sb, int num)
{
        char* buf = NULL;
        int buf_len;
        int state;
        int pos;
        int i;
        char c;

        int first_seq =1;
        ASSERT(fh!= NULL, "No file handler");
        ASSERT(sb!= NULL, "No sequence buffer");
        buf = fh->read_buffer;
        pos = fh->pos;
        buf_len = fh->bytes_read;
        state = fh->read_state;
        for(i = pos;i < buf_len;i++){
                c = buf[i];
                switch (state) {
                case S_START:
                        if(c == '@' || c == '>'){
                                fh->start_symbol = c;
                                fh->trans[S_START][S_NAME]++;
                                state = S_NAME;
                                if(first_seq){
                                        first_seq = 0;
                                }
                                i++;
                                read_until_eol(sb->sequences[sb->num_seq]->name, buf, &i);
                        }
                        break;
                case S_NAME:
                        if(is_aa[(uint8_t) c] || is_nuc[(uint8_t)c]){
                                fh->trans[S_NAME][S_SEQ]++;
                                state = S_SEQ;

                                read_until_eol(sb->sequences[sb->num_seq]->seq, buf, &i);
                                sb->sequences[sb->num_seq]->len = sb->sequences[sb->num_seq]->seq->len;
                        }else if(c != 0){
                                fh->n_parse_error++;
                                WARNING_MSG("ERROR %d: A name line is not followed by a sequence line.",fh->n_parse_error);
                                print_error_context(buf, buf_len,i);
                                /* fprintf(stdout,"%s",buf); */
                        }
                        break;
                case S_SEQ:
                        if(is_aa[(uint8_t) c] || is_nuc[(uint8_t)c]){
                                fh->trans[S_SEQ][S_SEQ]++;
                                state = S_SEQ;

                                read_until_eol(sb->sequences[sb->num_seq]->seq, buf, &i);
                                sb->sequences[sb->num_seq]->len = sb->sequences[sb->num_seq]->seq->len;

                        }else if(c == fh->start_symbol){
                                if(first_seq){
                                        first_seq = 0;
                                }else{
                                        sb->num_seq++;
                                        if(sb->num_seq == num){
                                                fh->pos = i;
                                                fh->read_state = state;
                                                return OK;
                                        }
                                }

                                fh->trans[S_SEQ][S_NAME]++;
                                state = S_NAME;

                                i++;
                                read_until_eol(sb->sequences[sb->num_seq]->name, buf, &i);
                        }else if(c == '+'){
                                fh->trans[S_SEQ][S_Q_NEXT]++;
                                state = S_Q_NEXT;

                                skip_until_eol(buf, &i);
                                /* skip until eol  */
                        }else if(c != 0){
                                fh->n_parse_error++;
                                WARNING_MSG("ERROR %d: Don't understand - was in a seq and expecting more sequences, a '+' line or a name: %c",fh->n_parse_error,c);
                                /* LOG_MSG("%c %s", fh->start_symbol, buf+ i); */
                                print_error_context(buf, buf_len,i);
                        }
                        break;
                case S_Q_NEXT:
                        /* range of quality encodings for pacbio (coversillumina18 intervals) */
                        if(c >= 33 && c <= 126){
                                fh->trans[S_Q_NEXT][S_Q]++;
                                state = S_Q;

                                read_until_eol(sb->sequences[sb->num_seq]->qual, buf, &i);
                        }else{
                                fh->n_parse_error++;
                                WARNING_MSG("ERROR %d: Line following a '^+' line does not start with a valid base quality: %c",fh->n_parse_error,c);
                                print_error_context(buf, buf_len,i);
                        }
                        break;
                case S_Q:
                        if(fh->start_symbol){
                                if(first_seq){
                                        first_seq =0;
                                }else{
                                        sb->num_seq++;
                                        if(sb->num_seq == num){
                                                fh->pos = i;
                                                fh->read_state = state;
                                                return OK;
                                        }
                                }

                                fh->trans[S_Q][S_NAME]++;
                                state = S_NAME;
                                i++;
                                read_until_eol(sb->sequences[sb->num_seq]->name, buf, &i);
                        }else if(c != 0){
                                fh->n_parse_error++;
                                WARNING_MSG("ERROR %d: A base quality line is not followed by a sequence line.: %c",fh->n_parse_error,c);
                                print_error_context(buf, buf_len,i);
                        }
                        break;
                }
        }
        sb->num_seq++;

        fh->read_state = state;
        fh->pos = 0;            /* am at end of buffer */

        return OK;
ERROR:
        return FAIL;
}


int finalize_read(struct file_handler *fh, struct tl_seq_buffer *sb)
{
        struct tl_seq* s = NULL;
        double*  dist = NULL;
        int max_len = -1;
        int n_is_dna = 0;
        int n_is_aa = 0;
        if(fh->n_parse_error){
                /* sb->num_seq = 0; */
                return FAIL;
                /* *type = FILE_TYPE_UNDEFINED; */
        }else{
                if(fh->trans[S_START][S_NAME] == 0){
                        WARNING_MSG("Could not detect a single sequence name.");
                        if(sb->num_seq){
                                WARNING_MSG("Even stranger - the parser did record a sequence");
                        }
                        /* sb->num_seq = 0; */
                        return FAIL;
                }else if(fh->trans[S_START][S_NAME] && fh->trans[S_NAME][S_SEQ] == 0){
                        WARNING_MSG("Could detect a name but no sequence.");
                        if(sb->num_seq){
                                WARNING_MSG("Even stranger - the parser did record a sequence");
                        }
                        /* sb->num_seq = 0; */
                        return FAIL;
                }else if(fh->trans[S_SEQ][S_Q_NEXT] && fh->trans[S_Q_NEXT][S_Q] == 0){
                        WARNING_MSG("Could detect (at least) a sequence that should have base qualities but found none .");
                        /* sb->num_seq = 0; */
                        return FAIL;
                }else if(fh->trans[S_SEQ][S_Q_NEXT] ){
                        sb->is_fastq = 1;
                }else{
                        sb->is_fastq = 0;
                }
        }
        sb->base_quality_offset = 0;


        galloc(&dist, 256);
        for(int i = 0; i < 256;i++){
                dist[i] = 0.0;
        }
        for(int i = 0; i < sb->num_seq;i++){
                s = sb->sequences[i];
                /* more sanity checks */
                if(sb->is_fastq){
                        if(s->seq->len != s->qual->len){
                                WARNING_MSG("Length of sequence %s is not equal to length of base qualities.", TLD_STR(s->name));
                                return FAIL;
                                /* sb->num_seq = 0; */
                        }
                }
                uint8_t* seq = s->seq->str;
                /* LOG_MSG("%d %d",s->len, s->seq->len); */
                for(int j = 0; j < s->len;j++){
                        if(is_nuc[seq[j]]){
                                n_is_dna++;
                        }
                        if(is_aa[seq[j]]){
                                n_is_aa++;
                        }
                        if(seq[j] < 33 ){
                                LOG_MSG("Weird character found : %d at position %d in seq %d", seq[j], j , i);
                        }
                        dist[seq[j]]++;
                }

                if(s->len > max_len){
                        max_len = sb->sequences[i]->len;
                }
        }
        if(sb->num_seq){
                if(n_is_aa > n_is_dna){
                        sb->L = TL_SEQ_BUFFER_PROTEIN;
                }else{
                        double shannon = 0.0;
                        double p1 = 0.0;
                        double p2 = 0.0;
                        double sum = 0.0;

                        tld_shannon(dist,256,&shannon);
                        /* Magical numbers ahead to see whether the shannon entropy
                           of the letter distribution matches nucleotide or protein
                           sequences patterns.
                         */
                        /* mean and standard deviation of shannon entropy
                           of first 25 nucleotide of sequences in:
                           uniprot_sprot.fasta.gz
                        */
                        tld_normal_pdf(shannon, 3.454989,0.257477,&p1);

                        /* mean and standard deviation of shannon entropy
                           of first 25 nucleotide of sequences in:
                           vertebrate_mammalian.1.rna.fna.gz
                        */
                        tld_normal_pdf(shannon, 1.838894,0.138224,&p2);

                        sum = p1 + p2;
                        p1 = p1 / sum;
                        p2 = p2 / sum ;
                        /* LOG_MSG("%d %d  protein: %f nuc: %f",n_is_dna,n_is_aa, p1, p2); */
                        if(p1 > p2){
                                sb->L = TL_SEQ_BUFFER_PROTEIN;
                        }else{
                                sb->L = TL_SEQ_BUFFER_DNA;
                        }
                }
        }
        sb->max_len = max_len;
        gfree(dist);
        /* exit(0); */
        return OK;
ERROR:
        if(dist){
                gfree(dist);
        }
        return FAIL;
}

/* int get_base_q_offset(struct tl_seq* s, int* offset) */
/* { */
/*         /\* */
/*            SSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSS..................................................... */
/*            ..........................XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX...................... */
/*            ...............................IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII...................... */
/*            .................................JJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJ..................... */
/*            LLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLL.................................................... */
/*            PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP */
/*            !"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~ */
/*            |                         |    |        |                              |                     | */
/*           33                        59   64       73                            104                   126 */
/*            0........................26...31.......40                                 S - Sanger  Phred+33 */
/*                                     -5....0........9.............................40  X - Solexa    Solexa+64 */
/*                                           0........9.............................40  I - Illumina 1.3+ */
/*                                              3.....9..............................4  J - Illumina 1.5+ */
/*            0.2......................26...31........41                                L - Illumina 1.8+ */
/*            0..................20........30........40........50..........................................93  P - PacBio */
/*         *\/ */
/*         char q_type[] = "SXIJLP"; */
/*         uint32_t count[6] = {0,0,0,0,0,0}; */
/*         uint8_t c = 0; */
/*         for(int i = 0; i < s->qual->len;i++){ */
/*                 c = s->qual->str[i]; */
/*                 if(c >= 33 && c < 59){ */
/*                         count[0]++; */
/*                         count[4]++; */
/*                         count[5]++; */
/*                 }else if(c >=59 && c < 64){ */

/*                 } */
/*         } */

/*         return OK; */

/* } */

int close_seq_file(struct file_handler** fh)
{
        if(*fh){
                free_io_handler(*fh);
                *fh = NULL;
        }
        return OK;
}


int get_io_handler(struct file_handler** fh,const char* filename,int mode)
{
        struct file_handler* f_handle = NULL;
        char* ret = NULL;
        char file_mode[4];
        gzFile local_gz_f_ptr = NULL;


        //ASSERT(my_file_exists(filename), "File: %s not found.", filename);

        /* This is a bit ugly - refine in future. */
        if(!*fh){
                MMALLOC(f_handle, sizeof(struct file_handler));
        }
        f_handle->f_ptr = NULL;
        f_handle->gz_f_ptr = NULL;
        f_handle->file_type = 0;
        f_handle->read_buffer = NULL;
        f_handle->bytes_read = 0;
        f_handle->pos = 0;
        f_handle->read_state = S_START ; // RS_UNDEFINED;
        f_handle->write_state = 0;
        f_handle->gz= 0;
        f_handle->n_parse_error = 0;
        f_handle->start_symbol = 0;
        for(int i = 0 ; i < 5; i++){
                for(int j = 0 ; j < 5; j++){
                        f_handle->trans[i][j] = 0;
                }
        }

        MMALLOC(f_handle->read_buffer, sizeof(char)*  TL_READ_BUFF_LEN + TL_READ_BUFF_LEN);

        ret = strstr(filename, "fasta");
        if(ret){
                f_handle->file_type += TLSEQIO_FASTA;
        }
        ret = strstr(filename, "fastq");
        if(ret){
                f_handle->file_type+= TLSEQIO_FASTQ;
        }
        ret = strstr(filename, "gz");
        if(ret){
                if(strlen(ret) == 2){
                        /* LOG_MSG("Is gzipped"); */
                        f_handle->file_type |= TLSEQIO_GZIPPED;
                }
        }
        if(mode == TLSEQIO_WRITE){
                if(f_handle->file_type & TLSEQIO_GZIPPED){
                        WARNING_MSG("Opening an file for uncompressed write was requested but the file ends in .gz");
                        WARNING_MSG("Will write compressed. Consider using the TLSEQIO_WRITE_GZIPPED!");
                        file_mode[0] = 'w';
                        file_mode[1] = 'b';
                        file_mode[2] = TL_DEFAULT_COMPRESSION;
                        file_mode[3] = 0;

                        RUNP(local_gz_f_ptr = gzopen(filename, file_mode));

                        f_handle->gz = 1;
                }else{
                        //LOG_MSG("Opening file %s for writing",filename);
                        file_mode[0] = 'w';
                        file_mode[1] = 0;
                        RUNP(f_handle->f_ptr = fopen(filename, file_mode));
                        f_handle->gz = 0;
                }
        }
        if(mode == TLSEQIO_WRITE_GZIPPED){
                //LOG_MSG("Opening file %s for writing",filename);
                file_mode[0] = 'w';
                file_mode[1] = 'b';
                file_mode[2] = TL_DEFAULT_COMPRESSION;
                file_mode[3] = 0;

                RUNP(local_gz_f_ptr= gzopen(filename, file_mode));
                f_handle->gz = 1;
        }

        if(mode == TLSEQIO_APPEND){
                if(f_handle->file_type & TLSEQIO_GZIPPED){
                        WARNING_MSG("Opening an file for uncompressed append was requested but the file ends in .gz");
                        WARNING_MSG("Will write compressed. Consider using the TLSEQIO_WRITE_GZIPPED!");
                        file_mode[0] = 'a';
                        file_mode[1] = 'b';
                        file_mode[2] = TL_DEFAULT_COMPRESSION;
                        file_mode[3] = 0;

                        RUNP(local_gz_f_ptr = gzopen(filename, file_mode));

                        f_handle->gz = 1;
                }else{
                        //LOG_MSG("Opening file %s for writing",filename);
                        file_mode[0] = 'a';
                        file_mode[1] = 0;
                        RUNP(f_handle->f_ptr = fopen(filename, file_mode));
                        f_handle->gz = 0;
                }
        }
        if(mode == TLSEQIO_APPEND_GZIPPED){
                //LOG_MSG("Opening file %s for writing",filename);
                file_mode[0] = 'a';
                file_mode[1] = 'b';
                file_mode[2] = TL_DEFAULT_COMPRESSION;
                file_mode[3] = 0;

                RUNP(local_gz_f_ptr= gzopen(filename, file_mode));
                f_handle->gz = 1;
        }

        if(mode == TLSEQIO_READ){
                file_mode[0] = 'r';
                file_mode[1] = 0;

                //LOG_MSG("Opening %s for reading (%s)", filename,file_mode);
                RUNP(local_gz_f_ptr = gzopen(filename, file_mode));
                     //fprintf(stdout,"%p\n",(void*) local_gz_f_ptr);

        }

        f_handle->gz_f_ptr = local_gz_f_ptr;

        *fh = f_handle;
        return OK;
ERROR:
        return FAIL;
}

void free_io_handler(struct file_handler* f)
{
        if(f){
                if(f->gz_f_ptr){
                        gzclose(f->gz_f_ptr);
                }
                if(f->f_ptr){
                        fclose(f->f_ptr);
                }
                if(f->read_buffer){
                        MFREE(f->read_buffer);
                }
                MFREE(f);
        }
}


int echo_file(struct file_handler* f)
{
        char* buf = NULL;

        buf = f->read_buffer;
        while (1) {
                int bytes_read;
                bytes_read = gzread ( f->gz_f_ptr, buf,  TL_READ_BUFF_LEN - 1);
                buf[bytes_read] = '\0';
                //printf ("%s", buf);
                LOG_MSG("Read: %d", bytes_read);
                if (bytes_read < TL_READ_BUFF_LEN - 1) {
                        LOG_MSG("Read: %d", bytes_read);
                        if (gzeof (f->gz_f_ptr )) {
                                break;
                        }else{
                                ERROR_MSG("Something went wrong");
                        }
                }
        }
        return OK;
ERROR:
        return FAIL;
}




/* Memory functions  */



int read_file_contents(struct file_handler* fh)
{
        int c;
        ASSERT(fh!=NULL, "No file handler");
        fh->bytes_read = gzread(fh->gz_f_ptr, fh->read_buffer, TL_READ_BUFF_LEN -1);
        fh->read_buffer[fh->bytes_read] = 0;
        fh->pos = 0;
        //fprintf(stdout,"%s\n\n", fh->read_buffer);
        /* Read more bytes  until we have a newline */
        if(fh->read_buffer[fh->bytes_read-1] != '\n'){

                while(1){
                        c = gzgetc(fh->gz_f_ptr);
                        if(c == EOF){
                                break;
                        }
                        fh->read_buffer[fh->bytes_read] = c;
                        fh->bytes_read++;
                        if(c == '\n'){
                                break;
                        }
                }
        }
        fh->read_buffer[fh->bytes_read] = 0;

        /* fprintf(stdout,"%s", fh->read_buffer); */
        return OK;
ERROR:
        return FAIL;
}


int write_fasta_fastq(struct tl_seq_buffer* sb, struct file_handler* fh)
{
        char* write_buffer = NULL;
        int wb_pos;
        int i;
        int write_ok;
        /* function pointer to switch between fasta and fastq output */
        int (*write_p)(struct tl_seq* seq, char* buf, int* index,int* write_ok) = NULL;

        ASSERT(sb != NULL, "No sequence buffer");

        wb_pos = 0;
        MMALLOC(write_buffer, sizeof(char) * TL_READ_BUFF_LEN);

        if(sb->is_fastq){
                write_p = &write_fastq_to_buf;
        }else{
                write_p = &write_fasta_to_buf;
        }

        for(i = 0; i < sb->num_seq;i++){
                RUN(write_p(sb->sequences[i],write_buffer, &wb_pos,&write_ok));
                if(!write_ok){
                        if(fh->gz){
                                gzwrite(fh->gz_f_ptr, write_buffer, wb_pos);
                        }else{
                                fwrite(write_buffer, sizeof(char), wb_pos,fh->f_ptr);
                        }
                        wb_pos = 0;
                        RUN(write_p(sb->sequences[i],write_buffer, &wb_pos,&write_ok));
                        if(!write_ok){
                                ERROR_MSG("Couldn't fit sequence into write buffer(!)");
                        }
                }
        }
        if(wb_pos){
                if(fh->gz){
                        gzwrite(fh->gz_f_ptr, write_buffer, wb_pos);
                }else{
                        fwrite(write_buffer, sizeof(char), wb_pos,fh->f_ptr);
                }
        }
        MFREE(write_buffer);
        return OK;
ERROR:
        return FAIL;
}

int write_fasta_to_buf(struct tl_seq* seq, char* buf, int* index,int* write_ok)
{
        int name_len;
        int len;
        int i;
        int local_i;
        int check;

        local_i = *index;
        /* length of name; plus one for '>' plus one for newline  */
        name_len = TLD_STRLEN(seq->name);//  strnlen(seq->name, TL_SEQ_MAX_NAME_LEN);
        len = name_len +2;
        /* length of sequence + one for newline + len / 70 for internal line breaks  */
        len += seq->len + 1 + seq->len / TL_OUT_LINE_LEN;
        /* plus one character for the null terminator  */
        len += 1;
        *write_ok = 1;
        /* does this fit?  */
        if(local_i + len >= TL_READ_BUFF_LEN){
                *write_ok = 0;
                return OK;
        }

        check = local_i;
        buf[local_i] = '>';
        local_i++;
        for(i = 0;i < name_len;i++){
                buf[local_i] = TLD_STR(seq->name)[i]; //TLD_S  seq->name[i];
                local_i++;
        }
        buf[local_i] = '\n';
        local_i++;
        /* write sequence */
        for(i = 0;i < seq->len;i++){
                if((i!=0) && (i % TL_OUT_LINE_LEN) == 0){
                        buf[local_i] = '\n';
                        local_i++;
                }
                buf[local_i] = TLD_STR(seq->seq)[i];
                local_i++;
        }
        buf[local_i] = '\n';
        local_i++;

        buf[local_i] = 0;

        check = (local_i+1) - check;
        ASSERT(check == len, "Wrote %d but estimated %d", check,len);
        *index = local_i;
        return OK;
ERROR:
        return FAIL;
}


int write_fastq_to_buf(struct tl_seq* seq, char* buf, int* index,int* write_ok)
{
        int name_len;
        int len;
        int i;
        int local_i;
        int check;

        local_i = *index;
        /* length of name; plus one for '>' plus one for newline  */
        name_len =  TLD_STRLEN(seq->name);//  strnlen(seq->name, TL_SEQ_MAX_NAME_LEN);
        len = name_len +2;
        /* length of sequence + one for newline*/
        len += seq->len + 1 ;
        /* add two for '+' and newline */
        len += 2;
        /* same again for the base quality  */
        len += seq->len + 1 ;
        /* plus one character for the null terminator  */
        len += 1;
        *write_ok = 1;
        /* does this fit?  */
        if(local_i + len >= TL_READ_BUFF_LEN){
                *write_ok = 0;
                return OK;
        }

        check = local_i;
        buf[local_i] = '@';
        local_i++;
        for(i = 0;i < name_len;i++){
                buf[local_i] =  TLD_STR(seq->name)[i];//  seq->name[i];
                local_i++;
        }
        buf[local_i] = '\n';
        local_i++;
        /* write sequence */
        for(i = 0;i < seq->len;i++){
                buf[local_i] = TLD_STR(seq->seq)[i];
                local_i++;
        }
        buf[local_i] = '\n';
        local_i++;

        buf[local_i] = '+';
        local_i++;
        buf[local_i] = '\n';
        local_i++;

        for(i = 0;i < seq->len;i++){
                buf[local_i] = TLD_STR(seq->qual)[i];
                local_i++;
        }
        buf[local_i] = '\n';
        local_i++;

        buf[local_i] = 0;

        check = (local_i+1) - check;
        ASSERT(check == len, "Wrote %d but estimated %d", check,len);

        *index = local_i;
        return OK;
ERROR:
        return FAIL;
}

int read_until_eol(tld_strbuf *t, char *buf, int *pos)
{
        int i = *pos;
        while(1){
                if(buf[i] == '\n' || buf[i] == 0){
                        break;
                }
                /* This is annoying but required to skip \r and other non-printable characters.  */
                if(buf[i] >= 32){
                        tld_append_char(t, buf[i]);
                }
                i++;

        }
        *pos = i;
        return OK;
}

int skip_until_eol(char *buf, int *pos)
{
        int i = *pos;
        while(1){
                if(buf[i] == '\n' || buf[i] == 0){
                        break;
                }
                i++;
        }
        *pos = i;
        return OK;
}
