#include "tld.h"
#include "test.h"
#include <stdio.h>

static int checks(void)
{
        SECTION_MSG("messages");
        #ifdef DEBUG
        LOG_MSG("DEBUG");
        #endif
        LOG_MSG("All good!");
        WARNING_MSG("WARNING");
        if(1 == 0){
                ERROR_MSG("no longer");
        }

        int* f = NULL;
        double* d = NULL;

        SECTION_MSG("1D galloc / gfree");
        /* galloc(&f, 19); */
        galloc(&f, 1186718291);
        DEBUG_MSG("pointer: %p", (void *)f);
        for(int i = 0; i < 19;i++){
                f[i] = i;
        }
        galloc(&d, 1900);
        gfree(f);
        gfree(d);
        OK_MSG("allocated and freed 1186718291 ints and 1900 doubles");

        SECTION_MSG("2D galloc / gfree");
        float** mat = NULL;
        int dim1;
        int dim2;
        dim1 = 24;
        dim2 = 15;
        KV_MSG("dims", "%d x %d", dim1, dim2);

        galloc(&mat, dim1,dim2);

        for(int i = 0; i < dim1; i++){
                char row[256];
                int o = 0;
                for(int j = 0; j < dim2;j++){
                        mat[i][j] = MACRO_MAX(i,j);
                        o += snprintf(row + o, sizeof row - (size_t)o, "%0.1f ", mat[i][j]);
                }
                DEBUG_MSG("%s", row);
        }

        gfree(mat);
        OK_MSG("filled and freed the matrix");
        return OK;
ERROR:
        return FAIL;
}

/* unit-alloc ROWS [COLS]: galloc a matrix, fill it, print it. */
static int demo(int n, char **args)
{
        int rows = atoi(args[0]);
        int cols = n > 1 ? atoi(args[1]) : rows;
        int **m = NULL;
        if(rows < 1 || cols < 1){
                ERROR_MSG("ROWS and COLS must be positive");
        }
        SECTION_MSG("galloc(&m, %d, %d)", rows, cols);
        galloc(&m, rows, cols);
        DEBUG_MSG("matrix at %p, row 0 at %p", (void *)m, (void *)m[0]);
        for(int i = 0; i < rows; i++){
                for(int j = 0; j < cols; j++){
                        m[i][j] = i * cols + j;
                        fprintf(stdout, "%d%c", m[i][j], j + 1 == cols ? '\n' : ' ');
                }
        }
        gfree(m);
        OK_MSG("allocated, filled and freed %d x %d ints", rows, cols);
        return OK;
ERROR:
        return FAIL;
}

int main(int argc, char *argv[])
{
        test_init(&argc, argv, "[ROWS [COLS]]",
                  "With no arguments: logs a message and a warning, allocates and frees large\n"
                  "1D arrays and a 24 x 15 float matrix with galloc/gfree.\n"
                  "With ROWS [COLS]: allocates a ROWS x COLS int matrix (square if COLS is\n"
                  "omitted), fills it with row * COLS + col and prints it.\n"
                  "Example: unit-alloc 3 4");
        if(argc > 1){
                RUN(demo(argc - 1, argv + 1));
        } else {
                RUN(checks());
        }
        return EXIT_SUCCESS;
ERROR:
        return EXIT_FAILURE;
}
