#include "template.h"

#include "../core/tld-core.h"
#include "../alloc/tld-alloc.h"
#include "../string/str.h"
#include "../hash/hash.h"

#include <stdio.h>
#include <string.h>

#define MAX_VAR_NAME 64
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
        tld_strbuf *content;            /* text, or the name of a variable or IF */
        struct template_node** children;
        int n;
        int n_alloc;
        struct template_node* parent;
} template_node;

/* input is the template text; a syntax error leaves its reason in error. */
typedef struct {
        const char *input;
        int position;
        char error[MAX_ERROR_MSG];
} Parser;

tld_internal  int parse_template(Parser *parser, template_node **node);
tld_internal  int process_tree(template_node *node, tld_hash *hash, tld_strbuf* out);

tld_internal  int get_vars(template_node *node, tld_hash* h);
tld_internal  int parse_text(Parser *parser, template_node **node);
tld_internal  int parse_tag(Parser *parser, template_node **node);

tld_internal  int template_node_add_child(template_node *parent, template_node *child);
tld_internal  int template_node_create(template_node **node, template_node_type type, const char *content);
tld_internal  int template_node_resize(template_node *node);
tld_internal void template_node_free(template_node *node);

/* Parse the text in txt into a new tree in *root (nodes copy their text). A syntax
   error is printed with its reason. */
static int template_parse_strbuf(tld_strbuf *txt, template_node **root)
{
        template_node* r = NULL;
        Parser parser = {TLD_STR(txt), 0, ""};
        if(parse_template(&parser, &r) != OK){
                if(parser.error[0] != '\0'){
                        ERROR_MSG("Error parsing template: %s", parser.error);
                }
                ERROR_MSG("Could not parse the template");
        }
        *root = r;
        return OK;
ERROR:
        return FAIL;
}

int process_tree(template_node *node, tld_hash *hash, tld_strbuf* out)
{
        switch(node->type){
        case NODE_ROOT:
                for(int i = 0; i < node->n; i++){
                        RUN(process_tree(node->children[i], hash, out));
                }
                break;

        case NODE_TEXT:
                RUN(tld_append(out, TLD_STR(node->content)));
                break;

        case NODE_VARIABLE:
        {
                const char* replacement = NULL;
                /* an absent name gives no output */
                if(tld_hash_get(hash, TLD_STR(node->content), &replacement) == OK){
                        RUN(tld_append(out, replacement));
                }
        }
        break;

        case NODE_IF:
        {
                const char* condition_value = NULL;

                tld_hash_get(hash, TLD_STR(node->content), &condition_value);
                int condition_met = (condition_value != NULL && strcmp(condition_value, "") != 0);

                if(condition_met){
                        for(int i = 0; i < node->n; i++){
                                RUN(process_tree(node->children[i], hash, out));
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
                                                        RUN(process_tree(else_node->children[j], hash, out));
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
                /* handled by the IF they belong to */
                break;
        }
        return OK;
ERROR:
        return FAIL;
}

void template_node_free(template_node *node)
{
        if(node){
                tld_strbuf_free(node->content);
                for(int i = 0; i < node->n; i++){
                        template_node_free(node->children[i]);
                }
                MFREE(node->children);
                MFREE(node);
        }
}

/* Build the tree: text and variables are children of the innermost open IF (or
   ELSE, or the root). A syntax error sets parser->error and returns FAIL, as does
   running out of memory (error left empty). */
int parse_template(Parser *parser, template_node **node)
{
        template_node *root = NULL;
        template_node *current = NULL;
        template_node *child = NULL;
        template_node** stack = NULL;
        int stack_top = 0;
        int stack_size = 10;
        MMALLOC(stack, sizeof(template_node*) * stack_size);

        RUN(template_node_create(&root, NODE_ROOT, NULL));
        current = root;

        while(parser->input[parser->position] != '\0'){
                int at = parser->position;
                if(strncmp(parser->input + parser->position, "{{", 2) != 0){
                        RUN(parse_text(parser, &child));
                        RUN(template_node_add_child(current, child));
                        child = NULL;
                        continue;
                }
                if(parse_tag(parser, &child) != OK){
                        goto ERROR;
                }
                switch(child->type){
                case NODE_IF:
                        if(stack_top >= MAX_IF_DEPTH){
                                snprintf(parser->error, MAX_ERROR_MSG,
                                         "IF nesting deeper than %d at byte %d", MAX_IF_DEPTH, at);
                                goto ERROR;
                        }
                        if(stack_top >= stack_size){
                                stack_size = stack_size + stack_size / 2;
                                MREALLOC(stack, sizeof(template_node*) * stack_size);
                        }
                        RUN(template_node_add_child(current, child));
                        stack[stack_top++] = current;
                        current = child;
                        break;
                case NODE_ELSE:
                        if(stack_top == 0 || current->type != NODE_IF){
                                snprintf(parser->error, MAX_ERROR_MSG, "ELSE without matching IF at byte %d", at);
                                goto ERROR;
                        }
                        RUN(template_node_add_child(stack[stack_top - 1], child));
                        current = child;
                        break;
                case NODE_ENDIF:
                        if(stack_top == 0){
                                snprintf(parser->error, MAX_ERROR_MSG, "ENDIF without matching IF at byte %d", at);
                                goto ERROR;
                        }
                        template_node_free(child);
                        current = stack[--stack_top];
                        break;
                default:
                        RUN(template_node_add_child(current, child));
                        break;
                }
                child = NULL;
        }
        if(stack_top > 0){
                snprintf(parser->error, MAX_ERROR_MSG, "IF without ENDIF (%d still open at the end)", stack_top);
                goto ERROR;
        }
        MFREE(stack);
        *node = root;
        return OK;
ERROR:
        template_node_free(child);
        MFREE(stack);
        template_node_free(root);
        return FAIL;
}

/* Make child the last child of parent. The parent owns it afterwards; if that
   fails, the caller still does. */
int template_node_add_child(template_node *parent, template_node *child)
{
        if(parent->n == parent->n_alloc){
                RUN(template_node_resize(parent));
        }
        parent->children[parent->n++] = child;
        child->parent = parent;
        return OK;
ERROR:
        return FAIL;
}

/* Text up to the next {{ (or the end) as a new text node in *node. */
int parse_text(Parser *parser, template_node **node)
{
        char *text = NULL;
        const char *start = parser->input + parser->position;
        int length = 0;
        while(start[length] != '\0' && (start[length] != '{' || start[length + 1] != '{')){
                length++;
        }
        MMALLOC(text, length + 1);
        memcpy(text, start, length);
        text[length] = '\0';
        RUN(template_node_create(node, NODE_TEXT, text));
        MFREE(text);
        parser->position += length;
        return OK;
ERROR:
        MFREE(text);
        return FAIL;
}

/* The {{...}} tag at the parser's position as a new node in *node. A malformed tag
   leaves its reason in parser->error. */
int parse_tag(Parser *parser, template_node **node)
{
        char tag[MAX_VAR_NAME];
        int at = parser->position;
        int i = 0;
        const char *p = parser->input + parser->position + 2;   /* after the {{ */

        while(!(p[0] == '}' && p[1] == '}')){
                if(p[0] == '\0'){
                        snprintf(parser->error, MAX_ERROR_MSG, "Tag at byte %d is not closed with }}", at);
                        return FAIL;
                }
                if(i == MAX_VAR_NAME - 1){
                        snprintf(parser->error, MAX_ERROR_MSG,
                                 "Tag at byte %d is longer than %d characters", at, MAX_VAR_NAME - 1);
                        return FAIL;
                }
                tag[i++] = *p++;
        }
        tag[i] = '\0';
        parser->position = (int) (p + 2 - parser->input);

        if(i == 0){
                snprintf(parser->error, MAX_ERROR_MSG, "Empty tag {{}} at byte %d", at);
                return FAIL;
        }
        if(strcmp(tag, "ELSE") == 0){
                return template_node_create(node, NODE_ELSE, NULL);
        }
        if(strcmp(tag, "ENDIF") == 0){
                return template_node_create(node, NODE_ENDIF, NULL);
        }
        if(strcmp(tag, "IF") == 0 || strcmp(tag, "IF ") == 0){
                snprintf(parser->error, MAX_ERROR_MSG, "IF without a name at byte %d", at);
                return FAIL;
        }
        if(strncmp(tag, "IF ", 3) == 0){
                return template_node_create(node, NODE_IF, tag + 3);
        }
        return template_node_create(node, NODE_VARIABLE, tag);
}

int template_node_create(template_node **node, template_node_type type, const char *content)
{
        template_node* n = NULL;
        MMALLOC(n, sizeof(template_node));      /* zeroed: no children, no content */
        n->type = type;
        if(content){
                RUN(tld_strbuf_alloc(&n->content, 16));
                RUN(tld_append(n->content, content));
        }
        *node = n;
        return OK;
ERROR:
        template_node_free(n);
        return FAIL;
}

/* Room for more children: 4 at first, then half as many again. n_alloc changes only
   once the memory is there. */
int template_node_resize(template_node *node)
{
        int new_size = node->n_alloc ? node->n_alloc + node->n_alloc / 2 : 4;
        MREALLOC(node->children, sizeof(template_node*) * new_size);
        node->n_alloc = new_size;
        return OK;
ERROR:
        return FAIL;
}

int tld_template_apply(tld_strbuf *txt, tld_hash *hash)
{
        template_node* root = NULL;
        tld_strbuf* out = NULL;

        RUN(template_parse_strbuf(txt, &root));
        RUN(tld_strbuf_alloc(&out, 256));
        RUN(process_tree(root, hash, out));
        RUN(tld_strbuf_copy(txt, out));

        template_node_free(root);
        tld_strbuf_free(out);
        return OK;
ERROR:
        tld_strbuf_free(out);
        template_node_free(root);
        return FAIL;
}

int get_vars(template_node *node, tld_hash* h)
{
        if(node->type == NODE_VARIABLE || node->type == NODE_IF){
                RUN(tld_hash_add(&h, TLD_STR(node->content), TLD_STR(node->content)));
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
        tld_hash_free(h);
        template_node_free(root);
        return FAIL;
}
