/* A program that, after the implementation of tld.h, defines names a program is likely
 * to use, many of which tld used to define itself. It must compile and exit 0. */
#define TLD_IMPLEMENTATION
#include "tld.h"

#define AT 1
#define SCALE 2
#define EXTERN extern
#define NARG3 3
#define MAX_VAR_NAME 4
#define WORK_TODO 5
#define ADDFAILED 6
#define MESSAGE_TYPE_ERROR 7
#define M_2PI 8
#define JS 9
#define JO 10
#define JA 11
#define MACRO_MIN 12
#define LOGSUM_SIZE 13

typedef struct { int x; } Parser;
typedef struct { int y; } template_node;
typedef struct { int z; } json_parser;
typedef struct { int k; } kdtree;
typedef struct { int r; } rng_state;
struct work_queue { int w; };
enum { NODE_ROOT, NODE_TEXT, NODE_IF };
enum { VAL_STRING, VAL_OBJ, VAL_ARR, VAL_NUM, VAL_INT, VAL_BOOL, VAL_NULL };

static int next(int x) { return x + 1; }
static int search(int x) { return x; }
static int jump(int x) { return x; }
static int vlog(int x) { return x; }
static int rotl(int x) { return x; }
static int mix64(int x) { return x; }
static int add_val(int x) { return x; }
static int detect_type(int x) { return x; }
static int logsum(int x) { return x; }
static int init_rng(int x) { return x; }
static int get_time(int x) { return x; }
static int process_tree(int x) { return x; }

int main(void)
{
        Parser p = {NODE_TEXT + VAL_STRING};
        template_node t = {NODE_IF};
        json_parser j = {0};
        kdtree k = {0};
        rng_state r = {0};
        struct work_queue q = {0};
        int v = next(search(jump(vlog(rotl(mix64(add_val(detect_type(p.x))))))));
        v += logsum(init_rng(get_time(process_tree(t.y + j.z + k.k + r.r + q.w))));
        v += AT + SCALE + NARG3 + MAX_VAR_NAME + WORK_TODO + ADDFAILED + MESSAGE_TYPE_ERROR + M_2PI +
             JS + JO + JA + MACRO_MIN + LOGSUM_SIZE;
        /* p.x = 1: next(1) = 2; t.y = 2; the constants sum to 91 */
        return v == 2 + 2 + 91 ? 0 : 1;
}
