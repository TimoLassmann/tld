#include "template.h"

#include "../core/tld-core.h"
#include "../alloc/tld-alloc.h"
#include "../string/str.h"

#include <stdio.h>
#include <ctype.h>

#define TEMPLATE_OK 0
#define TEMPLATE_MISMATCH_DELIM 1
#define TEMPLATE_NOREP 2

#define MAX_VAR_NAME 64
#define MAX_CHILDREN 10
#define MAX_ERROR_MSG 256

typedef struct tld_template_hash_entry {
    tld_strbuf* key;
    tld_strbuf* value;
    struct tld_template_hash_entry* next; // For handling collisions using chaining
} tld_template_hash_entry;

typedef struct tld_template_hash {
    tld_template_hash_entry** table; // Array of pointers to entries
    int table_size;             // Size of the hash table
    int n;                      // Number of entries
    int n_rep;
    int status;
} tld_template_hash;

static unsigned int str_hash(char *str, int table_size);

static unsigned int str_hash(char *str, int table_size)
{
        unsigned int hash = 5381;
        int c;
        while ((c = *str++)){
                hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
        }
        return hash % table_size;
}

int tld_template_add(tld_template_hash **hash, char *key, char *val)
{
        tld_template_hash* h = NULL;

        if(*hash){
                h = *hash;
        }else{
                RUN(tld_template_hash_alloc(&h, 1024));
        }

        ASSERT(h != NULL, "No Map");

        unsigned int index = str_hash(key, h->table_size);

        // Create a new entry
        tld_template_hash_entry* new_entry = NULL;//(tld_template_hash_entry*)malloc(sizeof(tld_template_hash_entry));
        MMALLOC(new_entry, sizeof(tld_template_hash_entry));
        tld_strbuf_alloc(&new_entry->key, 16);
        tld_strbuf_alloc(&new_entry->value, 16);
        tld_append(new_entry->key, key);
        tld_append(new_entry->value, val);
        new_entry->next = h->table[index];
        h->table[index] = new_entry;
        h->n++;

        *hash = h;
        return OK;
ERROR:
        return FAIL;
}

int tld_template_get(tld_template_hash *h, char *key, char **val)
{
        ASSERT(h != NULL, "No Map!!");
        ASSERT(key != NULL, "No key");

        unsigned int index = str_hash(key, h->table_size);
        tld_template_hash_entry* entry = h->table[index];

        while (entry) {
                if (strcmp(TLD_STR(entry->key), key) == 0) {
                        *val = TLD_STR(entry->value);
                        return OK;
                }
                entry = entry->next;
        }

        *val = NULL; // Key not found
        return FAIL;
ERROR:
        return FAIL;
}

int tld_template_rename_var(tld_template_hash *h, char *key, char* new_key) {
        ASSERT(h != NULL, "No Map!!");
        ASSERT(key != NULL, "No key");

        unsigned int index = str_hash(key, h->table_size);
        tld_template_hash_entry* entry = h->table[index];

        while (entry) {
                if (strcmp(TLD_STR(entry->key), key) == 0) {
                        // Remove the old key from the hash table
                        tld_template_remove(h, key);
                        // Add the new key with the existing replacement
                        tld_template_add(&h, new_key, TLD_STR(entry->value));
                        return OK;
                }
                entry = entry->next;
        }

        return FAIL; // Key not found
ERROR:
        return FAIL;
}

int tld_template_remove(tld_template_hash *h, char *key)
{
        ASSERT(h != NULL, "No Map!!");
        ASSERT(key != NULL, "No key");

        unsigned int index = str_hash(key, h->table_size);
        tld_template_hash_entry* entry = h->table[index];
        tld_template_hash_entry* prev = NULL;

        while(entry){
                if(strcmp(TLD_STR(entry->key), key) == 0){
                        if(prev){
                                prev->next = entry->next;
                        }else{
                                h->table[index] = entry->next;
                        }
                        // Free the entry
                        tld_strbuf_free(entry->key);
                        tld_strbuf_free(entry->value);
                        MFREE(entry);
                        h->n--;
                        return OK;
                }
                prev = entry;
                entry = entry->next;
        }

        return FAIL; // Key not found
ERROR:
        return FAIL;
}

int tld_template_hash_alloc(tld_template_hash **hash, int size)
{
        tld_template_hash* h = NULL;
        MMALLOC(h, sizeof(tld_template_hash));
        h->table_size = size; // Choose an appropriate size
        // Initialize the map with a default size
        MMALLOC(h->table, sizeof(tld_template_hash_entry*) * h->table_size);
        h->n = 0;
        *hash = h;
        return OK;
ERROR:
        if(h){
                tld_template_hash_free(h);
        }
        return FAIL;
}

void tld_template_hash_free(tld_template_hash *h)
{
        if(h){
                for(int i = 0; i < h->table_size; i++){
                        tld_template_hash_entry* entry = h->table[i];
                        while(entry){
                                tld_template_hash_entry* next = entry->next;
                                tld_strbuf_free(entry->key);
                                tld_strbuf_free(entry->value);
                                MFREE(entry);
                                entry = next;
                        }
                }
                MFREE(h->table);
                MFREE(h);
        }
}

typedef enum {
        NODE_ROOT,
        NODE_TEXT,
        NODE_VARIABLE,
        NODE_IF,
        NODE_ELSE,
        NODE_ENDIF
} template_node_type;

typedef struct template_node {
        template_node_type type;
        tld_strbuf *content;
        /* char *content; */
        struct template_node** children;
        /* struct Node *children[MAX_CHILDREN]; */
        int n;
        int n_alloc;
        /* int child_count; */
        /* struct Node *parent; */
        struct template_node* parent;
} template_node;

typedef struct {
        const char *input;
        int position;
        char error[MAX_ERROR_MSG];
} Parser;

/* tld_internal int find(tld_strbuf *txt, int offset, tld_strbuf *delim,int*start_pos,int* end_pos); */
/* tld_internal int find_open(tld_strbuf *txt,int offset, tld_strbuf *delim, int*pos); */
/* tld_internal int find_end(tld_strbuf *txt,int offset, tld_strbuf* delim, int*pos); */

/* tld_internal int tld_template_allocate(tld_template_map **map, int n); */
/* tld_internal int tld_template_resize(tld_template_map *m); */

tld_internal  int parse_text(Parser *parser, template_node **node);
tld_internal  int parse_tag(Parser *parser, template_node **node);

tld_internal  int is_end_of_input(Parser *parser);
tld_internal char peek(Parser *parser);
tld_internal char parser_next(Parser *parser);
tld_internal void skip_whitespace(Parser *parser);

tld_internal  int template_node_add_child(template_node *parent, template_node *child);
tld_internal  int template_node_create(template_node **node, template_node_type type, char *content);
tld_internal  int template_node_resize(template_node *node);
tld_internal void template_node_free(template_node *node);

int parse_template(Parser *parser, template_node **node)
{
        template_node *root = NULL;
        template_node *current = NULL;
        template_node** stack = NULL;
        int stack_top = 0;
        int stack_size = 10;
        MMALLOC(stack, sizeof(template_node*) * stack_size);
        /* MMALLOC(n->children, sizeof(template_node*) * n->n_alloc); */
        /* int stack_top = 0; */

        RUN(template_node_create(&root, NODE_ROOT, NULL));
        current = root;

        while(!is_end_of_input(parser)){
                if(strncmp(parser->input + parser->position, "{{", 2) == 0){
                        template_node *tag_node = NULL;
                        RUN(parse_tag(parser, &tag_node));
                        if(tag_node){
                                switch(tag_node->type){
                                case NODE_IF:
                                        RUN(template_node_add_child(current, tag_node));
                                        if(stack_top >= stack_size){
                                                stack_size = stack_size + stack_size / 2;
                                                MREALLOC(stack, sizeof(template_node*) * stack_size);
                                        }
                                        stack[stack_top] = current;
                                        stack_top++;
                                        current = tag_node;
                                        break;
                                case NODE_ELSE:
                                        if(stack_top > 0 && current->type == NODE_IF){
                                                current = stack[stack_top - 1];
                                                RUN(template_node_add_child(current, tag_node));
                                                current = tag_node;
                                        }else{
                                                snprintf(parser->error, MAX_ERROR_MSG, "ELSE without matching IF");
                                                template_node_free(tag_node);
                                        }
                                        break;
                                case NODE_ENDIF:
                                        if(stack_top > 0){
                                                current = stack[--stack_top];
                                                template_node_free(tag_node);
                                        }else{
                                                snprintf(parser->error, MAX_ERROR_MSG, "ENDIF without matching IF");
                                                template_node_free(tag_node);
                                        }
                                        break;
                                default:
                                        RUN(template_node_add_child(current, tag_node));
                                }
                        }else if(parser->error[0] == '\0'){
                                snprintf(parser->error, MAX_ERROR_MSG, "Failed to parse tag");
                        }
                }else{
                        template_node *text_node = NULL;
                        RUN(parse_text(parser, &text_node));
                        if(text_node){
                                RUN(template_node_add_child(current, text_node));
                        }else if(parser->error[0] == '\0'){
                                snprintf(parser->error, MAX_ERROR_MSG, "Failed to parse text");
                        }
                }

                if(parser->error[0] != '\0'){
                        break;
                }
        }

        if(stack_top > 0){
                snprintf(parser->error, MAX_ERROR_MSG, "Unclosed IF statement");
        }
        MFREE(stack);
        *node = root;
        return OK;
ERROR:
        if(stack){
                MFREE(stack);
        }
        return FAIL;
}


int template_node_add_child(template_node *parent, template_node *child)
{
        if(parent->n < parent->n_alloc){
                parent->children[parent->n] = child;
                parent->n++;
                if(parent->n == parent->n_alloc){
                        RUN(template_node_resize(parent));
                }
                child->parent = parent;
        }else{
                template_node_free(child);
        }
        return OK;
ERROR:
        return FAIL;
}


int parse_text(Parser *parser, template_node **node)
{
        char *text = NULL;
        const char *start = parser->input + parser->position;
        while (!is_end_of_input(parser) && (peek(parser) != '{' || parser->input[parser->position + 1] != '{')) {
                parser_next(parser);
        }
        int length = parser->input + parser->position - start;
        if (length > 0) {
                MMALLOC(text, length + 1);
                strncpy(text, start, length);
                text[length] = '\0';
                template_node_create(node, NODE_TEXT, text);
                MFREE(text);
        }
        return OK;
ERROR:
        return FAIL;
}

int parse_tag(Parser *parser, template_node **node)
{
        char tag_content[MAX_VAR_NAME];
        int i = 0;
        if (parser_next(parser) != '{' || parser_next(parser) != '{') {
                snprintf(parser->error, MAX_ERROR_MSG, "Invalid tag opening");
                return FAIL;
        }
        while (!is_end_of_input(parser) && (peek(parser) != '}' || parser->input[parser->position + 1] != '}')) {
                if (i < MAX_VAR_NAME - 1) {
                        tag_content[i++] = parser_next(parser);
                } else {
                        snprintf(parser->error, MAX_ERROR_MSG, "Tag content too long");
                        return FAIL;
                }
        }
        tag_content[i] = '\0';
        if(parser_next(parser) != '}' || parser_next(parser) != '}'){
                snprintf(parser->error, MAX_ERROR_MSG, "Invalid tag closing");
                return FAIL;
        }

        if(strcmp(tag_content, "ELSE") == 0){
                return template_node_create(node, NODE_ELSE, NULL);
        }else if(strcmp(tag_content, "ENDIF") == 0){
                return template_node_create(node, NODE_ENDIF, NULL);
        }else if(strncmp(tag_content, "IF ", 3) == 0){
                return template_node_create(node, NODE_IF, tag_content + 3);
        }else{
                return template_node_create(node, NODE_VARIABLE, tag_content);
        }
}


/* node *parse_template(Parser *parser) { */
/*         Node *root = create_node(NODE_ROOT, NULL); */
/*         Node *current = root; */
/*         Node *stack[MAX_CHILDREN]; */
/*         int stack_top = 0; */

/*         while (!is_end_of_input(parser)) { */
/*                 if (strncmp(parser->input + parser->position, "{{", 2) == 0) { */
/*                         Node *tag_node = parse_tag(parser); */
/*                         if(tag_node){ */
/*                                 switch(tag_node->type){ */
/*                                 case NODE_IF: */
/*                                         add_child(current, tag_node); */
/*                                         if (stack_top >= MAX_CHILDREN) { */
/*                                                 snprintf(parser->error, MAX_ERROR_MSG, "Nesting too deep"); */
/*                                                 return root; */
/*                                         } */
/*                                         stack[stack_top++] = current; */
/*                                         current = tag_node; */
/*                                         break; */
/*                                 case NODE_ELSE: */
/*                                         if(stack_top > 0 && current->type == NODE_IF){ */
/*                                                 current = stack[stack_top - 1]; */
/*                                                 add_child(current, tag_node); */
/*                                                 current = tag_node; */
/*                                         }else{ */
/*                                                 snprintf(parser->error, MAX_ERROR_MSG, "ELSE without matching IF"); */
/*                                                 free_node(tag_node); */
/*                                         } */
/*                                         break; */
/*                                 case NODE_ENDIF: */
/*                                         if(stack_top > 0){ */
/*                                                 current = stack[--stack_top]; */
/*                                                 free_node(tag_node); */
/*                                         }else{ */
/*                                                 snprintf(parser->error, MAX_ERROR_MSG, "ENDIF without matching IF"); */
/*                                                 free_node(tag_node); */
/*                                         } */
/*                                         break; */
/*                                 default: */
/*                                         add_child(current, tag_node); */
/*                                 } */
/*                         }else if(parser->error[0] == '\0'){ */
/*                                 snprintf(parser->error, MAX_ERROR_MSG, "Failed to parse tag"); */
/*                         } */
/*                 }else{ */
/*                         Node *text_node = parse_text(parser); */
/*                         if(text_node){ */
/*                                 add_child(current, text_node); */
/*                         }else if(parser->error[0] == '\0'){ */
/*                                 snprintf(parser->error, MAX_ERROR_MSG, "Failed to parse text"); */
/*                         } */
/*                 } */

/*                 if(parser->error[0] != '\0'){ */
/*                         break; */
/*                 } */
/*         } */

/*         if(stack_top > 0){ */
/*                 snprintf(parser->error, MAX_ERROR_MSG, "Unclosed IF statement"); */
/*         } */

/*         return root; */
/* } */

void print_tree(template_node *node, int depth) {
        for (int i = 0; i < depth; i++) {
                printf("  ");
        }

        switch (node->type) {
        case NODE_ROOT:
                printf("ROOT\n");
                break;
        case NODE_TEXT:
                printf("TEXT: %s\n", TLD_STR(node->content));
                break;
        case NODE_VARIABLE:
                printf("VARIABLE: %s\n", TLD_STR(node->content));
                break;
        case NODE_IF:
                printf("IF: %s\n", TLD_STR(node->content));
                break;
        case NODE_ELSE:
                printf("ELSE\n");
                break;
        case NODE_ENDIF:
                printf("ENDIF\n");
                break;
        }

        for (int i = 0; i < node->n; i++) {
                print_tree(node->children[i], depth + 1);
        }
}

void process_tree(template_node *node, tld_template_hash *hash, FILE *output,tld_strbuf* out)
{
        switch(node->type){
        case NODE_ROOT:
                for(int i = 0; i < node->n; i++){
                        process_tree(node->children[i], hash, output,out);
                }
                break;

        case NODE_TEXT:
                tld_append(out, TLD_STR(node->content));
                /* fprintf(output, "%s", node->content); */
                break;

        case NODE_VARIABLE:
        {
                char* replacement = NULL;
                tld_template_get(hash, TLD_STR(node->content), &replacement);
                /* char* replacement = find_in_map(hash, TLD_STR(node->content)); */
                if(replacement){
                        tld_append(out, replacement);
                        /* fprintf(output, "%s", replacement); */
                }
                // If no replacement found, output nothing
        }
        break;

        case NODE_IF:
        {
                char* condition_value = NULL;//f ind_in_map(hash, TLD_STR(node->content));

                tld_template_get(hash, TLD_STR(node->content), &condition_value);
                int condition_met = (condition_value != NULL && strcmp(condition_value, "") != 0);

                if(condition_met){
                        // Process the 'if' branch
                        for(int i = 0; i < node->n; i++){
                                process_tree(node->children[i], hash, output,out);
                        }
                }else{
                        // Find and process the 'else' branch
                        template_node* current = node;
                        while(current->parent != NULL){
                                int sibling_index = -1;
                                for(int i = 0; i < current->parent->n; i++){
                                        if (current->parent->children[i] == current){
                                                sibling_index = i;
                                                break;
                                        }
                                }
                                if(sibling_index != -1 && sibling_index + 1 < current->parent->n){
                                        template_node* next_sibling = current->parent->children[sibling_index + 1];
                                        if(next_sibling->type == NODE_ELSE){
                                                for(int i = 0; i < next_sibling->n; i++){
                                                        process_tree(next_sibling->children[i], hash, output,out);
                                                }
                                                break;
                                        }
                                }
                               current = current->parent;
                        }
                }
        }
        break;

        case NODE_ELSE:
        case NODE_ENDIF:
                // These are handled in the IF case, do nothing here
                break;
        }
}

int tld_template_apply(tld_strbuf *txt, tld_template_hash *hash)
{
        char* tmp = NULL;

        template_node* root = NULL;
        /* Node *root = NULL; */
        tld_strbuf* out = NULL;

        /* Parser parser  = NULL; */
        galloc(&tmp,txt->len+1);

        memcpy((void*)tmp, txt->str, txt->len);
        tmp[txt->len] = '\0';
        Parser parser = {tmp, 0, ""};
        /* root = parse_template(&parser); */
        parse_template(&parser, &root);
        if (parser.error[0] != '\0') {
                ERROR_MSG("Error parsing template: %s",parser.error);
        }
        /* print_tree(root, 0); */
        RUN(tld_strbuf_alloc(&out, 256));
        process_tree(root, hash, stdout,out);
        RUN(tld_strbuf_copy(txt, out));

        template_node_free(root);
        /* free_node(root); */

        tld_strbuf_free(out);
        gfree(tmp);
        return OK;
ERROR:
        if(out){
                tld_strbuf_free(out);
        }
        if(root){
                template_node_free(root);
        }
        if(tmp){
                gfree(tmp);
        }
        return FAIL;
}


/* int tld_template_apply2(tld_strbuf *txt, tld_template_map *map) */
/* { */
/*         /\* loop until there are no more valid reps  *\/ */
/*         tld_strbuf* out = NULL; */
/*         tld_strbuf* pattern = NULL; */
/*         int offset = 0; */
/*         int start; */
/*         int stop; */

/*         int read_header = 0; */
/*         /\* int substitutions_made = 0; *\/ */

/*         map->status = TEMPLATE_OK; */
/*         map->n_rep = 0; */

/*         RUN(tld_strbuf_alloc(&out, 32)); */
/*         RUN(tld_strbuf_alloc(&pattern, 32)); */


/*         while(1){ */
/*                 start = -1; */
/*                 stop  = -1; */
/*                 find_open(txt, offset, map->delim_start, &start); */
/*                 if(start != -1){ */
/*                         find_end(txt, start, map->delim_end, &stop); */
/*                         if(stop != -1){ */
/*                                 offset = stop; */
/*                                 pattern->len = 0; */
/*                                 for(int i = start; i < stop;i++){ */
/*                                         RUN(tld_append_char(pattern, txt->str[i])); */
/*                                         /\* fprintf(stdout,"%c",txt->str[i]); *\/ */
/*                                 } */
/*                                 int internal_start = -1; */
/*                                 find_open(pattern, 0, map->delim_start, &internal_start); */
/*                                 if(internal_start != -1){ */
/*                                         map->status = TEMPLATE_MISMATCH_DELIM; */
/*                                 } */

/*                                 /\* fprintf(stdout,"%s (%d - %d)\n", TLD_STR(pattern), start , stop); *\/ */
/*                                 int idx = -1; */
/*                                 int a = TLD_STRLEN(pattern); */
/*                                 for(int i = 0; i < map->n;i++){ */
/*                                         int b = TLD_STRLEN(map->identifier[i]); */
/*                                         if(a == b){ */
/*                                                 if(strncmp(TLD_STR(pattern), TLD_STR(map->identifier[i]), a) == 0){ */
/*                                                         idx = i; */
/*                                                         break; */
/*                                                 } */
/*                                         } */
/*                                 } */
/*                                 if(idx != -1){ */
/*                                         /\* fprintf(stdout,"%s -> %s \n", TLD_STR(map->identifier[idx]), TLD_STR(map->replacement[idx])); *\/ */

/*                                         for(int i = read_header; i < start-TLD_STRLEN(map->delim_start);i++){ */
/*                                                 RUN(tld_append_char(out, txt->str[i])); */
/*                                         } */
/*                                         RUN(tld_append(out,TLD_STR(map->replacement[idx]))); */
/*                                         read_header = stop + TLD_STRLEN(map->delim_end); */
/*                                         map->n_rep++; */
/*                                 }else{ */
/*                                         map->status = TEMPLATE_NOREP; */
/*                                         /\* WARNING_MSG("No replacement found"); *\/ */
/*                                 } */
/*                         }else{ */
/*                                 map->status = TEMPLATE_MISMATCH_DELIM; */
/*                                 /\* break; *\/ */
/*                         } */
/*                 }else{ */
/*                         break; */
/*                 } */
/*         } */
/*         if(read_header < TLD_STRLEN(txt)){ */
/*                 RUN(tld_append(out, TLD_STR(txt) + read_header)); */
/*         } */

/*         /\* LOG_MSG("%s", TLD_STR(text)); *\/ */
/*         /\* LOG_MSG("READ HEADER: %d", read_header); *\/ */
/*         RUN(tld_strbuf_copy(txt, out)); */

/*         tld_strbuf_free(pattern); */
/*         tld_strbuf_free(out); */
/*         return OK; */
/* ERROR: */
/*         if(pattern){ */
/*                 tld_strbuf_free(pattern); */
/*         } */
/*         if(out){ */
/*                 tld_strbuf_free(out); */
/*         } */
/*         return FAIL; */

/* } */



/* int tld_template_chk(tld_template_map *map, int verbose) */
/* { */
/*         if(verbose){ */
/*                 LOG_MSG("%d replacement made", map->n_rep); */
/*         } */
/*         switch (map->status) { */
/*         case TEMPLATE_OK: */
/*                 break; */
/*         case TEMPLATE_NOREP: */

/*                 ERROR_MSG("tag found in text but no matching replacement!"); */
/*                 break; */
/*         case TEMPLATE_MISMATCH_DELIM: */
/*                 ERROR_MSG("Mismatched deliminters!"); */
/*                 break; */
/*         } */
/*         return OK; */
/* ERROR: */
/*         return FAIL; */
/* } */

#undef TEMPLATE_OK
#undef TEMPLATE_MISMATCH_DELIM
#undef TEMPLATE_NOREP

/* int find(tld_strbuf *txt, int offset, tld_strbuf *delim,int*start_pos,int* end_pos) */
/* { */
/*         int d_len = -1; */
/*         int t_len = -1; */

/*         t_len = TLD_STRLEN(txt); */
/*         d_len = TLD_STRLEN(delim); */
/*         int start = -1; */
/*         int end= -1; */
/*         if(offset >= TLD_STRLEN(txt)){ */
/*                 return OK; */
/*         } */

/*         for(int i = offset; i < t_len-1;i++){ */
/*                 int m = 0; */
/*                 for(int j = 0; j < d_len;j++){ */
/*                         if(txt->str[i+j] != delim->str[j]){ */
/*                                 break; */
/*                         } */
/*                         m++; */
/*                 } */
/*                 if(m == d_len){ */
/*                         start = i; */
/*                         end = i+m; */
/*                         break; */
/*                 } */
/*         } */
/*         *start_pos = start; */
/*         *end_pos = end; */
/*         return OK; */
/* } */

int find_open(tld_strbuf *txt, int offset, tld_strbuf *delim,int*pos)
{
        int d_len = -1;
        int t_len = -1;

        t_len = TLD_STRLEN(txt);
        d_len = TLD_STRLEN(delim);
        int p = -1;
        if(offset >= TLD_STRLEN(txt)){
                return OK;
        }

        for(int i = offset; i < t_len-1;i++){
                int m = 0;
                for(int j = 0; j < d_len;j++){
                        if(txt->str[i+j] != delim->str[j]){
                                break;
                        }
                        m++;
                }
                if(m == d_len){
                        p = i+m;
                        break;
                }
        }
        *pos = p;
        return OK;
}

int find_end(tld_strbuf *txt, int offset, tld_strbuf *delim,int*pos)
{
        int d_len = -1;
        int t_len = -1;

        t_len = TLD_STRLEN(txt);
        d_len = TLD_STRLEN(delim);
        int p = -1;
        if(offset >= TLD_STRLEN(txt)){
                return OK;
        }

        for(int i = offset; i < t_len-1;i++){
                int m = 0;
                for(int j = 0; j < d_len;j++){
                        if(txt->str[i+j] != delim->str[j]){
                                break;
                        }
                        m++;
                }
                if(m == d_len){
                        p = i;
                        break;
                }
        }
        *pos = p;
        return OK;
}


/* int tld_template_add(tld_template_map **map, char *id, char *rep) */
/* { */
/*         tld_template_map* m = NULL; */
/*         int idx; */

/*         if(*map){ */
/*                 m = *map; */
/*         }else{ */
/*                 RUN(tld_template_allocate(&m, 16)); */
/*         } */

/*         ASSERT(m != NULL,"No Map"); */

/*         idx = m->n; */

/*         tld_append(m->identifier[idx], id); */
/*         tld_append(m->replacement[idx], rep); */
/*         m->n++; */
/*         if(m->n == m->n_alloc){ */
/*                 tld_template_resize(m); */
/*         } */
/*         *map = m; */
/*         return OK; */
/* ERROR: */
/*         return FAIL; */
/* } */

/* int tld_template_get(tld_template_map *m, char *key, char** val) */
/* { */
/*         ASSERT(m != NULL," No Map!!"); */
/*         ASSERT(key != NULL, "No key)"); */

/*         int k_len = strnlen(key, 1024); */

/*         *val = NULL; */
/*         for(int i = 0 ; i < m->n;i++){ */
/*                 int t_len = TLD_STRLEN(m->identifier[i]); */
/*                 if(k_len == t_len){ */
/*                         if(strncmp(key, TLD_STR(m->identifier[i]), k_len) == 0){ */
/*                                 *val = TLD_STR(m->replacement[i]); */
/*                                 break; */
/*                         } */
/*                 } */
/*         } */

/*         return OK; */
/* ERROR: */
/*         return FAIL; */
/* } */

/* int tld_template_rename_var(tld_template_map *m, char *key, char* new_key) */
/* { */
/*         ASSERT(m != NULL," No Map!!"); */
/*         ASSERT(key != NULL, "No key)"); */

/*         int k_len = strnlen(key, 1024); */


/*         for(int i = 0 ; i < m->n;i++){ */
/*                 int t_len = TLD_STRLEN(m->identifier[i]); */
/*                 if(k_len == t_len){ */
/*                         if(strncmp(key, TLD_STR(m->identifier[i]), k_len) == 0){ */
/*                                 tld_strbuf_clear(m->identifier[i]); */
/*                                 tld_append(m->identifier[i], new_key); */
/*                                 break; */
/*                         } */
/*                 } */
/*         } */

/*         return OK; */
/* ERROR: */
/*         return FAIL; */
/* } */


int template_node_create(template_node **node, template_node_type type, char *content)
{
        template_node* n = NULL;
        MMALLOC(n, sizeof(template_node));
        n->type = type;
        n->content = NULL;
        n->n = 0;
        n->n_alloc = 10;
        n->children = NULL;
        MMALLOC(n->children, sizeof(template_node*) * n->n_alloc);
        for(int i = 0; i < n->n_alloc;i++){
                n->children[i] = NULL;
        }
        if(content){
                tld_strbuf_alloc(&n->content, 16);
                tld_append(n->content, content);
        }
        *node = n;
        return OK;
ERROR:
        template_node_free(n);
        return FAIL;
}

int template_node_resize(template_node *node)
{
        int old_size = node->n_alloc;
        node->n_alloc = node->n_alloc + node->n_alloc /2;
        MREALLOC(node->children, sizeof(template_node*) * node->n_alloc);
        for(int i = old_size; i < node->n_alloc;i++){
                node->children[i] = NULL;
        }
        return OK;
ERROR:
        return FAIL;
}

void template_node_free(template_node *node)
{
        if(node){
                if(node->content){
                        tld_strbuf_free(node->content);
                }
                if(node->children){
                        for(int i = 0; i < node->n_alloc;i++){
                                template_node_free(node->children[i]);
                        }
                        MFREE(node->children);
                }
                MFREE(node);
        }
}

int is_end_of_input(Parser *parser)
{
        return parser->input[parser->position] == '\0';
}

char peek(Parser *parser)
{
        return parser->input[parser->position];
}

char parser_next(Parser *parser)
{
        if(is_end_of_input(parser)){
                snprintf(parser->error, MAX_ERROR_MSG, "Unexpected end of input");
                return '\0';
        }
        return parser->input[parser->position++];
}

void skip_whitespace(Parser *parser)
{
        while(isspace(peek(parser))){
                parser_next(parser);
        }
}


/* int tld_template_init(tld_template_map **map, char **id, char **rep, int n) */
/* { */
/*         tld_template_map* m = NULL; */

/*         RUN(tld_template_allocate(&m, n)); */
/*         /\* MMALLOC(m, sizeof(tld_template_map)); *\/ */
/*         m->n = 0; */
/*         for(int i = 0; i < n;i++){ */
/*                 tld_append(m->identifier[i], id[i]); */
/*                 tld_append(m->replacement[i], rep[i]); */
/*                 m->n++; */
/*         } */

/*         *map = m; */
/*         return OK; */
/* ERROR: */
/*         tld_template_free(m); */
/*         return FAIL; */
/* } */

/* int tld_template_allocate(tld_template_map **map, int n) */
/* { */
/*         tld_template_map* m = NULL; */
/*         MMALLOC(m, sizeof(tld_template_map)); */
/*         m->identifier = NULL; */
/*         m->replacement = NULL; */
/*         m->delim_start = NULL; */
/*         m->delim_end = NULL; */
/*         m->if_delim_start = NULL; */
/*         m->if_delim_end = NULL; */
/*         m->n = 0; */
/*         m->n_alloc = n; */
/*         m->status = 0; */
/*         m->n_rep = 0; */
/*         MMALLOC(m->identifier, sizeof(tld_strbuf*) * m->n_alloc); */
/*         MMALLOC(m->replacement, sizeof(tld_strbuf*) * m->n_alloc); */


/*         for(int i = 0; i < m->n_alloc;i++){ */
/*                 m->identifier[i] = NULL; */
/*                 m->replacement[i] = NULL; */
/*                 tld_strbuf_alloc(&m->identifier[i], 16); */
/*                 tld_strbuf_alloc(&m->replacement[i], 16); */
/*         } */
/*         /\* Default delim = {{ and }} *\/ */
/*         tld_strbuf_alloc(&m->delim_start, 3); */
/*         tld_strbuf_alloc(&m->delim_end, 3); */

/*         tld_append(m->delim_start, "{{"); */
/*         tld_append(m->delim_end,"}}"); */

/*         tld_strbuf_alloc(&m->if_delim_start, 8); */
/*         tld_strbuf_alloc(&m->if_delim_end, 16); */
/*         tld_append(m->if_delim_start, "{{IF "); */
/*         tld_append(m->if_delim_end,"{{ENDIF}}"); */

/*         *map = m; */

/*         return OK; */
/* ERROR: */
/*         tld_template_free(m); */
/*         return FAIL; */
/* } */

/* int tld_template_resize(tld_template_map *m) */
/* { */
/*         int old_size = m->n_alloc; */
/*         m->n_alloc = m->n_alloc + m->n_alloc /2; */


/*         MREALLOC(m->identifier, sizeof(tld_strbuf*) * m->n_alloc); */
/*         MREALLOC(m->replacement, sizeof(tld_strbuf*) * m->n_alloc); */


/*         for(int i = old_size; i < m->n_alloc;i++){ */
/*                 m->identifier[i] = NULL; */
/*                 m->replacement[i] = NULL; */
/*                 tld_strbuf_alloc(&m->identifier[i], 16); */
/*                 tld_strbuf_alloc(&m->replacement[i], 16); */
/*         } */

/*         return OK; */
/* ERROR: */
/*         tld_template_free(m); */
/*         return FAIL; */
/* } */


/* void tld_template_free(tld_template_map *m) */
/* { */
/*         if(m){ */
/*                 if(m->n_alloc){ */
/*                         for(int i = 0; i < m->n_alloc;i++){ */
/*                                 tld_strbuf_free(m->identifier[i]); */
/*                                 tld_strbuf_free(m->replacement[i]); */

/*                         } */
/*                         MFREE(m->identifier); */
/*                         MFREE(m->replacement); */
/*                 } */
/*                 if(m->delim_start){ */
/*                         tld_strbuf_free(m->delim_start); */
/*                 } */
/*                 if(m->delim_end){ */
/*                         tld_strbuf_free(m->delim_end); */
/*                 } */
/*                 if(m->if_delim_start){ */
/*                         tld_strbuf_free(m->if_delim_start); */
/*                 } */
/*                 if(m->if_delim_end){ */
/*                         tld_strbuf_free(m->if_delim_end); */
/*                 } */
/*                 MFREE(m); */
/*         } */
/* } */
