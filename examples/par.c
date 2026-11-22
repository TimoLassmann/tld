/* examples/par.c - parallel calls, embeddings, nearest neighbours.
 *   cc -std=c11 -I. examples/par.c -o par -lpthread -lm
 *   ./par 8          (number of threads, default 8) */
#define _POSIX_C_SOURCE 200809L /* clock_gettime; ai.h adds what it needs itself */
#define TLD_IMPLEMENTATION
#define AI_IMPLEMENTATION
#include "ai.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define TOPIC(X) X(genetics, "Genes and variants") X(imaging, "Scans and images")
AI_CHOICE(topic, "What is this abstract about?", TOPIC)

static const char *abstracts[16] = {"Variant calling in 60 families",
                                    "Imaging of the infant brain",
                                    "Variant effect prediction",
                                    "Imaging biomarkers in epilepsy",
                                    "Variant reclassification over time",
                                    "Imaging with MRI at 7T",
                                    "Variant interpretation with RNA-seq",
                                    "Imaging the heart in RHD",
                                    "Variant 1",
                                    "Imaging 1",
                                    "Variant 2",
                                    "Imaging 2",
                                    "Variant 3",
                                    "Imaging 3",
                                    "Variant 4",
                                    "Imaging 4"};
static char *summary[16];
static struct topic_answer label[16];

static void work(int i, void *ctx) {
    (void)ctx;
    summary[i] = ai_text("Summarise in one sentence: %s", abstracts[i]);
    if (!summary[i]) fprintf(stderr, "par: text %d: %s\n", i, ai_error());
    label[i] = ai_choose(topic, abstracts[i]);
    if (!label[i].ok) fprintf(stderr, "par: choose %d: %s\n", i, ai_error());
}

int main(int argc, char **argv) {
    int threads = argc > 1 ? atoi(argv[1]) : 8;
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    ai_parallel(16, threads, work, NULL);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    int ok = 0;
    for (int i = 0; i < 16; i++) ok += summary[i] && label[i].ok;
    printf("threads %d: %d/16 ok in %.2f s\n", threads, ok,
           (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9);

    ai_vecs m = ai_embed_all(abstracts, 16);
    if (!m.x) fprintf(stderr, "par: embed_all: %s\n", ai_error());
    ai_vec q = ai_embed("Variant calling pipelines");
    if (!q.x) fprintf(stderr, "par: embed: %s\n", ai_error());
    int idx[3];
    double sc[3];
    int k = ai_nearest(q, m, 3, idx, sc);
    printf("dim %d, nearest:", m.dim);
    for (int i = 0; i < k; i++) printf(" [%s %.2f]", abstracts[idx[i]], sc[i]);
    printf("\n");
    return ok == 16 && m.x && q.x ? 0 : 1;
}
