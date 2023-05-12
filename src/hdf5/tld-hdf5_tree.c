#include "../core/tld-core.h"
#include "../alloc/tld-alloc.h"
#include "../misc/misc.h"
#include <hdf5.h>

#include "tld-hdf5.h"
#include "tld-hdf5_struct.h"

#define TLD_HDF5_TREE_IMPORT
#include "tld-hdf5_tree.h"

herr_t worker_hdf_node  (hid_t loc_id, const char *name, const H5L_info_t *info, void *operator_data);

int tree_recurse_fill(struct hdf5_data *d, struct hdf5_node *root);
static int hdf5_print_tree(struct hdf5_node* n , int l);

static int hdf5_node_alloc(struct hdf5_node **out, int size);
static int hdf5_node_resize(struct hdf5_node *n);

int hdf5_build_tree(struct hdf5_data *d)
{
        struct hdf5_node* root = NULL;

        RUN(hdf5_node_alloc(&root, 16));
        MMALLOC(root->path_name, 2);
        strncpy(root->path_name,"/",2);
        root->path_name[1] = 0;

        MMALLOC(root->name, 2);
        strncpy(root->name,"/",2);
        root->name[1] = 0;
        /* snprintf(root->name, 5, "/"); */
        root->type = H5O_TYPE_GROUP;

        tree_recurse_fill(d, root);
        RUN(hdf5_print_tree(root , 0));

        d->root = root;
/* hdf5_node_free(root); */
        return OK;
ERROR:
        if(root){
                hdf5_node_free(root);
        }
        return FAIL;
}

int tree_recurse_fill(struct hdf5_data *d, struct hdf5_node *root)
{

        if(root->type == H5O_TYPE_GROUP){
                LOG_MSG("Arrived at %s", root->path_name);
                RUN(tld_hdf5_open_group(d, root->path_name));
                int status = H5Literate (d->group, H5_INDEX_NAME, H5_ITER_NATIVE, NULL, worker_hdf_node, root);
                if(status){
                        ERROR_MSG("H5Literate failed");
                }
                RUN(tld_hdf5_close_group(d));
        }
        for(int i =0 ; i < root->n;i++){
                if(root->l[i]->type == H5O_TYPE_GROUP){
                        RUN(tree_recurse_fill(d,root->l[i]));
                }

        }

        return OK;
ERROR:
        return FAIL;
}

int hdf5_print_tree(struct hdf5_node* n , int l)
{

        for(int i = 0; i < l;i++){
                fprintf(stdout," ");
        }
        switch (n->type) {
        case H5O_TYPE_GROUP:
                fprintf(stdout,"%s\tGroup\n",n->name);
                break;
        case H5O_TYPE_DATASET:
                fprintf(stdout,"%s\tDataset\n",n->name);

                break;
        case H5O_TYPE_NAMED_DATATYPE:
                fprintf(stdout,"%s\tDatatype\n",n->name);
                break;
        default:
                fprintf(stdout,"%s\tUnknown\n",n->name);
        }
        for(int i = 0; i < n->n;i++){
                hdf5_print_tree( n->l[i], l + 3);
        }
        return OK;
}

#define UNUSED(expr) do { (void)(expr); } while (0)
herr_t worker_hdf_node  (hid_t loc_id, const char *name, const H5L_info_t *info, void *operator_data)
{
        herr_t          status;
        H5O_info_t      infobuf;

        struct hdf5_node* root = (struct hdf5_node*) operator_data;
        struct hdf5_node* n;
        char buffer[BUFSIZ];

        UNUSED(info);
        if(root->n == root->n_alloc){
                hdf5_node_resize(root);
        }

        hdf5_node_alloc(&root->l[root->n], 32);
        n = root->l[root->n];
        root->n++;

        status = H5Oget_info_by_name (loc_id, name, &infobuf, H5P_DEFAULT);
        if(status){
                WARNING_MSG("H5Oget_info_by_name failed in hdf5 tree ");
        }
        n->type = infobuf.type;

        snprintf(buffer, BUFSIZ, "%s", name);

        int len = strnlen(buffer, BUFSIZ);


        /* LOG_MSG("len %5d name:%s", strlen(name), name); */

        MMALLOC(n->name, len+1 );

        strncpy(n->name, name, len);
        n->name[len] = 0;

        /* snprintf(n->name, len, "%s"m ) */
        if(strncmp(root->path_name,"/",512) == 0){
                snprintf(buffer, BUFSIZ, "/%s", name);
                /* snprintf(n->name, 512, "/%s", name); */
        }else{
                snprintf(buffer, BUFSIZ, "%s/%s", root->name,  name);

        }
        len = strnlen(buffer, BUFSIZ);
        MMALLOC(n->path_name, len+1 );
        strncpy(n->path_name, buffer, len);
        n->path_name[len] = 0;
        root->l[root->n] = n;
        return 0;
ERROR:
        WARNING_MSG("Malloc in hdf5 tree node building failed!!!");
        return 0;
}
#undef UNUSED


int hdf5_node_alloc(struct hdf5_node **out, int size)
{
        struct hdf5_node* n = NULL;
        MMALLOC(n, sizeof(struct hdf5_node));

        n->l = NULL;

        n->name = NULL;
        n->path_name = NULL;
        n->n_alloc = size;
        n->n = 0;
        n->visited = 0;
        MMALLOC(n->l, sizeof(struct hdf5_node*) * n->n_alloc);
        /* MMALLOC(n->visited, sizeof(uint8_t) * n->n_alloc); */

        for(int i = 0; i <n->n_alloc;i++){
                n->l[i] = NULL;
                /* n->visited[i] = 0; */
        }
        *out = n;
        return OK;
ERROR:
        hdf5_node_free(n);
        return FAIL;
}

int hdf5_node_resize (struct hdf5_node *n)
{

        int old_size = n->n;
        n->n_alloc = n->n_alloc + n->n_alloc / 2;


        MREALLOC(n->l, sizeof(struct hdf5_node*) * n->n_alloc);
        /* MREALLOC(n->visited, sizeof(uint8_t) * n->n_alloc); */

        for(int i = old_size; i <n->n_alloc;i++){
                n->l[i] = NULL;
                /* n->visited[i] = 0; */
        }
        return OK;
ERROR:
        hdf5_node_free(n);
        return FAIL;
}


void hdf5_node_free(struct hdf5_node *n)
{
        if(n){

                if(n->n){
                        for(int i = 0;i < n->n;i++){
                                hdf5_node_free(n->l[i]);
                        }
                }
                if(n->l){
                        MFREE(n->l);
                }
                /* if(n->visited){ */
                        /* MFREE(n->visited); */
                /* } */
                if(n->path_name){
                        MFREE(n->path_name);
                }
                if(n->name){
                        MFREE(n->name);
                }
                MFREE(n);
        }
}
