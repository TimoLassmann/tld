/* Tests for the JSON fixes and additions (modernisation, PRD 5.5). */
#include "tld.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static int n_fail = 0;
static int n_check = 0;

#define CHECK(x) do {                                                   \
                n_check++;                                              \
                if(!(x)){                                               \
                        fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #x); \
                        n_fail++;                                       \
                }                                                       \
        } while(0)

static int parse_cstr(const char *s, tld_json_obj **out)
{
        tld_strbuf* b = NULL;
        int r;
        if(tld_strbuf_alloc(&b, 64) != OK){
                return FAIL;
        }
        tld_append(b, (char*)s);
        r = tld_json_parse(b, out);
        tld_strbuf_free(b);
        return r;
}

/* parse from an explicit byte range (may contain NUL bytes) */
static int parse_bytes(const char *s, int len, tld_json_obj **out)
{
        tld_strbuf* b = NULL;
        int r;
        if(tld_strbuf_alloc(&b, len + 16) != OK){
                return FAIL;
        }
        memcpy(b->str, s, len);
        b->len = len;
        b->str[len] = 0;
        r = tld_json_parse(b, out);
        tld_strbuf_free(b);
        return r;
}

static tld_json_val* get_val(tld_json_obj *o, const char *key)
{
        for(int i = 0; i < o->n; i++){
                if(strcmp(TLD_STR(o->key[i]), key) == 0){
                        return o->v[i];
                }
        }
        return NULL;
}

static void test_roundtrip_prompts(void)
{
        const char* prompts[] = {
                "He said \"hello\" and left",
                "back\\slash, double \\\\ and trailing \\",
                "x\\\\",
                "Line1\nLine2\r\nLine3\n",
                "col1\tcol2\t\tcol3",
                "emoji \xF0\x9F\x98\x80 rocket \xF0\x9F\x9A\x80 flag \xF0\x9F\x87\xA6\xF0\x9F\x87\xBA",
                "caf\xC3\xA9 na\xC3\xAFve \xE2\x82\xAC",
                "42",
                "true",
                "false",
                "null",
                "0.2",
                "-1e5",
                "",
                "{",
                "}",
                "[1,2]",
                ",",
                ":",
                "{\"inner\": \"json\"}",
                "control \x01\x02\x1f bell \x07 backspace \b formfeed \f",
                "slash / and </script>",
                "Return JSON: {\"answer\": \"yes\", \"why\": \"because \\\"x\\\"\"}"
        };
        int n = (int)(sizeof(prompts) / sizeof(prompts[0]));

        for(int i = 0; i < n; i++){
                tld_strbuf* b = NULL;
                tld_json_obj* o = NULL;
                tld_json_obj* p = NULL;
                const char* got = NULL;
                tld_strbuf_alloc(&b, 64);

                o = JSON_OBJ(
                        JS("model", "llama3.2"),
                        JSTR("prompt", prompts[i]),
                        JA("messages", JSON_ARR(
                                   JO(NULL, JSON_OBJ(JSTR("role", "user"),
                                                     JSTR("content", prompts[i]))),
                                   JSTR(NULL, prompts[i])
                                   ))
                        );
                CHECK(o != NULL);
                CHECK(tld_json_dump(o, b, 0) == OK);
                CHECK(parse_cstr(TLD_STR(b), &p) == OK);
                if(p){
                        CHECK(tld_json_get_str(p, "prompt", &got) == OK);
                        CHECK(got != NULL && strcmp(got, prompts[i]) == 0);
                        if(got && strcmp(got, prompts[i]) != 0){
                                fprintf(stderr, "  expected [%s]\n  got      [%s]\n", prompts[i], got);
                        }
                        got = NULL;
                        CHECK(tld_json_get_str(p, "messages[0].content", &got) == OK);
                        CHECK(got != NULL && strcmp(got, prompts[i]) == 0);
                        got = NULL;
                        CHECK(tld_json_get_str(p, "messages[1]", &got) == OK);
                        CHECK(got != NULL && strcmp(got, prompts[i]) == 0);
                        got = NULL;
                        CHECK(tld_json_get_str(p, "model", &got) == OK);
                        CHECK(got != NULL && strcmp(got, "llama3.2") == 0);
                        /* type is string, never retyped */
                        CHECK(get_val(p, "prompt")->type == TLD_JSON_STR);
                }
                tld_json_obj_free(o);
                tld_json_obj_free(p);
                tld_strbuf_free(b);
        }
}

static void test_escapes(void)
{
        tld_json_obj* o = NULL;
        const char* s = NULL;
        int64_t i = 0;

        CHECK(parse_cstr("{\"a\":\"caf\\u00e9 \\u00E9 \\ud83d\\ude00 \\/ \\b\\f\\n\\r\\t \\\\ \\\"q\\\"\"}", &o) == OK);
        CHECK(tld_json_get_str(o, "a", &s) == OK);
        CHECK(s && strcmp(s, "caf\xC3\xA9 \xC3\xA9 \xF0\x9F\x98\x80 / \b\f\n\r\t \\ \"q\"") == 0);
        tld_json_obj_free(o);
        o = NULL;

        /* "x\\" followed by another key: the old lexer took \" as escaped */
        CHECK(parse_cstr("{\"a\":\"x\\\\\",\"b\":1}", &o) == OK);
        CHECK(tld_json_get_str(o, "a", &s) == OK);
        CHECK(s && strcmp(s, "x\\") == 0);
        CHECK(tld_json_get_int(o, "b", &i) == OK && i == 1);
        tld_json_obj_free(o);
        o = NULL;

        /* lone surrogates become U+FFFD */
        CHECK(parse_cstr("{\"a\":\"\\ud83d x \\ude00\"}", &o) == OK);
        CHECK(tld_json_get_str(o, "a", &s) == OK);
        CHECK(s && strcmp(s, "\xEF\xBF\xBD x \xEF\xBF\xBD") == 0);
        tld_json_obj_free(o);
        o = NULL;

        /* escaped key */
        CHECK(parse_cstr("{\"k\\u00e9y\":\"v\"}", &o) == OK);
        CHECK(tld_json_get_str(o, "k\xC3\xA9y", &s) == OK);
        tld_json_obj_free(o);
        o = NULL;

        /* raw UTF-8 passes through */
        CHECK(parse_cstr("{\"a\":\"\xF0\x9F\x98\x80\"}", &o) == OK);
        CHECK(tld_json_get_str(o, "a", &s) == OK && strcmp(s, "\xF0\x9F\x98\x80") == 0);
        tld_json_obj_free(o);
}

static void test_whitespace(void)
{
        tld_json_obj* o = NULL;
        const char* s = NULL;
        int64_t i = 0;
        int b = 0;
        CHECK(parse_cstr("\r\n{\r\n\t\"a\" :\r\n \"x y\" ,\r\n\"b\"\r\n:\r\n12\r\n,\"c\":true\r\n}\r\n", &o) == OK);
        CHECK(tld_json_get_str(o, "a", &s) == OK && strcmp(s, "x y") == 0);
        CHECK(tld_json_get_int(o, "b", &i) == OK && i == 12);
        CHECK(tld_json_get_bool(o, "c", &b) == OK && b == 1);
        tld_json_obj_free(o);
}

static void test_quoted_typing(void)
{
        tld_json_obj* o = NULL;
        const char* keys[] = {"a", "b", "c", "d", "e", "f", "g", "h", "i"};
        const char* vals[] = {"123", "true", "truex", "{", ",", "null", "1.5", "False", "]"};
        const char* s = NULL;
        int64_t i = 0;
        double d = 0;
        int b = 0;

        CHECK(parse_cstr("{\"a\":\"123\",\"b\":\"true\",\"c\":\"truex\",\"d\":\"{\",\"e\":\",\","
                         "\"f\":\"null\",\"g\":\"1.5\",\"h\":\"False\",\"i\":\"]\"}", &o) == OK);
        CHECK(o && o->n == 9);
        for(int k = 0; k < 9 && o; k++){
                CHECK(tld_json_get_str(o, keys[k], &s) == OK && strcmp(s, vals[k]) == 0);
                CHECK(get_val(o, keys[k])->type == TLD_JSON_STR);
        }
        CHECK(tld_json_get_int(o, "a", &i) == FAIL);
        CHECK(tld_json_get_num(o, "g", &d) == FAIL);
        CHECK(tld_json_get_bool(o, "b", &b) == FAIL);
        tld_json_obj_free(o);
}

static void test_numbers(void)
{
        tld_json_obj* o = NULL;
        int64_t i = 0;
        double d = 0;
        const char* s = NULL;

        CHECK(parse_cstr("{\"big\":5123456789,\"min\":-2147483648,\"max\":2147483647,"
                         "\"over\":2147483648,\"neg\":-5123456789,\"e\":1.4e-06,\"f\":0.99,"
                         "\"z\":-0,\"huge\":99999999999999999999,\"ie\":1E3,\"frac\":2.5,"
                         "\"n\":null}", &o) == OK);
        CHECK(get_val(o, "big")->type == TLD_JSON_DBL);
        CHECK(tld_json_get_int(o, "big", &i) == OK && i == 5123456789LL);
        CHECK(tld_json_get_num(o, "big", &d) == OK && d == 5123456789.0);
        CHECK(get_val(o, "min")->type == TLD_JSON_INT);
        CHECK(tld_json_get_int(o, "min", &i) == OK && i == -2147483648LL);
        CHECK(get_val(o, "max")->type == TLD_JSON_INT);
        CHECK(tld_json_get_int(o, "max", &i) == OK && i == 2147483647LL);
        CHECK(get_val(o, "over")->type == TLD_JSON_DBL);
        CHECK(tld_json_get_int(o, "over", &i) == OK && i == 2147483648LL);
        CHECK(tld_json_get_int(o, "neg", &i) == OK && i == -5123456789LL);
        CHECK(tld_json_get_num(o, "e", &d) == OK && d == 1.4e-06);
        CHECK(tld_json_get_num(o, "f", &d) == OK && d == 0.99);
        CHECK(tld_json_get_int(o, "z", &i) == OK && i == 0);
        CHECK(tld_json_get_num(o, "huge", &d) == OK && d == 1e20);
        CHECK(tld_json_get_int(o, "huge", &i) == FAIL);
        CHECK(tld_json_get_int(o, "ie", &i) == OK && i == 1000);
        CHECK(tld_json_get_int(o, "frac", &i) == FAIL);
        CHECK(tld_json_get_num(o, "frac", &d) == OK && d == 2.5);
        CHECK(get_val(o, "n")->type == TLD_JSON_UNDEF);
        CHECK(tld_json_get_str(o, "n", &s) == FAIL);
        CHECK(tld_json_get_num(o, "n", &d) == FAIL);
        tld_json_obj_free(o);
}

static void test_writer_numbers(void)
{
        tld_strbuf* b = NULL;
        tld_json_obj* o = NULL;
        tld_json_obj* p = NULL;
        double d = 0;
        int64_t i = 0;
        int bo = -1;
        double third = 1.0 / 3.0;

        tld_strbuf_alloc(&b, 64);
        o = JSON_OBJ(
                JNUM("temperature", 0.1),
                JNUM("third", third),
                JNUM("tiny", 1.4e-300),
                JNUM("big", 1.7976931348623157e308),
                JNUM("inf", INFINITY),
                JNUM("nan", NAN),
                JINT("ns", 5123456789L),
                JINT("ctx", 4096),
                JINT("neg", -7),
                JBOOL("stream", 0),
                JBOOL("think", 5),
                JNULL("nothing"),
                JS("guess_dbl", "0.2"),
                JS("guess_int", "12"),
                JS("guess_bigint", "5123456789"),
                JS("guess_bool", "true"),
                JS("not_num", "2024-11"),
                JS("not_bool", "truex"),
                JS("empty", ""),
                JSTR("str42", "42")
                );
        CHECK(tld_json_dump(o, b, 0) == OK);
        CHECK(strstr(TLD_STR(b), "\"temperature\": 0.1,") != NULL);
        CHECK(strstr(TLD_STR(b), "\"inf\": null,") != NULL);
        CHECK(strstr(TLD_STR(b), "\"nan\": null,") != NULL);
        CHECK(strstr(TLD_STR(b), "\"ns\": 5123456789,") != NULL);
        CHECK(strstr(TLD_STR(b), "\"ctx\": 4096,") != NULL);
        CHECK(strstr(TLD_STR(b), "\"stream\": false,") != NULL);
        CHECK(strstr(TLD_STR(b), "\"think\": true,") != NULL);
        CHECK(strstr(TLD_STR(b), "\"nothing\": null,") != NULL);
        CHECK(strstr(TLD_STR(b), "\"guess_dbl\": 0.2,") != NULL);
        CHECK(strstr(TLD_STR(b), "\"guess_int\": 12,") != NULL);
        CHECK(strstr(TLD_STR(b), "\"guess_bigint\": 5123456789,") != NULL);
        CHECK(strstr(TLD_STR(b), "\"guess_bool\": true,") != NULL);
        CHECK(strstr(TLD_STR(b), "\"not_num\": \"2024-11\",") != NULL);
        CHECK(strstr(TLD_STR(b), "\"not_bool\": \"truex\",") != NULL);
        CHECK(strstr(TLD_STR(b), "\"empty\": \"\",") != NULL);
        CHECK(strstr(TLD_STR(b), "\"str42\": \"42\"") != NULL);

        CHECK(parse_cstr(TLD_STR(b), &p) == OK);
        CHECK(tld_json_get_num(p, "temperature", &d) == OK && d == 0.1);
        CHECK(tld_json_get_num(p, "third", &d) == OK && d == third);
        CHECK(tld_json_get_num(p, "tiny", &d) == OK && d == 1.4e-300);
        CHECK(tld_json_get_num(p, "big", &d) == OK && d == 1.7976931348623157e308);
        CHECK(tld_json_get_int(p, "ns", &i) == OK && i == 5123456789LL);
        CHECK(tld_json_get_int(p, "neg", &i) == OK && i == -7);
        CHECK(tld_json_get_bool(p, "stream", &bo) == OK && bo == 0);
        CHECK(tld_json_get_bool(p, "think", &bo) == OK && bo == 1);
        tld_json_obj_free(o);
        tld_json_obj_free(p);
        tld_strbuf_free(b);
}

static void test_ollama_chat(void)
{
        const char* in = "{\"model\":\"x\",\"message\":{\"role\":\"assistant\",\"content\":\"Line1\\nLine2 \\\"q\\\"\"},"
                "\"done\":true,\"total_duration\":5123456789,\"eval_count\":12}";
        tld_json_obj* o = NULL;
        tld_json_obj* m = NULL;
        const char* s = NULL;
        int64_t i = 0;
        int b = 0;
        CHECK(parse_cstr(in, &o) == OK);
        CHECK(tld_json_get_str(o, "model", &s) == OK && strcmp(s, "x") == 0);
        CHECK(tld_json_get_str(o, "message.role", &s) == OK && strcmp(s, "assistant") == 0);
        CHECK(tld_json_get_str(o, "message.content", &s) == OK && strcmp(s, "Line1\nLine2 \"q\"") == 0);
        CHECK(tld_json_get_bool(o, "done", &b) == OK && b == 1);
        CHECK(tld_json_get_int(o, "total_duration", &i) == OK && i == 5123456789LL);
        CHECK(tld_json_get_int(o, "eval_count", &i) == OK && i == 12);
        CHECK(tld_json_get_obj(o, "message", &m) == OK && m != NULL && m->n == 2);
        CHECK(tld_json_get_str(m, "content", &s) == OK);
        CHECK(tld_json_get_obj(o, "", &m) == OK && m == o);
        CHECK(tld_json_get_str(o, "message.missing", &s) == FAIL);
        CHECK(tld_json_get_str(o, "message", &s) == FAIL);
        CHECK(tld_json_get_str(o, "model.x", &s) == FAIL);
        tld_json_obj_free(o);
}

static void test_ollama_decision(void)
{
        const char* in = "{\"answers\":{\"team\":{\"type\":\"choice\",\"choice\":\"billing\","
                "\"probabilities\":{\"bug\":1.4e-06,\"billing\":0.99},\"confidence\":0.99}},"
                "\"usage\":{\"input_tokens\":197,\"output_tokens\":0}}";
        tld_json_obj* o = NULL;
        tld_json_obj* pr = NULL;
        const char* s = NULL;
        double d = 0;
        int64_t i = -1;
        CHECK(parse_cstr(in, &o) == OK);
        CHECK(tld_json_get_str(o, "answers.team.type", &s) == OK && strcmp(s, "choice") == 0);
        CHECK(tld_json_get_str(o, "answers.team.choice", &s) == OK && strcmp(s, "billing") == 0);
        CHECK(tld_json_get_num(o, "answers.team.probabilities.bug", &d) == OK && d == 1.4e-06);
        CHECK(tld_json_get_num(o, "answers.team.probabilities.billing", &d) == OK && d == 0.99);
        CHECK(tld_json_get_num(o, "answers.team.confidence", &d) == OK && d == 0.99);
        CHECK(tld_json_get_int(o, "usage.input_tokens", &i) == OK && i == 197);
        CHECK(tld_json_get_int(o, "usage.output_tokens", &i) == OK && i == 0);
        CHECK(tld_json_get_obj(o, "answers.team.probabilities", &pr) == OK && pr->n == 2);
        CHECK(tld_json_get_num(o, "answers.team.probabilities.other", &d) == FAIL);
        tld_json_obj_free(o);
}

/* Computed once into a table: compilers may fuse the multiply-adds differently at
 * different call sites (gcc on arm64), so recomputing could give another last bit. */
static double embed_tab[4][768];
static int embed_tab_done = 0;

static double embed_value(int row, int col)
{
        if(!embed_tab_done){
                for(int r = 0; r < 4; r++){
                        for(int c = 0; c < 768; c++){
                                embed_tab[r][c] = sin(r * 1.7 + c * 0.37) * 0.0712345678901 + (c % 7) * 1e-9;
                        }
                }
                embed_tab_done = 1;
        }
        return embed_tab[row][col];
}

static void test_ollama_embed(void)
{
        tld_strbuf* b = NULL;
        tld_json_obj* o = NULL;
        tld_json_arr* a = NULL;
        char num[64];
        int ok = 1;
        int64_t i = 0;
        tld_strbuf_alloc(&b, 1 << 17);
        tld_append(b, "{\"model\":\"nomic-embed-text\",\"embeddings\":[");
        for(int r = 0; r < 4; r++){
                tld_append(b, r ? ",[" : "[");
                for(int c = 0; c < 768; c++){
                        snprintf(num, sizeof(num), "%s%.17g", c ? "," : "", embed_value(r, c));
                        tld_append(b, num);
                }
                tld_append(b, "]");
        }
        tld_append(b, "],\"total_duration\":14143917,\"load_duration\":1019500,\"prompt_eval_count\":8}");

        CHECK(tld_json_parse(b, &o) == OK);
        CHECK(tld_json_get_arr(o, "embeddings", &a) == OK && a->n == 4);
        for(int r = 0; r < 4 && o; r++){
                tld_json_arr* row = NULL;
                char path[64];
                snprintf(path, sizeof(path), "embeddings[%d]", r);
                CHECK(tld_json_get_arr(o, path, &row) == OK && row->n == 768);
                for(int c = 0; c < 768; c++){
                        double d = 0;
                        snprintf(path, sizeof(path), "embeddings[%d][%d]", r, c);
                        if(tld_json_get_num(o, path, &d) != OK || d != embed_value(r, c)){
                                ok = 0;
                        }
                }
        }
        CHECK(ok);
        CHECK(tld_json_get_num(o, "embeddings[4][0]", NULL) == FAIL);
        CHECK(tld_json_get_num(o, "embeddings[0][768]", NULL) == FAIL);
        CHECK(tld_json_get_int(o, "prompt_eval_count", &i) == OK && i == 8);

        /* dump and re-parse keeps every float exactly */
        tld_strbuf_clear(b);
        CHECK(tld_json_dump(o, b, 0) == OK);
        tld_json_obj_free(o);
        o = NULL;
        CHECK(tld_json_parse(b, &o) == OK);
        ok = 1;
        for(int r = 0; r < 4 && o; r++){
                for(int c = 0; c < 768; c++){
                        double d = 0;
                        char path[64];
                        snprintf(path, sizeof(path), "embeddings[%d][%d]", r, c);
                        if(tld_json_get_num(o, path, &d) != OK || d != embed_value(r, c)){
                                ok = 0;
                        }
                }
        }
        CHECK(ok);
        tld_json_obj_free(o);
        tld_strbuf_free(b);
}

static void test_ollama_lists(void)
{
        const char* tags = "{\"models\":["
                "{\"name\":\"llama3.2:latest\",\"model\":\"llama3.2:latest\",\"modified_at\":\"2025-05-04T17:37:44.706015396-07:00\","
                "\"size\":2019393189,\"digest\":\"a80c4f17acd55265feec403c7aef86be0c25983ab279d83f3bcd3abbcb5b8b72\","
                "\"details\":{\"parent_model\":\"\",\"format\":\"gguf\",\"family\":\"llama\",\"families\":[\"llama\"],"
                "\"parameter_size\":\"3.2B\",\"quantization_level\":\"Q4_K_M\"}},"
                "{\"name\":\"nomic-embed-text:latest\",\"model\":\"nomic-embed-text:latest\",\"modified_at\":\"2025-01-02T10:00:00Z\","
                "\"size\":274302450,\"digest\":\"0a109f422b47e3a30ba2b10eca18548e944e8a23073ee3f3e947efcf3c45e59f\","
                "\"details\":{\"parent_model\":\"\",\"format\":\"gguf\",\"family\":\"nomic-bert\",\"families\":[\"nomic-bert\"],"
                "\"parameter_size\":\"137M\",\"quantization_level\":\"F16\"}},"
                "{\"name\":\"gemma3:27b\",\"model\":\"gemma3:27b\",\"modified_at\":\"2025-03-12T08:00:00Z\","
                "\"size\":17396936887,\"digest\":\"abc\","
                "\"details\":{\"parent_model\":\"\",\"format\":\"gguf\",\"family\":\"gemma3\",\"families\":null,"
                "\"parameter_size\":\"27.4B\",\"quantization_level\":\"Q4_K_M\"}}"
                "]}";
        const char* ps = "{\"models\":[{\"name\":\"mistral:latest\",\"model\":\"mistral:latest\",\"size\":5137025024,"
                "\"digest\":\"2ae6f6dd7a3dd734790bbbf58b8909a606e0e7e97e94b7604e0aa7ae4490e6d8\","
                "\"details\":{\"parent_model\":\"\",\"format\":\"gguf\",\"family\":\"llama\",\"families\":[\"llama\"],"
                "\"parameter_size\":\"7.2B\",\"quantization_level\":\"Q4_0\"},"
                "\"expires_at\":\"2024-06-04T14:38:31.83753-07:00\",\"size_vram\":5137025024,\"context_length\":4096}]}";
        tld_json_obj* o = NULL;
        tld_json_arr* a = NULL;
        const char* s = NULL;
        int64_t i = 0;

        CHECK(parse_cstr(tags, &o) == OK);
        CHECK(tld_json_get_arr(o, "models", &a) == OK && a->n == 3);
        CHECK(tld_json_get_str(o, "models[0].name", &s) == OK && strcmp(s, "llama3.2:latest") == 0);
        CHECK(tld_json_get_str(o, "models[2].name", &s) == OK && strcmp(s, "gemma3:27b") == 0);
        CHECK(tld_json_get_str(o, "models[1].details.family", &s) == OK && strcmp(s, "nomic-bert") == 0);
        CHECK(tld_json_get_str(o, "models[0].details.families[0]", &s) == OK && strcmp(s, "llama") == 0);
        CHECK(tld_json_get_str(o, "models[0].details.parent_model", &s) == OK && strcmp(s, "") == 0);
        CHECK(tld_json_get_arr(o, "models[2].details.families", &a) == FAIL);
        CHECK(tld_json_get_int(o, "models[0].size", &i) == OK && i == 2019393189LL);
        CHECK(tld_json_get_int(o, "models[2].size", &i) == OK && i == 17396936887LL);
        CHECK(tld_json_get_str(o, "models[3].name", &s) == FAIL);
        tld_json_obj_free(o);
        o = NULL;

        CHECK(parse_cstr(ps, &o) == OK);
        CHECK(tld_json_get_str(o, "models[0].name", &s) == OK && strcmp(s, "mistral:latest") == 0);
        CHECK(tld_json_get_int(o, "models[0].size_vram", &i) == OK && i == 5137025024LL);
        CHECK(tld_json_get_int(o, "models[0].context_length", &i) == OK && i == 4096);
        CHECK(tld_json_get_str(o, "models[0].expires_at", &s) == OK && strcmp(s, "2024-06-04T14:38:31.83753-07:00") == 0);
        tld_json_obj_free(o);
        o = NULL;

        CHECK(parse_cstr("{\"models\":[]}", &o) == OK);
        CHECK(tld_json_get_arr(o, "models", &a) == OK && a->n == 0);
        CHECK(tld_json_get_str(o, "models[0].name", &s) == FAIL);
        tld_json_obj_free(o);
}

static void test_top_level_array(void)
{
        tld_json_obj* o = NULL;
        tld_json_arr* a = NULL;
        const char* s = NULL;
        int64_t i = 0;
        CHECK(parse_cstr(" [ {\"name\":\"a\"}, {\"name\":\"b\"}, 3 ] ", &o) == OK);
        CHECK(tld_json_get_arr(o, "Top Level Arr", &a) == OK && a->n == 3);
        CHECK(tld_json_get_str(o, "Top Level Arr[0].name", &s) == OK && strcmp(s, "a") == 0);
        CHECK(tld_json_get_str(o, "[1].name", &s) == OK && strcmp(s, "b") == 0);
        CHECK(tld_json_get_int(o, "[2]", &i) == OK && i == 3);
        CHECK(tld_json_get_int(o, "[3]", &i) == FAIL);
        tld_json_obj_free(o);
        o = NULL;
        CHECK(parse_cstr("[]", &o) == OK);
        CHECK(tld_json_get_arr(o, "Top Level Arr", &a) == OK && a->n == 0);
        tld_json_obj_free(o);
        o = NULL;
        CHECK(parse_cstr("{\"a\":1}", &o) == OK);
        CHECK(tld_json_get_int(o, "[0]", &i) == FAIL);
        tld_json_obj_free(o);
}

static void test_bad_paths(void)
{
        tld_json_obj* o = NULL;
        const char* s = NULL;
        int64_t i = 0;
        const char* bad[] = {"", ".a", "a.", "a..b", "a[", "a[x]", "a[]", "a[0", "a[-1]", "a[0]b",
                             "a[0]]", "b[0]", "a[99]", "a[0].c", "a[1].c[0]", "a[1][0]", "a[9999999999]", "zz"};
        CHECK(parse_cstr("{\"a\":[1,{\"c\":\"x\"}],\"b\":{\"c\":2}}", &o) == OK);
        for(int k = 0; k < (int)(sizeof(bad) / sizeof(bad[0])); k++){
                int r1 = tld_json_get_str(o, bad[k], &s);
                int r2 = tld_json_get_int(o, bad[k], &i);
                CHECK(r1 == FAIL);
                CHECK(r2 == FAIL);
                if(r1 != FAIL || r2 != FAIL){
                        fprintf(stderr, "  path [%s]\n", bad[k]);
                }
        }
        CHECK(tld_json_get_int(o, "a[0]", &i) == OK && i == 1);
        CHECK(tld_json_get_str(o, "a[1].c", &s) == OK && strcmp(s, "x") == 0);
        CHECK(tld_json_get_int(o, "b.c", &i) == OK && i == 2);
        CHECK(tld_json_get_str(NULL, "a", &s) == FAIL);
        CHECK(tld_json_get_str(o, NULL, &s) == FAIL);
        tld_json_obj_free(o);
}

static void test_malformed(void)
{
        const char* bad[] = {
                "", "   ", "\r\n", "{", "[", "}", "]", "{\"a\":1,\"b\":", "[1,2,", "[1,2", "{\"a\"", "{\"a\":",
                "{\"a\" 1}", "{\"a\":1,}", "[1,]", "[,1]", "{,}", "{\"a\":tru}", "{\"a\":truex}", "{\"a\":nul}",
                "{\"a\":\"\\u12\"}", "{\"a\":\"\\u12g4\"}", "{\"a\":\"\\x\"}", "{\"a\":01}", "{\"a\":1.}",
                "{\"a\":.5}", "{\"a\":-}", "{\"a\":1e}", "{\"a\":1e+}", "{\"a\":+1}", "{\"a\":1}x", "{}{}",
                "{} ]", "\"str\"", "42", "true", "null", "{a:1}", "{'a':1}", "{\"a\":\"unterminated",
                "{\"a\":\"\\", "{\"a\":\"\\\"}", "{\"a\":NaN}", "{\"a\":Infinity}", "{\"a\":\"\\u0000\"}",
                "{\"a\":[1 2]}", "{\"a\":{\"b\":1}", "[[[]]", "{\"a\":1 \"b\":2}", "{\"a\"::1}", "{1:2}",
                "{\"a\":\"\\ud83d\\u12\"}"
        };
        int n = (int)(sizeof(bad) / sizeof(bad[0]));
        for(int i = 0; i < n; i++){
                tld_json_obj* o = NULL;
                int r = parse_cstr(bad[i], &o);
                CHECK(r == FAIL);
                if(r != FAIL){
                        fprintf(stderr, "  input [%s] parsed\n", bad[i]);
                        tld_json_obj_free(o);
                }
        }

        /* embedded NUL byte inside a string */
        {
                tld_json_obj* o = NULL;
                const char s[] = "{\"a\":\"x\0y\"}";
                CHECK(parse_bytes(s, (int)sizeof(s) - 1, &o) == FAIL);
        }

        /* every proper prefix of a realistic response must fail */
        {
                const char* full = "{\"model\":\"x\",\"message\":{\"role\":\"assistant\",\"content\":\"a \\\"b\\\" \\u00e9\\n\","
                        "\"tool_calls\":[{\"function\":{\"name\":\"f\",\"arguments\":{\"x\":[1,2.5e-3,true,false,null]}}}]},"
                        "\"done\":true,\"total_duration\":5123456789}";
                int len = (int)strlen(full);
                int all_fail = 1;
                tld_json_obj* o = NULL;
                for(int l = 0; l < len; l++){
                        o = NULL;
                        if(parse_bytes(full, l, &o) != FAIL){
                                fprintf(stderr, "  prefix of length %d parsed\n", l);
                                all_fail = 0;
                                tld_json_obj_free(o);
                        }
                }
                CHECK(all_fail);
                o = NULL;
                CHECK(parse_bytes(full, len, &o) == OK);
                tld_json_obj_free(o);
        }
        /* NULL input */
        {
                tld_json_obj* o = NULL;
                CHECK(tld_json_parse(NULL, &o) == FAIL);
        }
}

static void test_deep_nesting(void)
{
        int depth = 100000;
        char* s = malloc(depth * 2 + 16);
        tld_json_obj* o = NULL;
        int p = 0;

        /* deep arrays */
        for(int i = 0; i < depth; i++){
                s[p++] = '[';
        }
        for(int i = 0; i < depth; i++){
                s[p++] = ']';
        }
        s[p] = 0;
        CHECK(parse_cstr(s, &o) == FAIL);

        /* deep objects */
        p = 0;
        for(int i = 0; i < 20000; i++){
                memcpy(s + p, "{\"a\":", 5);
                p += 5;
        }
        s[p++] = '1';
        for(int i = 0; i < 20000; i++){
                s[p++] = '}';
        }
        s[p] = 0;
        CHECK(parse_cstr(s, &o) == FAIL);

        /* 512 levels are fine, 513 are not */
        p = 0;
        for(int i = 0; i < 512; i++){
                s[p++] = '[';
        }
        for(int i = 0; i < 512; i++){
                s[p++] = ']';
        }
        s[p] = 0;
        o = NULL;
        CHECK(parse_cstr(s, &o) == OK);
        tld_json_obj_free(o);
        o = NULL;

        p = 0;
        for(int i = 0; i < 513; i++){
                s[p++] = '[';
        }
        for(int i = 0; i < 513; i++){
                s[p++] = ']';
        }
        s[p] = 0;
        CHECK(parse_cstr(s, &o) == FAIL);
        free(s);
}

static void test_long_strings(void)
{
        int sizes[] = {15, 16, 17, 255, 256, 257, 4095, 4096, 4097, 6143, 6144, 6145, 100000};
        for(int k = 0; k < (int)(sizeof(sizes) / sizeof(sizes[0])); k++){
                int n = sizes[k];
                char* s = malloc(n + 1);
                tld_strbuf* b = NULL;
                tld_json_obj* o = NULL;
                tld_json_obj* p = NULL;
                const char* got = NULL;
                for(int i = 0; i < n; i++){
                        /* mix in characters that need escaping */
                        int m = i % 11;
                        s[i] = m == 3 ? '"' : m == 7 ? '\\' : m == 9 ? '\n' : (char)('a' + (i % 26));
                }
                s[n] = 0;
                tld_strbuf_alloc(&b, 64);
                o = JSON_OBJ(JSTR("s", s), JSTR("t", "after"));
                CHECK(tld_json_dump(o, b, 0) == OK);
                CHECK(parse_cstr(TLD_STR(b), &p) == OK);
                CHECK(tld_json_get_str(p, "s", &got) == OK && strcmp(got, s) == 0);
                CHECK(tld_json_get_str(p, "t", &got) == OK && strcmp(got, "after") == 0);
                tld_json_obj_free(o);
                tld_json_obj_free(p);
                tld_strbuf_free(b);

                /* the same length of plain text, and escaped quotes at the
                   end of the old 4096 byte lexer buffer */
                {
                        char* raw = malloc(2 * n + 16);
                        int q = 0;
                        memcpy(raw, "{\"s\":\"", 6);
                        q = 6;
                        for(int i = 0; i < n; i++){
                                if(i % 2){
                                        raw[q++] = '\\';
                                        raw[q++] = '"';
                                }else{
                                        raw[q++] = 'x';
                                }
                        }
                        raw[q++] = '"';
                        raw[q++] = '}';
                        raw[q] = 0;
                        tld_strbuf_alloc(&b, q + 1);
                        tld_append(b, raw);
                        free(raw);
                }
                p = NULL;
                CHECK(tld_json_parse(b, &p) == OK);
                CHECK(tld_json_get_str(p, "s", &got) == OK && (int)strlen(got) == n);
                tld_json_obj_free(p);
                tld_strbuf_free(b);
                free(s);
        }
}

static void test_search_many(void)
{
        tld_json_obj* o = NULL;
        tld_json_obj* r = NULL;
        tld_strbuf* b = NULL;
        tld_strbuf_alloc(&b, 64);
        tld_append(b, "{\"x\":[");
        for(int i = 0; i < 40; i++){
                char tmp[64];
                snprintf(tmp, sizeof(tmp), "%s{\"k\":%d,\"o\":{\"k\":\"s%d\"}}", i ? "," : "", i, i);
                tld_append(b, tmp);
        }
        tld_append(b, "]}");
        CHECK(tld_json_parse(b, &o) == OK);
        CHECK(tld_json_search(o, "k", &r) == OK);
        CHECK(r != NULL && r->n == 80);
        tld_json_obj_free(r);
        tld_json_obj_free(o);
        tld_strbuf_free(b);
}

static void test_set_val(void)
{
        tld_json_obj* o = NULL;
        tld_json_val* v = NULL;
        const char* s = NULL;
        int64_t i = 0;
        tld_strbuf* b = NULL;
        tld_json_obj* p = NULL;

        CHECK(parse_cstr("{\"a\":\"string\",\"b\":{\"c\":[1,2,3]},\"d\":5,\"e\":[\"x\"]}", &o) == OK);

        tld_json_val_alloc(&v);
        v->type = TLD_JSON_INT;
        v->value.i_num = 7;
        CHECK(tld_json_set_val(o, "a", v) == OK);        /* STR -> INT */
        CHECK(tld_json_get_int(o, "a", &i) == OK && i == 7);
        CHECK(tld_json_set_val(o, "b", v) == OK);        /* OBJ -> INT */
        CHECK(tld_json_get_int(o, "b", &i) == OK && i == 7);
        tld_json_val_free(v);
        v = NULL;

        tld_json_val_alloc(&v);
        v->type = TLD_JSON_STR;
        tld_strbuf_alloc(&v->value.str, 16);
        tld_append(v->value.str, "now a string");
        CHECK(tld_json_set_val(o, "d", v) == OK);        /* INT -> STR */
        CHECK(tld_json_get_str(o, "d", &s) == OK && strcmp(s, "now a string") == 0);
        CHECK(tld_json_set_val(o, "e", v) == OK);        /* ARR -> STR */
        CHECK(tld_json_get_str(o, "e", &s) == OK && strcmp(s, "now a string") == 0);
        tld_json_val_free(v);
        v = NULL;

        /* set an object value from a subtree of the same document */
        CHECK(parse_cstr("{\"x\":{\"y\":{\"z\":1}},\"w\":2}", &p) == OK);
        CHECK(tld_json_set_val(p, "x", get_val(get_val(p, "x")->value.obj, "y")) == OK);
        CHECK(tld_json_get_int(p, "x.z", &i) == OK && i == 1);
        tld_json_obj_free(p);
        p = NULL;

        tld_strbuf_alloc(&b, 64);
        CHECK(tld_json_dump(o, b, 0) == OK);
        CHECK(parse_cstr(TLD_STR(b), &p) == OK);
        CHECK(tld_json_get_str(p, "e", &s) == OK && strcmp(s, "now a string") == 0);
        tld_json_obj_free(p);
        tld_json_obj_free(o);
        tld_strbuf_free(b);
}

static void test_build_request(void)
{
        /* the shape tld_ai.h sends to /api/chat */
        tld_strbuf* b = NULL;
        tld_json_obj* p = NULL;
        const char* s = NULL;
        double d = 0;
        int bo = -1;
        int64_t i = 0;
        const char* user = "Classify: \"my bill is wrong\"\n\tthanks \xF0\x9F\x99\x8F";
        tld_json_obj* o = JSON_OBJ(
                JSTR("model", "llama3.2"),
                JA("messages", JSON_ARR(
                           JO(NULL, JSON_OBJ(JSTR("role", "system"), JSTR("content", "Answer \"yes\" or \"no\"."))),
                           JO(NULL, JSON_OBJ(JSTR("role", "user"), JSTR("content", user)))
                           )),
                JBOOL("stream", 0),
                JO("options", JSON_OBJ(JNUM("temperature", 0.2), JINT("num_ctx", 8192), JINT("seed", 42))),
                JA("stop", JSON_ARR(JSTR(NULL, "\n\n"), JSTR(NULL, "</s>"), JNUM(NULL, 1.5), JINT(NULL, 3), JBOOL(NULL, 1), JNULL(NULL)))
                );
        tld_strbuf_alloc(&b, 64);
        CHECK(tld_json_dump(o, b, 0) == OK);
        CHECK(parse_cstr(TLD_STR(b), &p) == OK);
        CHECK(tld_json_get_str(p, "messages[1].content", &s) == OK && strcmp(s, user) == 0);
        CHECK(tld_json_get_str(p, "messages[0].content", &s) == OK && strcmp(s, "Answer \"yes\" or \"no\".") == 0);
        CHECK(tld_json_get_bool(p, "stream", &bo) == OK && bo == 0);
        CHECK(tld_json_get_num(p, "options.temperature", &d) == OK && d == 0.2);
        CHECK(tld_json_get_int(p, "options.num_ctx", &i) == OK && i == 8192);
        CHECK(tld_json_get_str(p, "stop[0]", &s) == OK && strcmp(s, "\n\n") == 0);
        CHECK(tld_json_get_num(p, "stop[2]", &d) == OK && d == 1.5);
        CHECK(tld_json_get_int(p, "stop[3]", &i) == OK && i == 3);
        CHECK(tld_json_get_bool(p, "stop[4]", &bo) == OK && bo == 1);
        CHECK(tld_json_get_str(p, "stop[5]", &s) == FAIL);
        CHECK(strstr(TLD_STR(b), "\"num_ctx\": 8192") != NULL);
        tld_json_obj_free(p);
        tld_json_obj_free(o);
        tld_strbuf_free(b);
}

int main(void)
{
        test_roundtrip_prompts();
        test_escapes();
        test_whitespace();
        test_quoted_typing();
        test_numbers();
        test_writer_numbers();
        test_ollama_chat();
        test_ollama_decision();
        test_ollama_embed();
        test_ollama_lists();
        test_top_level_array();
        test_bad_paths();
        test_malformed();
        test_deep_nesting();
        test_long_strings();
        test_search_many();
        test_set_val();
        test_build_request();

        fprintf(stdout, "unit_fix_json: %d checks, %d failed\n", n_check, n_fail);
        return n_fail ? EXIT_FAILURE : EXIT_SUCCESS;
}
