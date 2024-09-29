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

typedef enum {
        NODE_ROOT,
        NODE_TEXT,
        NODE_VARIABLE,
        NODE_IF,
        NODE_ELSE,
        NODE_ENDIF
} NodeType;

typedef struct Node {
        NodeType type;
        char *content;
        struct Node *children[MAX_CHILDREN];
        int child_count;
        struct Node *parent;
} Node;

typedef struct {
        const char *input;
        int position;
        char error[MAX_ERROR_MSG];
} Parser;

/* tld_internal int find(tld_strbuf *txt, int offset, tld_strbuf *delim,int*start_pos,int* end_pos); */
tld_internal int find_open(tld_strbuf *txt,int offset, tld_strbuf *delim, int*pos);
tld_internal int find_end(tld_strbuf *txt,int offset, tld_strbuf* delim, int*pos);

tld_internal int tld_template_allocate(tld_template_map **map, int n);
tld_internal int tld_template_resize(tld_template_map *m);

/* Node *create_node(NodeType type, const char *content) { */
/*         Node *node = (Node *)malloc(sizeof(Node)); */
/*         if (!node) { */
/*                 fprintf(stderr, "Memory allocation failed\n"); */
/*                 exit(1); */
/*         } */
/*         node->type = type; */
/*         node->content = content ? strdup(content) : NULL; */
/*         node->child_count = 0; */
/*         node->parent = NULL; */
/*         return node; */
/* } */

Node *create_node(NodeType type, const char *content) {
        Node *node = (Node *)malloc(sizeof(Node));
        if (!node) {
                fprintf(stderr, "Memory allocation failed for node\n");
                exit(1);
        }
        node->type = type;
        node->content = content ? strdup(content) : NULL;
        if (content && !node->content) {
                fprintf(stderr, "Memory allocation failed for node content\n");
                free(node);
                exit(1);
        }
        node->child_count = 0;
        node->parent = NULL;
        return node;
}

void free_node(Node *node) {
        if (node) {
                free(node->content);
                for (int i = 0; i < node->child_count; i++) {
                        free_node(node->children[i]);
                }
                free(node);
        }
}

void add_child(Node *parent, Node *child) {
        if (parent->child_count < MAX_CHILDREN) {
                parent->children[parent->child_count++] = child;
                child->parent = parent;
        } else {
                fprintf(stderr, "Maximum number of children exceeded\n");
                free_node(child);
        }
}

/* void free_node(Node *node) { */
/*         if (node) { */
/*                 free(node->content); */
/*                 for (int i = 0; i < node->child_count; i++) { */
/*                         free_node(node->children[i]); */
/*                 } */
/*                 free(node); */
/*         } */
/* } */


/* void add_child(Node *parent, Node *child) { */
/*         if (parent->child_count < MAX_CHILDREN) { */
/*                 parent->children[parent->child_count++] = child; */
/*                 child->parent = parent; */
/*         } else { */
/*                 fprintf(stderr, "Error: Maximum number of children exceeded\n"); */
/*                 free_node(child); */
/*         } */
/* } */


int is_end_of_input(Parser *parser) {
        return parser->input[parser->position] == '\0';
}

char peek(Parser *parser) {
        return parser->input[parser->position];
}

char parser_next(Parser *parser) {
        if (is_end_of_input(parser)) {
                snprintf(parser->error, MAX_ERROR_MSG, "Unexpected end of input");
                return '\0';
        }
        return parser->input[parser->position++];
}

void skip_whitespace(Parser *parser) {
        while (isspace(peek(parser))) {
                parser_next(parser);
        }
}

Node *parse_text(Parser *parser) {
        const char *start = parser->input + parser->position;
        while (!is_end_of_input(parser) && (peek(parser) != '{' || parser->input[parser->position + 1] != '{')) {
                parser_next(parser);
        }
        int length = parser->input + parser->position - start;
        if (length > 0) {
                char *text = (char *)malloc(length + 1);
                if (!text) {
                        snprintf(parser->error, MAX_ERROR_MSG, "Memory allocation failed");
                        return NULL;
                }
                strncpy(text, start, length);
                text[length] = '\0';
                return create_node(NODE_TEXT, text);
        }
        return NULL;
}

Node *parse_tag(Parser *parser) {
        char tag_content[MAX_VAR_NAME];
        int i = 0;
        if (parser_next(parser) != '{' || parser_next(parser) != '{') {
                snprintf(parser->error, MAX_ERROR_MSG, "Invalid tag opening");
                return NULL;
        }
        while (!is_end_of_input(parser) && (peek(parser) != '}' || parser->input[parser->position + 1] != '}')) {
                if (i < MAX_VAR_NAME - 1) {
                        tag_content[i++] = parser_next(parser);
                } else {
                        snprintf(parser->error, MAX_ERROR_MSG, "Tag content too long");
                        return NULL;
                }
        }
        tag_content[i] = '\0';
        if (parser_next(parser) != '}' || parser_next(parser) != '}') {
                snprintf(parser->error, MAX_ERROR_MSG, "Invalid tag closing");
                return NULL;
        }

        if (strcmp(tag_content, "ELSE") == 0) {
                return create_node(NODE_ELSE, NULL);
        } else if (strcmp(tag_content, "ENDIF") == 0) {
                return create_node(NODE_ENDIF, NULL);
        } else if (strncmp(tag_content, "IF ", 3) == 0) {
                return create_node(NODE_IF, tag_content + 3);
        } else {
                return create_node(NODE_VARIABLE, tag_content);
        }
}

Node *parse_template(Parser *parser) {
        Node *root = create_node(NODE_ROOT, NULL);
        Node *current = root;
        Node *stack[MAX_CHILDREN];
        int stack_top = 0;

        while (!is_end_of_input(parser)) {
                if (strncmp(parser->input + parser->position, "{{", 2) == 0) {
                        Node *tag_node = parse_tag(parser);
                        if (tag_node) {
                                switch (tag_node->type) {
                                case NODE_IF:
                                        add_child(current, tag_node);
                                        if (stack_top >= MAX_CHILDREN) {
                                                snprintf(parser->error, MAX_ERROR_MSG, "Nesting too deep");
                                                return root;
                                        }
                                        stack[stack_top++] = current;
                                        current = tag_node;
                                        break;
                                case NODE_ELSE:
                                        if (stack_top > 0 && current->type == NODE_IF) {
                                                current = stack[stack_top - 1];
                                                add_child(current, tag_node);
                                                current = tag_node;
                                        } else {
                                                snprintf(parser->error, MAX_ERROR_MSG, "ELSE without matching IF");
                                                free_node(tag_node);
                                        }
                                        break;
                                case NODE_ENDIF:
                                        if (stack_top > 0) {
                                                current = stack[--stack_top];
                                        } else {
                                                snprintf(parser->error, MAX_ERROR_MSG, "ENDIF without matching IF");
                                                free_node(tag_node);
                                        }
                                        break;
                                default:
                                        add_child(current, tag_node);
                                }
                        } else if (parser->error[0] == '\0') {
                                snprintf(parser->error, MAX_ERROR_MSG, "Failed to parse tag");
                        }
                } else {
                        Node *text_node = parse_text(parser);
                        if (text_node) {
                                add_child(current, text_node);
                        } else if (parser->error[0] == '\0') {
                                snprintf(parser->error, MAX_ERROR_MSG, "Failed to parse text");
                        }
                }

                if (parser->error[0] != '\0') {
                        break;
                }
        }

        if (stack_top > 0) {
                snprintf(parser->error, MAX_ERROR_MSG, "Unclosed IF statement");
        }

        return root;
}

void print_tree(Node *node, int depth) {
        for (int i = 0; i < depth; i++) {
                printf("  ");
        }

        switch (node->type) {
        case NODE_ROOT:
                printf("ROOT\n");
                break;
        case NODE_TEXT:
                printf("TEXT: %s\n", node->content);
                break;
        case NODE_VARIABLE:
                printf("VARIABLE: %s\n", node->content);
                break;
        case NODE_IF:
                printf("IF: %s\n", node->content);
                break;
        case NODE_ELSE:
                printf("ELSE\n");
                break;
        case NODE_ENDIF:
                printf("ENDIF\n");
                break;
        }

        for (int i = 0; i < node->child_count; i++) {
                print_tree(node->children[i], depth + 1);
        }
}

char* find_in_map(const tld_template_map* map, const char* key) {
        for (int i = 0; i < map->n; i++) {
                if (strcmp(TLD_STR(map->identifier[i]), key) == 0) {
                        return TLD_STR(map->replacement[i]);
                }
        }

        return NULL;
}

void process_tree(Node *node, const tld_template_map *map, FILE *output,tld_strbuf* out)
{
        switch(node->type){
        case NODE_ROOT:
                for(int i = 0; i < node->child_count; i++){
                        process_tree(node->children[i], map, output,out);
                }
                break;

        case NODE_TEXT:
                tld_append(out, node->content);
                /* fprintf(output, "%s", node->content); */
                break;

        case NODE_VARIABLE:
        {
                char* replacement = find_in_map(map, node->content);
                if(replacement){
                        tld_append(out, replacement);
                        /* fprintf(output, "%s", replacement); */
                }
                // If no replacement found, output nothing
        }
        break;

        case NODE_IF:
        {
                const char* condition_value = find_in_map(map, node->content);
                int condition_met = (condition_value != NULL && strcmp(condition_value, "") != 0);

                if(condition_met){
                        // Process the 'if' branch
                        for(int i = 0; i < node->child_count; i++){
                                process_tree(node->children[i], map, output,out);
                        }
                }else{
                        // Find and process the 'else' branch
                        Node* current = node;
                        while(current->parent != NULL){
                                int sibling_index = -1;
                                for(int i = 0; i < current->parent->child_count; i++){
                                        if (current->parent->children[i] == current){
                                                sibling_index = i;
                                                break;
                                        }
                                }
                                if(sibling_index != -1 && sibling_index + 1 < current->parent->child_count){
                                        Node* next_sibling = current->parent->children[sibling_index + 1];
                                        if(next_sibling->type == NODE_ELSE){
                                                for(int i = 0; i < next_sibling->child_count; i++){
                                                        process_tree(next_sibling->children[i], map, output,out);
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

int tld_template_apply(tld_strbuf *txt, tld_template_map *map)
{
        char* tmp = NULL;
        Node *root = NULL;
        tld_strbuf* out = NULL;

        /* Parser parser  = NULL; */
        galloc(&tmp,txt->len+1);

        memcpy((void*)tmp, txt->str, txt->len);

        Parser parser = {tmp, 0, ""};
        root = parse_template(&parser);
        if (parser.error[0] != '\0') {
                ERROR_MSG("Error parsing template");
        }
        /* print_tree(root, 0); */
        RUN(tld_strbuf_alloc(&out, 256));
        process_tree(root, map, stdout,out);
        RUN(tld_strbuf_copy(txt, out));
        tld_strbuf_free(out);
        gfree(tmp);
        return OK;
ERROR:
        if(out){
                tld_strbuf_free(out);
        }
        if(root){
                free_node(root);
        }
        if(tmp){
                gfree(tmp);
        }
        return FAIL;
}


int tld_template_apply2(tld_strbuf *txt, tld_template_map *map)
{
        /* loop until there are no more valid reps  */
        tld_strbuf* out = NULL;
        tld_strbuf* pattern = NULL;
        int offset = 0;
        int start;
        int stop;

        int read_header = 0;
        /* int substitutions_made = 0; */

        map->status = TEMPLATE_OK;
        map->n_rep = 0;

        RUN(tld_strbuf_alloc(&out, 32));
        RUN(tld_strbuf_alloc(&pattern, 32));

        
        while(1){
                start = -1;
                stop  = -1;
                find_open(txt, offset, map->delim_start, &start);
                if(start != -1){
                        find_end(txt, start, map->delim_end, &stop);
                        if(stop != -1){
                                offset = stop;
                                pattern->len = 0;
                                for(int i = start; i < stop;i++){
                                        RUN(tld_append_char(pattern, txt->str[i]));
                                        /* fprintf(stdout,"%c",txt->str[i]); */
                                }
                                int internal_start = -1;
                                find_open(pattern, 0, map->delim_start, &internal_start);
                                if(internal_start != -1){
                                        map->status = TEMPLATE_MISMATCH_DELIM;
                                }

                                /* fprintf(stdout,"%s (%d - %d)\n", TLD_STR(pattern), start , stop); */
                                int idx = -1;
                                int a = TLD_STRLEN(pattern);
                                for(int i = 0; i < map->n;i++){
                                        int b = TLD_STRLEN(map->identifier[i]);
                                        if(a == b){
                                                if(strncmp(TLD_STR(pattern), TLD_STR(map->identifier[i]), a) == 0){
                                                        idx = i;
                                                        break;
                                                }
                                        }
                                }
                                if(idx != -1){
                                        /* fprintf(stdout,"%s -> %s \n", TLD_STR(map->identifier[idx]), TLD_STR(map->replacement[idx])); */

                                        for(int i = read_header; i < start-TLD_STRLEN(map->delim_start);i++){
                                                RUN(tld_append_char(out, txt->str[i]));
                                        }
                                        RUN(tld_append(out,TLD_STR(map->replacement[idx])));
                                        read_header = stop + TLD_STRLEN(map->delim_end);
                                        map->n_rep++;
                                }else{
                                        map->status = TEMPLATE_NOREP;
                                        /* WARNING_MSG("No replacement found"); */
                                }
                        }else{
                                map->status = TEMPLATE_MISMATCH_DELIM;
                                /* break; */
                        }
                }else{
                        break;
                }
        }
        if(read_header < TLD_STRLEN(txt)){
                RUN(tld_append(out, TLD_STR(txt) + read_header));
        }

        /* LOG_MSG("%s", TLD_STR(text)); */
        /* LOG_MSG("READ HEADER: %d", read_header); */
        RUN(tld_strbuf_copy(txt, out));

        tld_strbuf_free(pattern);
        tld_strbuf_free(out);
        return OK;
ERROR:
        if(pattern){
                tld_strbuf_free(pattern);
        }
        if(out){
                tld_strbuf_free(out);
        }
        return FAIL;

}



int tld_template_chk(tld_template_map *map, int verbose)
{
        if(verbose){
                LOG_MSG("%d replacement made", map->n_rep);
        }
        switch (map->status) {
        case TEMPLATE_OK:
                break;
        case TEMPLATE_NOREP:

                ERROR_MSG("tag found in text but no matching replacement!");
                break;
        case TEMPLATE_MISMATCH_DELIM:
                ERROR_MSG("Mismatched deliminters!");
                break;
        }
        return OK;
ERROR:
        return FAIL;
}

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


int tld_template_add(tld_template_map **map, char *id, char *rep)
{
        tld_template_map* m = NULL;
        int idx;

        if(*map){
                m = *map;
        }else{
                RUN(tld_template_allocate(&m, 16));
        }

        ASSERT(m != NULL,"No Map");

        idx = m->n;

        tld_append(m->identifier[idx], id);
        tld_append(m->replacement[idx], rep);
        m->n++;
        if(m->n == m->n_alloc){
                tld_template_resize(m);
        }
        *map = m;
        return OK;
ERROR:
        return FAIL;
}

int tld_template_get(tld_template_map *m, char *key, char** val)
{
        ASSERT(m != NULL," No Map!!");
        ASSERT(key != NULL, "No key)");

        int k_len = strnlen(key, 1024);

        *val = NULL;
        for(int i = 0 ; i < m->n;i++){
                int t_len = TLD_STRLEN(m->identifier[i]);
                if(k_len == t_len){
                        if(strncmp(key, TLD_STR(m->identifier[i]), k_len) == 0){
                                *val = TLD_STR(m->replacement[i]);
                                break;
                        }
                }
        }

        return OK;
ERROR:
        return FAIL;
}


int tld_template_init(tld_template_map **map, char **id, char **rep, int n)
{
        tld_template_map* m = NULL;

        RUN(tld_template_allocate(&m, n));
        /* MMALLOC(m, sizeof(tld_template_map)); */
        m->n = 0;
        for(int i = 0; i < n;i++){
                tld_append(m->identifier[i], id[i]);
                tld_append(m->replacement[i], rep[i]);
                m->n++;
        }

        *map = m;
        return OK;
ERROR:
        tld_template_free(m);
        return FAIL;
}

int tld_template_allocate(tld_template_map **map, int n)
{
        tld_template_map* m = NULL;
        MMALLOC(m, sizeof(tld_template_map));
        m->identifier = NULL;
        m->replacement = NULL;
        m->delim_start = NULL;
        m->delim_end = NULL;
        m->if_delim_start = NULL;
        m->if_delim_end = NULL;
        m->n = 0;
        m->n_alloc = n;
        m->status = 0;
        m->n_rep = 0;
        MMALLOC(m->identifier, sizeof(tld_strbuf*) * m->n_alloc);
        MMALLOC(m->replacement, sizeof(tld_strbuf*) * m->n_alloc);


        for(int i = 0; i < m->n_alloc;i++){
                m->identifier[i] = NULL;
                m->replacement[i] = NULL;
                tld_strbuf_alloc(&m->identifier[i], 16);
                tld_strbuf_alloc(&m->replacement[i], 16);
        }
        /* Default delim = {{ and }} */
        tld_strbuf_alloc(&m->delim_start, 3);
        tld_strbuf_alloc(&m->delim_end, 3);

        tld_append(m->delim_start, "{{");
        tld_append(m->delim_end,"}}");

        tld_strbuf_alloc(&m->if_delim_start, 8);
        tld_strbuf_alloc(&m->if_delim_end, 16);
        tld_append(m->if_delim_start, "{{IF ");
        tld_append(m->if_delim_end,"{{ENDIF}}");

        *map = m;

        return OK;
ERROR:
        tld_template_free(m);
        return FAIL;
}

int tld_template_resize(tld_template_map *m)
{
        int old_size = m->n_alloc;
        m->n_alloc = m->n_alloc + m->n_alloc /2;


        MREALLOC(m->identifier, sizeof(tld_strbuf*) * m->n_alloc);
        MREALLOC(m->replacement, sizeof(tld_strbuf*) * m->n_alloc);


        for(int i = old_size; i < m->n_alloc;i++){
                m->identifier[i] = NULL;
                m->replacement[i] = NULL;
                tld_strbuf_alloc(&m->identifier[i], 16);
                tld_strbuf_alloc(&m->replacement[i], 16);
        }

        return OK;
ERROR:
        tld_template_free(m);
        return FAIL;
}


void tld_template_free(tld_template_map *m)
{
        if(m){
                if(m->n_alloc){
                        for(int i = 0; i < m->n_alloc;i++){
                                tld_strbuf_free(m->identifier[i]);
                                tld_strbuf_free(m->replacement[i]);

                        }
                        MFREE(m->identifier);
                        MFREE(m->replacement);
                }
                if(m->delim_start){
                        tld_strbuf_free(m->delim_start);
                }
                if(m->delim_end){
                        tld_strbuf_free(m->delim_end);
                }
                if(m->if_delim_start){
                        tld_strbuf_free(m->if_delim_start);
                }
                if(m->if_delim_end){
                        tld_strbuf_free(m->if_delim_end);
                }
                MFREE(m);
        }
}
