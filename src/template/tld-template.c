#include "tld-template.h"

#include "../core/tld-core.h"
#include "../alloc/tld-alloc.h"
#include "../string/tld-string.h"
#include "../hash/tld-hash.h"

#include <stdio.h>
#include <string.h>

#define TLD__TEMPLATE_MAX_NAME 64
#define TLD__TEMPLATE_MAX_ERROR 256
#define TLD__TEMPLATE_MAX_IF_DEPTH 512

typedef enum {
        TLD__NODE_ROOT,
        TLD__NODE_TEXT,
        TLD__NODE_VARIABLE,
        TLD__NODE_IF,
        TLD__NODE_ELSE,
        TLD__NODE_ENDIF
} tld__template_node_type;

typedef struct tld__template_node {
        tld__template_node_type type;
        tld_strbuf *content;            /* text, or the name of a variable or IF */
        struct tld__template_node** children;
        int n;
        int n_alloc;
        struct tld__template_node* parent;
} tld__template_node;

/* input is the template text; a syntax error leaves its reason in error. */
typedef struct {
        const char *input;
        int position;
        char error[TLD__TEMPLATE_MAX_ERROR];
} tld__template_parser;

static  int tld__template_parse(tld__template_parser *parser, tld__template_node **node);
static  int tld__template_process(tld__template_node *node, tld_hash *hash, tld_strbuf* out);

static  int tld__template_get_vars(tld__template_node *node, tld_hash* h);
static  int tld__template_parse_text(tld__template_parser *parser, tld__template_node **node);
static  int tld__template_parse_tag(tld__template_parser *parser, tld__template_node **node);

static  int tld__template_node_add_child(tld__template_node *parent, tld__template_node *child);
static  int tld__template_node_create(tld__template_node **node, tld__template_node_type type, const char *content);
static  int tld__template_node_resize(tld__template_node *node);
static void tld__template_node_free(tld__template_node *node);

/* Parse the text in txt into a new tree in *root (nodes copy their text). A syntax
   error is printed with its reason. */
static int tld__template_parse_strbuf(tld_strbuf *txt, tld__template_node **root)
{
        tld__template_node* r = NULL;
        tld__template_parser parser = {TLD_STR(txt), 0, ""};
        if(tld__template_parse(&parser, &r) != OK){
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

int tld__template_process(tld__template_node *node, tld_hash *hash, tld_strbuf* out)
{
        switch(node->type){
        case TLD__NODE_ROOT:
                for(int i = 0; i < node->n; i++){
                        RUN(tld__template_process(node->children[i], hash, out));
                }
                break;

        case TLD__NODE_TEXT:
                RUN(tld_strbuf_append(out, TLD_STR(node->content)));
                break;

        case TLD__NODE_VARIABLE:
        {
                const char* replacement = NULL;
                /* an absent name gives no output */
                if(tld_hash_get(hash, TLD_STR(node->content), &replacement) == OK){
                        RUN(tld_strbuf_append(out, replacement));
                }
        }
        break;

        case TLD__NODE_IF:
        {
                const char* condition_value = NULL;

                tld_hash_get(hash, TLD_STR(node->content), &condition_value);
                int condition_met = (condition_value != NULL && strcmp(condition_value, "") != 0);

                if(condition_met){
                        for(int i = 0; i < node->n; i++){
                                RUN(tld__template_process(node->children[i], hash, out));
                        }
                }else if(node->parent != NULL){
                        /* The parser places an IF's own ELSE directly after
                           it among the parent's children. Only that ELSE
                           belongs to this IF; an ELSE further up belongs to
                           an enclosing IF. */
                        tld__template_node* parent = node->parent;
                        for(int i = 0; i < parent->n; i++){
                                if(parent->children[i] == node){
                                        if(i + 1 < parent->n && parent->children[i + 1]->type == TLD__NODE_ELSE){
                                                tld__template_node* else_node = parent->children[i + 1];
                                                for(int j = 0; j < else_node->n; j++){
                                                        RUN(tld__template_process(else_node->children[j], hash, out));
                                                }
                                        }
                                        break;
                                }
                        }
                }
        }
        break;

        case TLD__NODE_ELSE:
        case TLD__NODE_ENDIF:
                /* handled by the IF they belong to */
                break;
        }
        return OK;
ERROR:
        return FAIL;
}

void tld__template_node_free(tld__template_node *node)
{
        if(node){
                tld_strbuf_free(node->content);
                for(int i = 0; i < node->n; i++){
                        tld__template_node_free(node->children[i]);
                }
                MFREE(node->children);
                MFREE(node);
        }
}

/* Build the tree: text and variables are children of the innermost open IF (or
   ELSE, or the root). A syntax error sets parser->error and returns FAIL, as does
   running out of memory (error left empty). */
int tld__template_parse(tld__template_parser *parser, tld__template_node **node)
{
        tld__template_node *root = NULL;
        tld__template_node *current = NULL;
        tld__template_node *child = NULL;
        tld__template_node** stack = NULL;
        int stack_top = 0;
        int stack_size = 10;
        MMALLOC(stack, sizeof(tld__template_node*) * stack_size);

        RUN(tld__template_node_create(&root, TLD__NODE_ROOT, NULL));
        current = root;

        while(parser->input[parser->position] != '\0'){
                int at = parser->position;
                if(strncmp(parser->input + parser->position, "{{", 2) != 0){
                        RUN(tld__template_parse_text(parser, &child));
                        RUN(tld__template_node_add_child(current, child));
                        child = NULL;
                        continue;
                }
                if(tld__template_parse_tag(parser, &child) != OK){
                        goto ERROR;
                }
                switch(child->type){
                case TLD__NODE_IF:
                        if(stack_top >= TLD__TEMPLATE_MAX_IF_DEPTH){
                                snprintf(parser->error, TLD__TEMPLATE_MAX_ERROR,
                                         "IF nesting deeper than %d at byte %d", TLD__TEMPLATE_MAX_IF_DEPTH, at);
                                goto ERROR;
                        }
                        if(stack_top >= stack_size){
                                stack_size = stack_size + stack_size / 2;
                                MREALLOC(stack, sizeof(tld__template_node*) * stack_size);
                        }
                        RUN(tld__template_node_add_child(current, child));
                        stack[stack_top++] = current;
                        current = child;
                        break;
                case TLD__NODE_ELSE:
                        if(stack_top == 0 || current->type != TLD__NODE_IF){
                                snprintf(parser->error, TLD__TEMPLATE_MAX_ERROR, "ELSE without matching IF at byte %d", at);
                                goto ERROR;
                        }
                        RUN(tld__template_node_add_child(stack[stack_top - 1], child));
                        current = child;
                        break;
                case TLD__NODE_ENDIF:
                        if(stack_top == 0){
                                snprintf(parser->error, TLD__TEMPLATE_MAX_ERROR, "ENDIF without matching IF at byte %d", at);
                                goto ERROR;
                        }
                        tld__template_node_free(child);
                        current = stack[--stack_top];
                        break;
                default:
                        RUN(tld__template_node_add_child(current, child));
                        break;
                }
                child = NULL;
        }
        if(stack_top > 0){
                snprintf(parser->error, TLD__TEMPLATE_MAX_ERROR, "IF without ENDIF (%d still open at the end)", stack_top);
                goto ERROR;
        }
        MFREE(stack);
        *node = root;
        return OK;
ERROR:
        tld__template_node_free(child);
        MFREE(stack);
        tld__template_node_free(root);
        return FAIL;
}

/* Make child the last child of parent. The parent owns it afterwards; if that
   fails, the caller still does. */
int tld__template_node_add_child(tld__template_node *parent, tld__template_node *child)
{
        if(parent->n == parent->n_alloc){
                RUN(tld__template_node_resize(parent));
        }
        parent->children[parent->n++] = child;
        child->parent = parent;
        return OK;
ERROR:
        return FAIL;
}

/* Text up to the next {{ (or the end) as a new text node in *node. */
int tld__template_parse_text(tld__template_parser *parser, tld__template_node **node)
{
        tld__template_node *n = NULL;
        const char *start = parser->input + parser->position;
        int length = 0;
        while(start[length] != '\0' && (start[length] != '{' || start[length + 1] != '{')){
                length++;
        }
        RUN(tld__template_node_create(&n, TLD__NODE_TEXT, ""));
        RUN(tld_strbuf_append_n(n->content, start, (size_t) length));
        parser->position += length;
        *node = n;
        return OK;
ERROR:
        tld__template_node_free(n);
        return FAIL;
}

/* The {{...}} tag at the parser's position as a new node in *node. A malformed tag
   leaves its reason in parser->error. */
int tld__template_parse_tag(tld__template_parser *parser, tld__template_node **node)
{
        char tag[TLD__TEMPLATE_MAX_NAME];
        int at = parser->position;
        int i = 0;
        const char *p = parser->input + parser->position + 2;   /* after the {{ */

        while(!(p[0] == '}' && p[1] == '}')){
                if(p[0] == '\0'){
                        snprintf(parser->error, TLD__TEMPLATE_MAX_ERROR, "Tag at byte %d is not closed with }}", at);
                        return FAIL;
                }
                if(i == TLD__TEMPLATE_MAX_NAME - 1){
                        snprintf(parser->error, TLD__TEMPLATE_MAX_ERROR,
                                 "Tag at byte %d is longer than %d characters", at, TLD__TEMPLATE_MAX_NAME - 1);
                        return FAIL;
                }
                tag[i++] = *p++;
        }
        tag[i] = '\0';
        parser->position = (int) (p + 2 - parser->input);

        if(i == 0){
                snprintf(parser->error, TLD__TEMPLATE_MAX_ERROR, "Empty tag {{}} at byte %d", at);
                return FAIL;
        }
        if(strcmp(tag, "ELSE") == 0){
                return tld__template_node_create(node, TLD__NODE_ELSE, NULL);
        }
        if(strcmp(tag, "ENDIF") == 0){
                return tld__template_node_create(node, TLD__NODE_ENDIF, NULL);
        }
        if(strcmp(tag, "IF") == 0 || strcmp(tag, "IF ") == 0){
                snprintf(parser->error, TLD__TEMPLATE_MAX_ERROR, "IF without a name at byte %d", at);
                return FAIL;
        }
        if(strncmp(tag, "IF ", 3) == 0){
                return tld__template_node_create(node, TLD__NODE_IF, tag + 3);
        }
        return tld__template_node_create(node, TLD__NODE_VARIABLE, tag);
}

int tld__template_node_create(tld__template_node **node, tld__template_node_type type, const char *content)
{
        tld__template_node* n = NULL;
        MMALLOC(n, sizeof(tld__template_node));      /* zeroed: no children, no content */
        n->type = type;
        if(content){
                RUN(tld_strbuf_alloc(&n->content, 16));
                RUN(tld_strbuf_append(n->content, content));
        }
        *node = n;
        return OK;
ERROR:
        tld__template_node_free(n);
        return FAIL;
}

/* Room for more children: 4 at first, then half as many again. n_alloc changes only
   once the memory is there. */
int tld__template_node_resize(tld__template_node *node)
{
        int new_size = node->n_alloc ? node->n_alloc + node->n_alloc / 2 : 4;
        MREALLOC(node->children, sizeof(tld__template_node*) * new_size);
        node->n_alloc = new_size;
        return OK;
ERROR:
        return FAIL;
}

int tld_template_apply(tld_strbuf *txt, tld_hash *hash)
{
        tld__template_node* root = NULL;
        tld_strbuf* out = NULL;
        ASSERT(txt != NULL, "No template text");
        ASSERT(hash != NULL, "No map");

        RUN(tld__template_parse_strbuf(txt, &root));
        RUN(tld_strbuf_alloc(&out, 256));
        RUN(tld__template_process(root, hash, out));
        RUN(tld_strbuf_copy(txt, out));

        tld__template_node_free(root);
        tld_strbuf_free(out);
        return OK;
ERROR:
        tld_strbuf_free(out);
        tld__template_node_free(root);
        return FAIL;
}

int tld__template_get_vars(tld__template_node *node, tld_hash* h)
{
        if(node->type == TLD__NODE_VARIABLE || node->type == TLD__NODE_IF){
                RUN(tld_hash_add(&h, TLD_STR(node->content), TLD_STR(node->content)));
        }
        for(int i = 0; i < node->n; i++){
                RUN(tld__template_get_vars(node->children[i], h));
        }
        return OK;
ERROR:
        return FAIL;
}

int tld_template_vars(tld_strbuf *txt, tld_hash **hash)
{
        tld_hash* h = NULL;
        tld__template_node* root = NULL;
        ASSERT(txt != NULL, "No template text");
        ASSERT(hash != NULL, "No output pointer");

        RUN(tld__template_parse_strbuf(txt, &root));
        RUN(tld_hash_alloc(&h,1024));
        RUN(tld__template_get_vars(root, h));

        tld__template_node_free(root);
        *hash = h;
        return OK;
ERROR:
        tld_hash_free(h);
        tld__template_node_free(root);
        return FAIL;
}

#undef TLD__TEMPLATE_MAX_ERROR
#undef TLD__TEMPLATE_MAX_IF_DEPTH
#undef TLD__TEMPLATE_MAX_NAME
