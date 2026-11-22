#include "template.h"

#include "../core/tld-core.h"
#include "../alloc/tld-alloc.h"
#include "../string/str.h"
#include "../hash/hash.h"

#include <stdio.h>
#include <ctype.h>

#define TEMPLATE_OK 0
#define TEMPLATE_MISMATCH_DELIM 1
#define TEMPLATE_NOREP 2

#define MAX_VAR_NAME 64
#define MAX_CHILDREN 10
#define MAX_ERROR_MSG 256
#define MAX_IF_DEPTH 512

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

tld_internal  int parse_template(Parser *parser, template_node **node);
tld_internal  int process_tree(template_node *node, tld_hash *hash, FILE *output,tld_strbuf* out);

tld_internal  int get_vars(template_node *node, tld_hash* h);
tld_internal  int parse_text(Parser *parser, template_node **node);
tld_internal  int parse_tag(Parser *parser, template_node **node);

tld_internal  int is_end_of_input(Parser *parser);
tld_internal char peek(Parser *parser);
tld_internal char parser_next(Parser *parser);

tld_internal  int template_node_add_child(template_node *parent, template_node *child);
tld_internal  int template_node_create(template_node **node, template_node_type type, char *content);
tld_internal  int template_node_resize(template_node *node);
tld_internal void template_node_free(template_node *node);

#undef TEMPLATE_OK
#undef TEMPLATE_MISMATCH_DELIM
#undef TEMPLATE_NOREP

/* Parse the text in txt into a new tree in *root (nodes copy their text, so the
   temporary NUL-terminated copy is freed here). */
static int template_parse_strbuf(tld_strbuf *txt, template_node **root)
{
        char* tmp = NULL;
        template_node* r = NULL;
        galloc(&tmp,txt->len+1);
        memcpy((void*)tmp, txt->str, txt->len);
        tmp[txt->len] = '\0';
        Parser parser = {tmp, 0, ""};
        RUN(parse_template(&parser, &r));
        if (parser.error[0] != '\0') {
                ERROR_MSG("Error parsing template: %s",parser.error);
        }
        gfree(tmp);
        *root = r;
        return OK;
ERROR:
        if(r){
                template_node_free(r);
        }
        gfree(tmp);
        return FAIL;
}

int process_tree(template_node *node, tld_hash *hash, FILE *output,tld_strbuf* out)
{
        switch(node->type){
        case NODE_ROOT:
                for(int i = 0; i < node->n; i++){
                        RUN(process_tree(node->children[i], hash, output,out));
                }
                break;

        case NODE_TEXT:
                RUN(tld_append(out, TLD_STR(node->content)));
                break;

        case NODE_VARIABLE:
        {
                char* replacement = NULL;
                tld_hash_get(hash, TLD_STR(node->content), &replacement);
                if(replacement){
                        RUN(tld_append(out, replacement));
                }
                // If no replacement found, output nothing
        }
        break;

        case NODE_IF:
        {
                char* condition_value = NULL;

                tld_hash_get(hash, TLD_STR(node->content), &condition_value);
                int condition_met = (condition_value != NULL && strcmp(condition_value, "") != 0);

                if(condition_met){
                        // Process the 'if' branch
                        for(int i = 0; i < node->n; i++){
                                RUN(process_tree(node->children[i], hash, output,out));
                        }
                }else if(node->parent != NULL){
                        /* The parser places an IF's own ELSE directly after
                           it among the parent's children. Only that ELSE
                           belongs to this IF; an ELSE further up belongs to
                           an enclosing IF. */
                        template_node* parent = node->parent;
                        for(int i = 0; i < parent->n; i++){
                                if(parent->children[i] == node){
                                        if(i + 1 < parent->n && parent->children[i + 1]->type == NODE_ELSE){
                                                template_node* else_node = parent->children[i + 1];
                                                for(int j = 0; j < else_node->n; j++){
                                                        RUN(process_tree(else_node->children[j], hash, output,out));
                                                }
                                        }
                                        break;
                                }
                        }
                }
        }
        break;

        case NODE_ELSE:
        case NODE_ENDIF:
                // These are handled in the IF case, do nothing here
                break;
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

int parse_template(Parser *parser, template_node **node)
{
        template_node *root = NULL;
        template_node *current = NULL;
        template_node** stack = NULL;
        int stack_top = 0;
        int stack_size = 10;
        MMALLOC(stack, sizeof(template_node*) * stack_size);

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
                                        if(stack_top >= MAX_IF_DEPTH){
                                                snprintf(parser->error, MAX_ERROR_MSG, "IF nesting deeper than %d", MAX_IF_DEPTH);
                                                break;
                                        }
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

        if(stack_top > 0 && parser->error[0] == '\0'){
                snprintf(parser->error, MAX_ERROR_MSG, "Unclosed IF statement");
        }
        MFREE(stack);
        *node = root;
        return OK;
ERROR:
        if(stack){
                MFREE(stack);
        }
        if(root){
                template_node_free(root);
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
                if(template_node_create(node, NODE_TEXT, text) != OK){
                        MFREE(text);
                        ERROR_MSG("Could not create text node");
                }
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

int tld_template_apply(tld_strbuf *txt, tld_hash *hash)
{
        template_node* root = NULL;
        tld_strbuf* out = NULL;

        RUN(template_parse_strbuf(txt, &root));
        RUN(tld_strbuf_alloc(&out, 256));
        RUN(process_tree(root, hash, stdout,out));
        RUN(tld_strbuf_copy(txt, out));

        template_node_free(root);

        tld_strbuf_free(out);
        return OK;
ERROR:
        if(out){
                tld_strbuf_free(out);
        }
        if(root){
                template_node_free(root);
        }
        return FAIL;
}

int get_vars(template_node *node, tld_hash* h)
{
        switch(node->type){
        case NODE_ROOT:
                /* printf("ROOT\n"); */
                break;
        case NODE_TEXT:
                /* printf("TEXT: %s\n", TLD_STR(node->content)); */
                break;
        case NODE_VARIABLE:
                tld_hash_add(&h, TLD_STR(node->content),TLD_STR(node->content));
                /* printf("VARIABLE: %s\n", TLD_STR(node->content)); */
                break;
        case NODE_IF:
                /* printf("IF: %s\n", TLD_STR(node->content)); */
                tld_hash_add(&h, TLD_STR(node->content),TLD_STR(node->content));

                break;
        case NODE_ELSE:
                /* printf("ELSE\n"); */
                break;
        case NODE_ENDIF:
                /* printf("ENDIF\n"); */
                break;
        }

        for(int i = 0; i < node->n; i++){
                RUN(get_vars(node->children[i], h));
        }
        return OK;
ERROR:
        return FAIL;
}

int tld_template_extract_var(tld_strbuf *txt, tld_hash **hash)
{
        tld_hash* h = NULL;
        template_node* root = NULL;

        RUN(template_parse_strbuf(txt, &root));
        RUN(tld_hash_alloc(&h,1024));
        RUN(get_vars(root, h));

        template_node_free(root);
        *hash = h;
        return OK;
ERROR:
        if(h){
                tld_hash_free(h);
        }
        if(root){
                template_node_free(root);
        }
        return FAIL;
}
