#ifndef TLD_SEQ_H
#define TLD_SEQ_H

#include "../core/tld-core.h"
#include "../string/str.h"
#include "stdint.h"

#define ALIGN64 64
struct tl_seq{
        uint8_t * seq;
        tld_strbuf* name;
        /* char* name; */
        char* qual;
        void* data;
        int malloc_len;
        int len;
} __attribute__((aligned(ALIGN64)));

#undef ALIGN64

#define ALIGN64 64
struct tl_seq_buffer{
        struct tl_seq** sequences;
        void* data;
        int malloc_num;
        int offset;
        int base_quality_offset;
        int num_seq;
        int max_len;
        int is_fastq;
        int L;
} __attribute__((aligned(ALIGN64)));

#undef ALIGN64

/* Alphabet  */
#define TLALPHABET_DEFAULT_PROTEIN 1
#define TLALPHABET_DEFAULT_DNA 2
#define TLALPHABET_REDUCED_PROTEIN 3

#define TLALPHABET_NOAMBIGIOUS_PROTEIN 4
#define TLALPHABET_NOAMBIGUOUS_DNA 5

struct alphabet;
struct rng_state;

tld_external int create_alphabet(struct alphabet** alphabet, struct rng_state* rng,int type);

tld_external uint8_t tlalphabet_get_code (const struct alphabet* a, char c);
tld_external int convert_to_internal(struct alphabet* a, uint8_t* seq, int len);
//EXTERN int convert_to_external(struct alphabet* a, uint8_t* seq, int len);

tld_external void free_alphabet(struct alphabet* a);


/* File IO */
#define TLSEQIO_READ 0

#define TLSEQIO_WRITE 1
#define TLSEQIO_WRITE_GZIPPED 2

#define TLSEQIO_APPEND 3
#define TLSEQIO_APPEND_GZIPPED 4


typedef struct file_handler file_handler;

tld_external int open_fasta_fastq_file(struct file_handler** fh,char* filename, int mode);
tld_external int open_sam_bam(struct file_handler** fh, char* filename, int mode);
tld_external int read_fasta_fastq_file(struct file_handler* fh, struct tl_seq_buffer** seq_buf, int num);
tld_external int close_seq_file(struct file_handler** fh);

tld_external int write_seq_buf(struct tl_seq_buffer* sb, struct file_handler* fh);

/* Sequence buffer  */
#define TL_SEQ_MAX_NAME_LEN 128
#define TL_SEQ_BUFFER_PROTEIN 20
#define TL_SEQ_BUFFER_DNA 4


tld_external int detect_format(struct tl_seq_buffer* sb);

tld_external int alloc_tl_seq_buffer(struct tl_seq_buffer** seq_buf, int size);
tld_external int resize_tl_seq_buffer(struct tl_seq_buffer* sb);
tld_external int reset_tl_seq_buffer(struct tl_seq_buffer* sb);
tld_external void free_tl_seq_buffer(struct tl_seq_buffer* sb);

tld_external int alloc_tl_seq(struct tl_seq** sequence);
tld_external int resize_tl_seq(struct tl_seq* s);
tld_external void free_tl_seq(struct tl_seq* sequence);

#endif
