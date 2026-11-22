/* examples/par.c - parallel calls, embeddings, nearest neighbours.
 *   cc -std=c11 -I. examples/par.c -o par -lpthread -lm
 *   ./par 8          (number of threads, default 8)
 *   ./par -vv 8      (also each request: endpoint, model, host)
 * The totals go to stdout; each job and the search go to stderr. */
#define _POSIX_C_SOURCE 200809L /* clock_gettime, setenv; tld_ai.h adds what it needs itself */
#define TLD_IMPLEMENTATION
#define AI_IMPLEMENTATION
#include "tld_ai.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define NAME(label, description) #label,
#define TOPIC(X) X(genetics, "Genes and variants") X(imaging, "Scans and images")
AI_CHOICE(topic, "What is this abstract about?", TOPIC)
static const char *topic_name[] = {TOPIC(NAME)};

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
static const char *summary[16];
static struct topic_answer label[16];
static double seconds[16];    /* time this job spent waiting for the server */
static int cached[16];        /* how many of its two calls came from the cache */

static void work(int i, void *ctx) {
    (void)ctx;
    summary[i] = ai_text("Summarise in one sentence: %s", abstracts[i]);
    if (!summary[i]) fprintf(stderr, "par: text %d: %s\n", i, ai_error());
    seconds[i] = ai_last()->seconds;   /* ai_last() is this thread's last call */
    cached[i] = ai_last()->from_cache;
    label[i] = ai_choose(topic, abstracts[i]);
    if (!label[i].ok) fprintf(stderr, "par: choose %d: %s\n", i, ai_error());
    seconds[i] += ai_last()->seconds;
    cached[i] += ai_last()->from_cache;
}

static void usage(FILE *f, const char *prog) {
    fprintf(f, "Usage: %s [-h] [-v] [THREADS]\n\n"
               "Summarises 16 abstract titles and picks a topic for each on THREADS threads\n"
               "(default 8), then embeds them and finds the 3 nearest to a query.\n"
               "Example: %s 4\n\n"
               "Options:\n"
               "  -h, --help      show this help and exit\n"
               "  -v, --verbose   -vv also shows each request (TLD_VERBOSE=2)\n\n"
               "TLD_VERBOSE=0 leaves only the totals and errors.\n",
            prog, prog);
}

int main(int argc, char **argv) {
    static struct option opts[] = {{"help", no_argument, 0, 'h'},
                                   {"verbose", no_argument, 0, 'v'},
                                   {0, 0, 0, 0}};
    int c, verbose = 0;
    while ((c = getopt_long(argc, argv, "hv", opts, NULL)) != -1) {
        switch (c) {
        case 'h': usage(stdout, argv[0]); return 0;
        case 'v': verbose++; break;
        default: usage(stderr, argv[0]); return 2;
        }
    }
    int threads = 8;
    if (optind < argc) {
        char *end;
        long t = strtol(argv[optind], &end, 10);
        if (*end || t < 1 || t > 256 || optind + 1 < argc) {
            fprintf(stderr, "par: THREADS must be a number from 1 to 256\n\n");
            usage(stderr, argv[0]);
            return 2;
        }
        threads = (int)t;
    }
    if (verbose) setenv("TLD_VERBOSE", verbose >= 2 ? "2" : "1", 1);

    SECTION_MSG("ai_parallel: 16 jobs (ai_text + ai_choose) on %d threads", threads);
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    ai_parallel(16, threads, work, NULL);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double wall = (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;
    int ok = 0;
    double busy = 0;
    for (int i = 0; i < 16; i++) {
        ok += summary[i] && label[i].ok;
        busy += seconds[i];
        if (summary[i] && label[i].ok)
            KV_MSG("job", "%2d %-8s %.2f  %s%s: %s", i, topic_name[label[i].value],
                   label[i].confidence, abstracts[i], cached[i] == 2 ? " (cache)" : "", summary[i]);
    }
    if (ok == 16)
        OK_MSG("16/16 ok in %.2f s wall, %.2f s of server time (%.1fx)", wall, busy,
               wall > 0 ? busy / wall : 0);
    printf("threads %d: %d/16 ok in %.2f s\n", threads, ok, wall);

    SECTION_MSG("ai_embed_all + ai_nearest");
    ai_vecs m = ai_embed_all(abstracts, 16);
    if (!m.x) fprintf(stderr, "par: embed_all: %s\n", ai_error());
    const char *query = "Variant calling pipelines";
    ai_vec q = ai_embed(query);
    if (!q.x) fprintf(stderr, "par: embed: %s\n", ai_error());
    int idx[3];
    double sc[3];
    int k = ai_nearest(q, m, 3, idx, sc);
    if (m.x && q.x) {
        const ai_info *in = ai_last();
        if (in->model) KV_MSG("model", "%s", in->model);
        KV_MSG("vectors", "16 of dimension %d", m.dim);
        KV_MSG("query", "%s", query);
        for (int i = 0; i < k; i++) KV_MSG("nearest", "%.3f  %s", sc[i], abstracts[idx[i]]);
    }
    printf("dim %d, nearest:", m.dim);
    for (int i = 0; i < k; i++) printf(" [%s %.2f]", abstracts[idx[i]], sc[i]);
    printf("\n");
    return ok == 16 && m.x && q.x ? 0 : 1;
}
