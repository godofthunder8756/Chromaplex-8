/*
 * cx8_pickup.c — Chromaplex 8 PickUp-to-Lua Transpiler
 *
 * Transforms PickUp language source into equivalent Lua 5.4 code.
 * PickUp is intentionally Lua-like, so most code passes through
 * unchanged. We only transform the syntactic differences:
 *
 *   1. [...] array literals    → {...}
 *   2. continue               → goto __cx8_continue_N__
 *   3. throw expr             → error(expr)
 *   4. try/catch/end          → pcall wrapper
 *   5. import "module"        → (stripped — CX8 APIs are global)
 *   6. 0-based arr[0]         → arr[0+1] (runtime shim)
 *
 * For array indexing, we inject a small runtime shim that makes
 * tables act 1-based internally while appearing 0-based in PickUp.
 */

#include "cx8_pickup.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ─── Output buffer ────────────────────────────────────────── */

typedef struct {
    char   *data;
    size_t  len;
    size_t  cap;
} buf_t;

static void buf_init(buf_t *b, size_t initial)
{
    b->cap  = initial;
    b->data = (char *)malloc(initial);
    b->len  = 0;
    if (b->data) b->data[0] = '\0';
}

static void buf_ensure(buf_t *b, size_t extra)
{
    if (b->len + extra + 1 > b->cap) {
        while (b->len + extra + 1 > b->cap)
            b->cap *= 2;
        b->data = (char *)realloc(b->data, b->cap);
    }
}

static void buf_append(buf_t *b, const char *s, size_t n)
{
    if (!s || n == 0) return;
    buf_ensure(b, n);
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
}

static void buf_appendz(buf_t *b, const char *s)
{
    buf_append(b, s, strlen(s));
}

/* ─── Token/keyword detection helpers ──────────────────────── */

static bool is_word_char(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

/* Check if position p in source is at the start of keyword `kw` */
static bool at_keyword(const char *p, const char *kw, const char *src_start)
{
    size_t kwlen = strlen(kw);
    if (strncmp(p, kw, kwlen) != 0) return false;
    /* Must not be preceded by a word char */
    if (p > src_start && is_word_char(*(p - 1))) return false;
    /* Must not be followed by a word char */
    if (is_word_char(p[kwlen])) return false;
    return true;
}

/* Skip a string literal, returning pointer past closing quote */
static const char *skip_string(const char *p)
{
    char quote = *p++;
    while (*p && *p != quote) {
        if (*p == '\\') p++;  /* skip escaped char */
        p++;
    }
    if (*p == quote) p++;
    return p;
}

/* Skip a Lua-style comment (-- ... or --[[ ... ]]) */
static const char *skip_comment(const char *p)
{
    p += 2;  /* skip -- */
    if (*p == '[' && *(p + 1) == '[') {
        /* Long comment */
        const char *end = strstr(p, "]]");
        return end ? end + 2 : p + strlen(p);
    }
    /* Line comment */
    while (*p && *p != '\n') p++;
    return p;
}

/* ─── Transpiler ───────────────────────────────────────────── */

/*
 * The PickUp runtime shim provides 0-based indexing.
 * We prepend this to every transpiled file.
 */
static const char *PICKUP_RUNTIME_SHIM =
    "-- [PickUp Runtime: 0-based indexing shim]\n"
    "local __pu_arr = function(...) "
    "local t = {...}; "
    "local mt = {__index = function(s,k) "
    "if type(k)=='number' then return rawget(s,k+1) end "
    "return rawget(s,k) end, "
    "__newindex = function(s,k,v) "
    "if type(k)=='number' then rawset(s,k+1,v) else rawset(s,k,v) end "
    "end}; "
    "return setmetatable(t, mt) end\n"
    "\n";

static int s_continue_id = 0;  /* unique label counter */

char *cx8_pickup_to_lua(const char *source, size_t source_len, const char **err_msg)
{
    if (!source || source_len == 0) {
        if (err_msg) *err_msg = "Empty source";
        return NULL;
    }

    buf_t out;
    buf_init(&out, source_len * 2 + 1024);

    /* Inject runtime shim */
    buf_appendz(&out, PICKUP_RUNTIME_SHIM);

    const char *p   = source;
    const char *end = source + source_len;

    /* Track loop nesting for continue support */
    int loop_stack[64];
    int loop_depth = 0;

    while (p < end) {
        /* ── Skip strings ─────────────────────────────── */
        if (*p == '"' || *p == '\'') {
            const char *start = p;
            p = skip_string(p);
            buf_append(&out, start, (size_t)(p - start));
            continue;
        }

        /* ── Skip long strings [[...]] ────────────────── */
        if (*p == '[' && *(p + 1) == '[') {
            const char *close = strstr(p, "]]");
            if (close) {
                close += 2;
                buf_append(&out, p, (size_t)(close - p));
                p = close;
                continue;
            }
        }

        /* ── Comments ─────────────────────────────────── */
        if (*p == '-' && *(p + 1) == '-') {
            const char *start = p;
            p = skip_comment(p);
            buf_append(&out, start, (size_t)(p - start));
            continue;
        }

        /* ── Array literals: [...] → __pu_arr(...) ────── */
        if (*p == '[') {
            /* Check if this is an array literal, not a table index.
             * Array literal if preceded by: =, (, ,, return, {, [ or line start */
            bool is_literal = false;
            const char *back = p - 1;
            while (back >= source && (*back == ' ' || *back == '\t')) back--;

            if (back < source) {
                is_literal = true;
            } else {
                char prev = *back;
                is_literal = (prev == '=' || prev == '(' || prev == ',' ||
                             prev == '[' || prev == '{' || prev == '\n' ||
                             prev == ';');
                /* Also after 'return' keyword */
                if (!is_literal && back >= source + 5) {
                    if (strncmp(back - 5, "return", 6) == 0)
                        is_literal = true;
                }
            }

            if (is_literal) {
                /* Find matching ] and replace with __pu_arr(...) */
                buf_appendz(&out, "__pu_arr(");
                p++;  /* skip [ */
                int depth = 1;
                while (p < end && depth > 0) {
                    if (*p == '[') depth++;
                    else if (*p == ']') {
                        depth--;
                        if (depth == 0) break;
                    }
                    else if (*p == '"' || *p == '\'') {
                        const char *start = p;
                        p = skip_string(p);
                        buf_append(&out, start, (size_t)(p - start));
                        continue;
                    }
                    buf_append(&out, p, 1);
                    p++;
                }
                buf_appendz(&out, ")");
                if (*p == ']') p++;  /* skip closing ] */
                continue;
            }
        }

        /* ── import "module" → stripped ───────────────── */
        if (at_keyword(p, "import", source)) {
            const char *start = p;
            p += 6;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '"' || *p == '\'') {
                p = skip_string(p);
                /* Emit a comment so line numbers stay aligned */
                buf_appendz(&out, "-- (import removed by PickUp transpiler)");
                continue;
            }
            /* Not a string import, emit as-is */
            p = start;
        }

        /* ── throw expr → error(expr) ────────────────── */
        if (at_keyword(p, "throw", source)) {
            buf_appendz(&out, "error(");
            p += 5;
            /* Read until end of line or 'end' keyword */
            while (*p == ' ' || *p == '\t') p++;
            const char *expr_start = p;
            while (p < end && *p != '\n' && *p != '\r') p++;
            /* Trim trailing whitespace from expression */
            const char *expr_end = p;
            while (expr_end > expr_start &&
                   (*(expr_end-1) == ' ' || *(expr_end-1) == '\t'))
                expr_end--;
            buf_append(&out, expr_start, (size_t)(expr_end - expr_start));
            buf_appendz(&out, ")");
            continue;
        }

        /* ── try ... catch e ... end → pcall pattern ──── */
        if (at_keyword(p, "try", source)) {
            buf_appendz(&out, "local __pu_ok__, __pu_err__ = pcall(function()");
            p += 3;
            continue;
        }

        if (at_keyword(p, "catch", source)) {
            p += 5;
            /* Get the error variable name */
            while (*p == ' ' || *p == '\t') p++;
            char err_var[64] = "__pu_err__";
            if (is_word_char(*p)) {
                int i = 0;
                while (is_word_char(*p) && i < 62)
                    err_var[i++] = *p++;
                err_var[i] = '\0';
            }
            buf_appendz(&out, "end)\nif not __pu_ok__ then\nlocal ");
            buf_appendz(&out, err_var);
            buf_appendz(&out, " = __pu_err__");
            continue;
        }

        /* ── continue → goto __cx8_continue_N__ ──────── */
        if (at_keyword(p, "continue", source)) {
            if (loop_depth > 0) {
                char label[64];
                snprintf(label, sizeof(label), "goto __cx8_continue_%d__",
                         loop_stack[loop_depth - 1]);
                buf_appendz(&out, label);
            } else {
                buf_appendz(&out, "goto __cx8_continue_0__"); /* fallback */
            }
            p += 8;
            continue;
        }

        /* ── Track loop entry (while/for) ────────────── */
        if (at_keyword(p, "while", source) || at_keyword(p, "for", source)) {
            s_continue_id++;
            if (loop_depth < 64) {
                loop_stack[loop_depth] = s_continue_id;
            }
            loop_depth++;

            /* Emit keyword as-is */
            if (*p == 'w') {
                buf_appendz(&out, "while");
                p += 5;
            } else {
                buf_appendz(&out, "for");
                p += 3;
            }
            continue;
        }

        /* ── 'end' keyword — inject continue label if closing a loop ── */
        if (at_keyword(p, "end", source)) {
            if (loop_depth > 0) {
                loop_depth--;
                int label_id = loop_stack[loop_depth];
                char label[64];
                snprintf(label, sizeof(label),
                         "::__cx8_continue_%d__::\nend", label_id);
                buf_appendz(&out, label);
            } else {
                buf_appendz(&out, "end");
            }
            p += 3;
            continue;
        }

        /* ── Default: pass through ────────────────────── */
        buf_append(&out, p, 1);
        p++;
    }

    printf("[CX8-PICKUP] Transpiled %zu bytes PickUp → %zu bytes Lua\n",
           source_len, out.len);

    return out.data;
}

bool cx8_pickup_is_pickup_file(const char *filename)
{
    if (!filename) return false;
    size_t len = strlen(filename);

    /* Check for .up extension */
    if (len > 3 && strcmp(filename + len - 3, ".up") == 0)
        return true;

    /* Check for .pickup extension */
    if (len > 7 && strcmp(filename + len - 7, ".pickup") == 0)
        return true;

    return false;
}
