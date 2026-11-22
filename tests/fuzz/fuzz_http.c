/* Whole client path against a fake server that answers every request with the input. */
#define TLD_IMPLEMENTATION
#define AI_IMPLEMENTATION
#include "tld.h"
#include "tld_ai.h"
#include <netinet/in.h>
#include <arpa/inet.h>
static int lfd = -1;
static const uint8_t *rep; static size_t repn;
static atomic_int done;
static void answer(int c) {
        char buf[65536]; size_t got = 0;
        struct timeval tv = {0, 200000}; setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
        for (;;) {                                  /* read the request: headers + Content-Length body */
                ssize_t k = recv(c, buf + got, sizeof buf - 1 - got, 0);
                if (k <= 0) break;
                got += (size_t)k; buf[got] = 0;
                char *e = strstr(buf, "\r\n\r\n");
                if (e) { char *cl = strcasestr(buf, "content-length:"); long L = cl ? atol(cl + 15) : 0;
                         if ((size_t)(e + 4 - buf) + (size_t)L <= got) break; }
        }
        size_t off = 0;
        while (off < repn) { ssize_t k = send(c, rep + off, repn - off, MSG_NOSIGNAL); if (k <= 0) break; off += (size_t)k; }
        close(c);
}
static void *serve(void *a) {
        (void)a;
        while (!atomic_load(&done)) {               /* every connection gets the same reply */
                struct pollfd pf = {lfd, POLLIN, 0};
                if (poll(&pf, 1, 20) == 1) { int c = accept(lfd, NULL, NULL); if (c >= 0) answer(c); }
        }
        return NULL;
}
int LLVMFuzzerInitialize(int *argc, char ***argv) {
        (void)argc; (void)argv;
        struct sockaddr_in a = {0}; socklen_t al = sizeof a;
        a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        lfd = socket(AF_INET, SOCK_STREAM, 0);
        bind(lfd, (struct sockaddr *)&a, sizeof a); listen(lfd, 16);
        getsockname(lfd, (struct sockaddr *)&a, &al);
        char host[64]; snprintf(host, sizeof host, "http://127.0.0.1:%d", ntohs(a.sin_port));
        setenv("TLD_AI_HOST", host, 1); setenv("TLD_AI_TIMEOUT", "2", 1); setenv("TLD_AI_FRESH", "1", 1);
        setenv("TLD_VERBOSE", "0", 1);
        return 0;
}
#define F(X) X(str, name, "") X(int, count, "")
AI_STRUCT(fz, F)
int LLVMFuzzerTestOneInput(const uint8_t *d, size_t n) {
        if (n < 1 || n > 1 << 16) return 0;
        uint8_t mode = d[0] % 4; rep = d + 1; repn = n - 1;
        atomic_store(&done, 0);
        ai__nprobes = 0;                            /* forget 'server down' from the last input */
        pthread_mutex_lock(&ai__dig_mu); tld_hash_free(ai__dig); ai__dig = NULL; pthread_mutex_unlock(&ai__dig_mu);
        pthread_t t; pthread_create(&t, NULL, serve, NULL);
        if (mode == 0) ai_text("hi");
        else if (mode == 1) { ai_vec v = ai_embed("hi"); (void)v; }
        else if (mode == 2) { struct fz x; ai_fill(&x, fz, "fill"); }
        else ai_yesno("q?", "s");
        atomic_store(&done, 1);
        pthread_join(t, NULL);
        ai_reset();
        return 0;
}
