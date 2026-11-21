/*
 * Sigma runtime.
 *
 * This file is embedded into the `sig` compiler and pasted at the top of
 * every C file it generates. Everything is `static`, so the C compiler only
 * keeps what a program actually uses.
 *
 * Values: a 16-byte tagged union (SigValue). Variables the compiler can prove
 * are always numbers never touch this type at all - they become plain C
 * doubles - which is where most of Sigma's speed comes from.
 *
 * Memory: objects are bump-allocated and never freed (there is no GC yet).
 * That's fine for scripts, tools and benchmarks; see README "Limitations".
 */
#ifndef SIGMA_RUNTIME_H
#define SIGMA_RUNTIME_H

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define SIG_LIKELY(x)   __builtin_expect(!!(x), 1)
#define SIG_UNLIKELY(x) __builtin_expect(!!(x), 0)

/* ------------------------------------------------------------------ */
/* Core types                                                           */
/* ------------------------------------------------------------------ */

typedef enum { T_NIL = 0, T_NUM, T_BOOL, T_STR, T_LIST, T_MAP, T_FN } SigType;

typedef struct SigStr SigStr;
typedef struct SigList SigList;
typedef struct SigMap SigMap;
typedef struct SigFn SigFn;

typedef struct SigValue {
    SigType t;
    union {
        double n;
        bool b;
        SigStr* s;
        SigList* l;
        SigMap* m;
        SigFn* f;
    } u;
} SigValue;

typedef SigValue (*SigFnPtr)(SigFn* self, int argc, SigValue* argv, int line);

/* Strings are immutable views (buf, len) over an append-only buffer. When a
 * string is the last thing written into its buffer, `a + b` can append in
 * place, so building a string in a loop is amortized O(n) instead of O(n^2). */
typedef struct { size_t cap, used; char data[]; } SigBuf;
struct SigStr { SigBuf* buf; size_t len; uint64_t hash; };
struct SigList { SigValue* items; int64_t len, cap; };
typedef struct { SigStr* key; SigValue val; } SigEntry;
/* Insertion-ordered hash map with string keys. */
struct SigMap { SigEntry* entries; int64_t len, cap, count; int64_t* slots; int64_t nslots; };
struct SigFn { SigFnPtr fn; int arity; int ncaps; const char* name; SigValue caps[]; };

typedef struct SigTry { jmp_buf jb; struct SigTry* prev; } SigTry;

static const char** sig_files;
static int sig_argc;
static char** sig_argv;
static SigTry* sig_try_top;
static SigValue sig_err_val;

/* `line` arguments encode (file index << 20) | line number. */
#define SIG_LINE_NO(l) ((l) & 0xFFFFF)
#define SIG_FILE_NO(l) ((l) >> 20)

static inline SigValue sig_nil(void) { SigValue v; v.t = T_NIL; v.u.n = 0; return v; }
static inline SigValue sig_num(double n) { SigValue v; v.t = T_NUM; v.u.n = n; return v; }
static inline SigValue sig_bool(bool b) { SigValue v; v.t = T_BOOL; v.u.n = 0; v.u.b = b; return v; }
static inline SigValue sig_strv(SigStr* s) { SigValue v; v.t = T_STR; v.u.s = s; return v; }
static inline SigValue sig_listv(SigList* l) { SigValue v; v.t = T_LIST; v.u.l = l; return v; }
static inline SigValue sig_mapv(SigMap* m) { SigValue v; v.t = T_MAP; v.u.m = m; return v; }
static inline SigValue sig_fnv(SigFn* f) { SigValue v; v.t = T_FN; v.u.f = f; return v; }

static void sig_fail(int line, const char* fmt, ...) __attribute__((noreturn, cold, format(printf, 2, 3)));
static void sig_throw(SigValue v, int line) __attribute__((noreturn, cold));
static SigStr* sig_to_str(SigValue v);
static SigStr* sig_repr(SigValue v);
static bool sig_eq(SigValue a, SigValue b);
static SigValue sig_call(SigValue f, int argc, SigValue* argv, int line);

/* ------------------------------------------------------------------ */
/* Memory                                                               */
/* ------------------------------------------------------------------ */

static void sig_oom(void) {
    fputs("error: out of memory\n", stderr);
    exit(1);
}

static char* sig_arena_ptr;
static size_t sig_arena_left;

static inline void* sig_alloc(size_t n) {
    n = (n + 7) & ~(size_t)7;
    if (SIG_UNLIKELY(n > sig_arena_left)) {
        if (n > (64u << 10)) {
            void* big = malloc(n);
            if (!big) sig_oom();
            return big;
        }
        size_t chunk = (size_t)1 << 20;
        sig_arena_ptr = (char*)malloc(chunk);
        if (!sig_arena_ptr) sig_oom();
        sig_arena_left = chunk;
    }
    void* p = sig_arena_ptr;
    sig_arena_ptr += n;
    sig_arena_left -= n;
    return p;
}

static void* sig_xrealloc(void* p, size_t n) {
    p = realloc(p, n ? n : 1);
    if (!p) sig_oom();
    return p;
}

/* ------------------------------------------------------------------ */
/* Strings                                                              */
/* ------------------------------------------------------------------ */

static SigStr* sig_str_wrap(SigBuf* b, size_t len) {
    SigStr* s = (SigStr*)sig_alloc(sizeof(SigStr));
    s->buf = b;
    s->len = len;
    s->hash = 0;
    return s;
}

/* A new string of length `len` with room for `cap` bytes, header and buffer
 * in one allocation. The caller fills s->buf->data[0..len). */
static SigStr* sig_str_make(size_t len, size_t cap) {
    SigStr* s = (SigStr*)sig_alloc(sizeof(SigStr) + sizeof(SigBuf) + cap + 1);
    SigBuf* b = (SigBuf*)(s + 1);
    b->cap = cap;
    b->used = len;
    b->data[len] = 0;
    s->buf = b;
    s->len = len;
    s->hash = 0;
    return s;
}

static SigStr* sig_str_new(const char* d, size_t n) {
    SigStr* s = sig_str_make(n, n);
    if (n) memcpy(s->buf->data, d, n);
    return s;
}

static SigValue sig_str_lit(const char* d, size_t n) { return sig_strv(sig_str_new(d, n)); }
static SigValue sig_cstrv(const char* s) { return sig_strv(sig_str_new(s, strlen(s))); }
static inline const char* sig_sd(const SigStr* s) { return s->buf->data; }

/* NUL-terminated view, for C library calls. */
static const char* sig_cstr(SigStr* s) {
    if (s->buf->used == s->len) {
        s->buf->data[s->len] = 0;
        return s->buf->data;
    }
    char* p = (char*)sig_alloc(s->len + 1);
    memcpy(p, s->buf->data, s->len);
    p[s->len] = 0;
    return p;
}

static SigStr* sig_str_cat(SigStr* a, SigStr* b) {
    if (b->len == 0) return a;
    if (a->len == 0) return b;
    size_t n = a->len + b->len;
    SigBuf* buf = a->buf;
    if (buf->used == a->len && buf->cap >= n) {
        memcpy(buf->data + a->len, sig_sd(b), b->len); /* append in place */
        buf->used = n;
        return sig_str_wrap(buf, n);
    }
    /* Leave room to grow only once a string gets long: small temporaries
     * stay small, while strings built up in a loop get amortized appends. */
    SigStr* s = sig_str_make(n, n < 64 ? n : n + (n >> 1));
    memcpy(s->buf->data, sig_sd(a), a->len);
    memcpy(s->buf->data + a->len, sig_sd(b), b->len);
    return s;
}

static uint64_t sig_str_hash(SigStr* s) {
    if (s->hash) return s->hash;
    uint64_t h = 1469598103934665603ULL;
    const unsigned char* p = (const unsigned char*)sig_sd(s);
    for (size_t i = 0; i < s->len; i++) {
        h ^= p[i];
        h *= 1099511628211ULL;
    }
    h ^= h >> 32;
    if (!h) h = 1;
    s->hash = h;
    return h;
}

static inline bool sig_str_eq(SigStr* a, SigStr* b) {
    return a == b || (a->len == b->len && memcmp(sig_sd(a), sig_sd(b), a->len) == 0);
}

static int64_t sig_memfind(const char* h, size_t hn, const char* n, size_t nn, size_t from) {
    if (nn == 0) return from <= hn ? (int64_t)from : -1;
    if (nn > hn) return -1;
    for (size_t i = from; i + nn <= hn; i++)
        if (h[i] == n[0] && memcmp(h + i, n, nn) == 0) return (int64_t)i;
    return -1;
}

/* Growable scratch buffer used for formatting. */
typedef struct { char* d; size_t len, cap; } SigSB;

static void sig_sb_put(SigSB* b, const char* s, size_t n) {
    if (b->len + n + 1 > b->cap) {
        size_t c = b->cap ? b->cap * 2 : 64;
        while (c < b->len + n + 1) c *= 2;
        b->d = (char*)sig_xrealloc(b->d, c);
        b->cap = c;
    }
    memcpy(b->d + b->len, s, n);
    b->len += n;
}
static void sig_sb_puts(SigSB* b, const char* s) { sig_sb_put(b, s, strlen(s)); }
static void sig_sb_putc(SigSB* b, char c) { sig_sb_put(b, &c, 1); }
static SigStr* sig_sb_finish(SigSB* b) {
    SigStr* s = sig_str_new(b->d ? b->d : "", b->len);
    free(b->d);
    return s;
}

/* ------------------------------------------------------------------ */
/* Formatting                                                           */
/* ------------------------------------------------------------------ */

static int sig_fmt_int(char* out, int64_t v) {
    char tmp[24];
    int n = 0, k = 0;
    uint64_t u = v < 0 ? (uint64_t)(-(v + 1)) + 1 : (uint64_t)v;
    do { tmp[n++] = (char)('0' + u % 10); u /= 10; } while (u);
    if (v < 0) out[k++] = '-';
    while (n) out[k++] = tmp[--n];
    out[k] = 0;
    return k;
}

static int sig_fmt_num(char* out, double n) {
    if (n == 0) { out[0] = '0'; out[1] = 0; return 1; }
    if (isnan(n)) return sprintf(out, "nan");
    if (isinf(n)) return sprintf(out, n > 0 ? "inf" : "-inf");
    if (fabs(n) <= 9007199254740992.0 && n == floor(n)) return sig_fmt_int(out, (int64_t)n);
    int k = 0;
    for (int prec = 15; prec <= 17; prec++) { /* shortest form that reads back exactly */
        k = sprintf(out, "%.*g", prec, n);
        if (strtod(out, NULL) == n) break;
    }
    return k;
}

static const char* sig_tname(SigType t) {
    static const char* names[] = {"nil", "num", "bool", "str", "list", "map", "fn"};
    return names[t];
}

static bool sig_is_ident(SigStr* s) {
    const char* d = sig_sd(s);
    if (s->len == 0 || !(isalpha((unsigned char)d[0]) || d[0] == '_')) return false;
    for (size_t i = 1; i < s->len; i++)
        if (!(isalnum((unsigned char)d[i]) || d[i] == '_')) return false;
    return true;
}

static void sig_write_quoted(SigSB* b, SigStr* s) {
    const char* d = sig_sd(s);
    sig_sb_putc(b, '"');
    for (size_t i = 0; i < s->len; i++) {
        char c = d[i];
        if (c == '"') sig_sb_puts(b, "\\\"");
        else if (c == '\\') sig_sb_puts(b, "\\\\");
        else if (c == '\n') sig_sb_puts(b, "\\n");
        else if (c == '\t') sig_sb_puts(b, "\\t");
        else sig_sb_putc(b, c);
    }
    sig_sb_putc(b, '"');
}

static void sig_write(SigSB* b, SigValue v, bool quote, int depth) {
    char tmp[40];
    switch (v.t) {
    case T_NIL: sig_sb_puts(b, "nil"); break;
    case T_NUM: sig_sb_put(b, tmp, (size_t)sig_fmt_num(tmp, v.u.n)); break;
    case T_BOOL: sig_sb_puts(b, v.u.b ? "true" : "false"); break;
    case T_STR:
        if (quote) sig_write_quoted(b, v.u.s);
        else sig_sb_put(b, sig_sd(v.u.s), v.u.s->len);
        break;
    case T_LIST:
        if (depth > 20) { sig_sb_puts(b, "[...]"); break; }
        sig_sb_putc(b, '[');
        for (int64_t i = 0; i < v.u.l->len; i++) {
            if (i) sig_sb_puts(b, ", ");
            sig_write(b, v.u.l->items[i], true, depth + 1);
        }
        sig_sb_putc(b, ']');
        break;
    case T_MAP: {
        if (depth > 20) { sig_sb_puts(b, "{...}"); break; }
        if (v.u.m->count == 0) { sig_sb_puts(b, "{}"); break; }
        sig_sb_putc(b, '{');
        bool first = true;
        for (int64_t i = 0; i < v.u.m->len; i++) {
            SigEntry* e = &v.u.m->entries[i];
            if (!e->key) continue;
            if (!first) sig_sb_puts(b, ", ");
            first = false;
            if (sig_is_ident(e->key)) sig_sb_put(b, sig_sd(e->key), e->key->len);
            else sig_write_quoted(b, e->key);
            sig_sb_puts(b, ": ");
            sig_write(b, e->val, true, depth + 1);
        }
        sig_sb_putc(b, '}');
        break;
    }
    case T_FN:
        sig_sb_puts(b, "<fn ");
        sig_sb_puts(b, v.u.f->name);
        sig_sb_putc(b, '>');
        break;
    }
}

static SigStr* sig_to_str(SigValue v) {
    if (v.t == T_STR) return v.u.s;
    if (v.t == T_NUM) {
        char tmp[40];
        return sig_str_new(tmp, (size_t)sig_fmt_num(tmp, v.u.n));
    }
    SigSB b = {0};
    sig_write(&b, v, false, 0);
    return sig_sb_finish(&b);
}

static SigStr* sig_repr(SigValue v) {
    SigSB b = {0};
    sig_write(&b, v, true, 0);
    return sig_sb_finish(&b);
}

/* Short printable form of a value for error messages. */
static const char* sig_short(SigValue v) {
    SigStr* r = sig_repr(v);
    if (r->len <= 40) return sig_cstr(r);
    char* p = (char*)sig_alloc(44);
    memcpy(p, sig_sd(r), 37);
    memcpy(p + 37, "...", 4);
    return p;
}

/* ------------------------------------------------------------------ */
/* Errors                                                               */
/* ------------------------------------------------------------------ */

static void sig_throw(SigValue v, int line) {
    if (sig_try_top) {
        sig_err_val = v;
        longjmp(sig_try_top->jb, 1);
    }
    fflush(stdout);
    SigStr* m = sig_to_str(v);
    fprintf(stderr, "error: %.*s\n", (int)m->len, sig_sd(m));
    if (line > 0 && sig_files)
        fprintf(stderr, "  --> %s:%d\n", sig_files[SIG_FILE_NO(line)], SIG_LINE_NO(line));
    exit(1);
}

static void sig_fail(int line, const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n < 0) n = 0;
    if (n >= (int)sizeof buf) n = (int)sizeof buf - 1;
    sig_throw(sig_str_lit(buf, (size_t)n), line);
}

static void sig_type_err(int line, const char* what, const char* need, SigValue v)
    __attribute__((noreturn, cold));
static void sig_type_err(int line, const char* what, const char* need, SigValue v) {
    sig_fail(line, "%s: expected a %s, got %s %s", what, need, sig_tname(v.t), sig_short(v));
}

static inline double sig_as_num(SigValue v, int line) {
    if (SIG_LIKELY(v.t == T_NUM)) return v.u.n;
    sig_type_err(line, "can't do math with this", "num", v);
}

static inline SigStr* sig_as_str(SigValue v, int line, const char* what) {
    if (SIG_LIKELY(v.t == T_STR)) return v.u.s;
    sig_type_err(line, what, "str", v);
}

static inline bool sig_truthy(SigValue v) {
    switch (v.t) {
    case T_NIL: return false;
    case T_BOOL: return v.u.b;
    case T_NUM: return v.u.n != 0;
    case T_STR: return v.u.s->len > 0;
    case T_LIST: return v.u.l->len > 0;
    case T_MAP: return v.u.m->count > 0;
    default: return true;
    }
}

/* ------------------------------------------------------------------ */
/* Lists                                                                */
/* ------------------------------------------------------------------ */

static SigList* sig_list_new(int64_t cap) {
    if (cap < 4) cap = 4;
    SigList* l = (SigList*)sig_alloc(sizeof(SigList));
    l->items = (SigValue*)sig_xrealloc(NULL, (size_t)cap * sizeof(SigValue));
    l->len = 0;
    l->cap = cap;
    return l;
}

static void sig_list_grow(SigList* l, int64_t need) {
    int64_t c = l->cap * 2;
    if (c < need) c = need;
    l->items = (SigValue*)sig_xrealloc(l->items, (size_t)c * sizeof(SigValue));
    l->cap = c;
}

static inline void sig_list_push(SigList* l, SigValue v) {
    if (SIG_UNLIKELY(l->len == l->cap)) sig_list_grow(l, l->len + 1);
    l->items[l->len++] = v;
}

static SigValue sig_list_of(int n, const SigValue* vs) {
    SigList* l = sig_list_new(n);
    if (n) memcpy(l->items, vs, (size_t)n * sizeof(SigValue));
    l->len = n;
    return sig_listv(l);
}

static SigValue sig_range(double a, double b, bool incl, int line) {
    if (a != floor(a) || b != floor(b)) sig_fail(line, "ranges need whole numbers (got %g..%g)", a, b);
    double cnt = b - a + (incl ? 1 : 0);
    if (cnt < 0) cnt = 0;
    if (cnt > 1e9) sig_fail(line, "range %g..%g is too big to turn into a list", a, b);
    int64_t n = (int64_t)cnt;
    SigList* l = sig_list_new(n);
    for (int64_t i = 0; i < n; i++) l->items[i] = sig_num(a + (double)i);
    l->len = n;
    return sig_listv(l);
}

static int64_t sig_norm_index(SigValue k, int64_t len, int line, const char* what) {
    if (k.t != T_NUM) sig_type_err(line, "bad index", "num", k);
    double d = k.u.n;
    if (d != floor(d)) sig_fail(line, "index must be a whole number, got %g", d);
    int64_t i = (int64_t)d;
    if (i < 0) i += len;
    if (i < 0 || i >= len)
        sig_fail(line, "index %lld is out of range (the %s has %lld item%s)", (long long)d, what,
                 (long long)len, len == 1 ? "" : "s");
    return i;
}

/* ------------------------------------------------------------------ */
/* Maps                                                                 */
/* ------------------------------------------------------------------ */

static SigMap* sig_map_new(void) {
    SigMap* m = (SigMap*)sig_alloc(sizeof(SigMap));
    memset(m, 0, sizeof *m);
    return m;
}

static int64_t sig_map_find(const SigMap* m, SigStr* key) {
    if (!m->nslots) return -1;
    uint64_t mask = (uint64_t)m->nslots - 1, h = sig_str_hash(key) & mask;
    for (;;) {
        int64_t e = m->slots[h];
        if (e < 0) return -1;
        SigStr* k = m->entries[e].key;
        if (k && sig_str_eq(k, key)) return e;
        h = (h + 1) & mask;
    }
}

static void sig_map_rehash(SigMap* m) {
    int64_t j = 0;
    for (int64_t i = 0; i < m->len; i++)
        if (m->entries[i].key) m->entries[j++] = m->entries[i];
    m->len = j;
    m->count = j;
    int64_t ns = 8;
    while (ns < (j + 1) * 4) ns <<= 1;
    m->slots = (int64_t*)sig_xrealloc(m->slots, (size_t)ns * sizeof(int64_t));
    m->nslots = ns;
    for (int64_t i = 0; i < ns; i++) m->slots[i] = -1;
    uint64_t mask = (uint64_t)ns - 1;
    for (int64_t i = 0; i < j; i++) {
        uint64_t h = sig_str_hash(m->entries[i].key) & mask;
        while (m->slots[h] >= 0) h = (h + 1) & mask;
        m->slots[h] = i;
    }
}

static bool sig_map_get(SigMap* m, SigStr* key, SigValue* out) {
    int64_t e = sig_map_find(m, key);
    if (e < 0) return false;
    *out = m->entries[e].val;
    return true;
}

static void sig_map_set(SigMap* m, SigStr* key, SigValue v) {
    int64_t e = sig_map_find(m, key);
    if (e >= 0) { m->entries[e].val = v; return; }
    if ((m->len + 1) * 2 > m->nslots) sig_map_rehash(m);
    if (m->len == m->cap) {
        m->cap = m->cap ? m->cap * 2 : 8;
        m->entries = (SigEntry*)sig_xrealloc(m->entries, (size_t)m->cap * sizeof(SigEntry));
    }
    e = m->len++;
    m->entries[e].key = key;
    m->entries[e].val = v;
    m->count++;
    uint64_t mask = (uint64_t)m->nslots - 1, h = sig_str_hash(key) & mask;
    while (m->slots[h] >= 0) h = (h + 1) & mask;
    m->slots[h] = e;
}

static bool sig_map_del(SigMap* m, SigStr* key) {
    int64_t e = sig_map_find(m, key);
    if (e < 0) return false;
    m->entries[e].key = NULL;
    m->count--;
    return true;
}

static SigValue sig_map_of(int n, const SigValue* keys, const SigValue* vals) {
    SigMap* m = sig_map_new();
    for (int i = 0; i < n; i++) sig_map_set(m, keys[i].u.s, vals[i]);
    return sig_mapv(m);
}

static SigStr* sig_key(SigValue k, int line) {
    if (k.t == T_STR) return k.u.s;
    if (k.t == T_NUM || k.t == T_BOOL) return sig_to_str(k);
    sig_fail(line, "map keys must be str or num, got %s", sig_tname(k.t));
}

static SigValue sig_map_at(SigMap* m, SigValue k) {
    SigValue out;
    if (k.t == T_STR && sig_map_get(m, k.u.s, &out)) return out;
    return sig_nil();
}

/* ------------------------------------------------------------------ */
/* Operators                                                            */
/* ------------------------------------------------------------------ */

static SigValue sig_add_slow(SigValue a, SigValue b, int line) {
    if (a.t == T_STR || b.t == T_STR) return sig_strv(sig_str_cat(sig_to_str(a), sig_to_str(b)));
    if (a.t == T_LIST && b.t == T_LIST) {
        SigList* r = sig_list_new(a.u.l->len + b.u.l->len);
        memcpy(r->items, a.u.l->items, (size_t)a.u.l->len * sizeof(SigValue));
        memcpy(r->items + a.u.l->len, b.u.l->items, (size_t)b.u.l->len * sizeof(SigValue));
        r->len = a.u.l->len + b.u.l->len;
        return sig_listv(r);
    }
    sig_fail(line, "can't add %s %s and %s %s", sig_tname(a.t), sig_short(a), sig_tname(b.t), sig_short(b));
}

static inline SigValue sig_add(SigValue a, SigValue b, int line) {
    if (SIG_LIKELY(a.t == T_NUM && b.t == T_NUM)) return sig_num(a.u.n + b.u.n);
    if (a.t == T_STR && b.t == T_STR) return sig_strv(sig_str_cat(a.u.s, b.u.s));
    return sig_add_slow(a, b, line);
}

static inline double sig_div(double a, double b, int line) {
    if (SIG_UNLIKELY(b == 0)) sig_fail(line, "division by zero");
    return a / b;
}

/* Modulo with the sign of the divisor (like Python): -7 % 3 == 2. */
static inline double sig_mod(double a, double b, int line) {
    if (SIG_UNLIKELY(b == 0)) sig_fail(line, "modulo by zero");
    if (fabs(a) < 9e15 && fabs(b) < 9e15) {
        int64_t ia = (int64_t)a, ib = (int64_t)b;
        if ((double)ia == a && (double)ib == b) {
            int64_t r = ia % ib;
            if (r != 0 && ((r < 0) != (ib < 0))) r += ib;
            return (double)r;
        }
    }
    double r = fmod(a, b);
    if (r != 0 && ((r < 0) != (b < 0))) r += b;
    return r;
}

static inline double sig_min2(double a, double b) { return a < b ? a : b; }
static inline double sig_max2(double a, double b) { return a > b ? a : b; }

static int sig_cmp(SigValue a, SigValue b, int line) {
    if (a.t == T_NUM && b.t == T_NUM) return (a.u.n > b.u.n) - (a.u.n < b.u.n);
    if (a.t == T_STR && b.t == T_STR) {
        size_t n = a.u.s->len < b.u.s->len ? a.u.s->len : b.u.s->len;
        int c = memcmp(sig_sd(a.u.s), sig_sd(b.u.s), n);
        if (c) return c < 0 ? -1 : 1;
        return (a.u.s->len > b.u.s->len) - (a.u.s->len < b.u.s->len);
    }
    sig_fail(line, "can't compare %s %s with %s %s", sig_tname(a.t), sig_short(a), sig_tname(b.t), sig_short(b));
}

static bool sig_eq(SigValue a, SigValue b) {
    if (a.t != b.t) return false;
    switch (a.t) {
    case T_NIL: return true;
    case T_NUM: return a.u.n == b.u.n;
    case T_BOOL: return a.u.b == b.u.b;
    case T_STR: return sig_str_eq(a.u.s, b.u.s);
    case T_FN: return a.u.f == b.u.f;
    case T_LIST:
        if (a.u.l == b.u.l) return true;
        if (a.u.l->len != b.u.l->len) return false;
        for (int64_t i = 0; i < a.u.l->len; i++)
            if (!sig_eq(a.u.l->items[i], b.u.l->items[i])) return false;
        return true;
    case T_MAP:
        if (a.u.m == b.u.m) return true;
        if (a.u.m->count != b.u.m->count) return false;
        for (int64_t i = 0; i < a.u.m->len; i++) {
            SigEntry* e = &a.u.m->entries[i];
            SigValue other;
            if (!e->key) continue;
            if (!sig_map_get(b.u.m, e->key, &other) || !sig_eq(e->val, other)) return false;
        }
        return true;
    }
    return false;
}

/* ------------------------------------------------------------------ */
/* Indexing, fields, iteration                                          */
/* ------------------------------------------------------------------ */

static SigValue sig_index_slow(SigValue c, SigValue k, int line) {
    switch (c.t) {
    case T_LIST: return c.u.l->items[sig_norm_index(k, c.u.l->len, line, "list")];
    case T_STR: {
        int64_t i = sig_norm_index(k, (int64_t)c.u.s->len, line, "string");
        return sig_str_lit(sig_sd(c.u.s) + i, 1);
    }
    case T_MAP: {
        SigValue out;
        if (sig_map_get(c.u.m, sig_key(k, line), &out)) return out;
        return sig_nil();
    }
    default: sig_fail(line, "can't index into %s %s", sig_tname(c.t), sig_short(c));
    }
}

static inline SigValue sig_index(SigValue c, SigValue k, int line) {
    if (SIG_LIKELY(c.t == T_LIST && k.t == T_NUM)) {
        double d = k.u.n;
        int64_t i = (int64_t)d;
        if (SIG_LIKELY((double)i == d && i >= 0 && i < c.u.l->len)) return c.u.l->items[i];
    }
    return sig_index_slow(c, k, line);
}

static SigValue sig_set_index(SigValue c, SigValue k, SigValue v, int line) {
    switch (c.t) {
    case T_LIST: c.u.l->items[sig_norm_index(k, c.u.l->len, line, "list")] = v; break;
    case T_MAP: sig_map_set(c.u.m, sig_key(k, line), v); break;
    case T_STR: sig_fail(line, "strings can't be changed in place - build a new one instead");
    default: sig_fail(line, "can't index into %s %s", sig_tname(c.t), sig_short(c));
    }
    return v;
}

static void sig_field_err(SigValue o, SigValue key, int line) __attribute__((noreturn, cold));
static void sig_field_err(SigValue o, SigValue key, int line) {
    const char* k = sig_cstr(key.u.s);
    if (!strcmp(k, "len") || !strcmp(k, "length") || !strcmp(k, "size") || !strcmp(k, "count"))
        sig_fail(line, "%s has no field .%s - use len(x) to get the length", sig_tname(o.t), k);
    sig_fail(line, "can't read .%s on %s %s (only maps have fields)", k, sig_tname(o.t), sig_short(o));
}

static inline SigValue sig_field(SigValue o, SigValue key, int line) {
    if (SIG_LIKELY(o.t == T_MAP)) {
        SigValue out;
        if (sig_map_get(o.u.m, key.u.s, &out)) return out;
        return sig_nil();
    }
    sig_field_err(o, key, line);
}

static SigValue sig_set_field(SigValue o, SigValue key, SigValue v, int line) {
    if (o.t != T_MAP) sig_fail(line, "can't set .%s on %s %s (only maps have fields)", sig_cstr(key.u.s), sig_tname(o.t), sig_short(o));
    sig_map_set(o.u.m, key.u.s, v);
    return v;
}

/* What `grind x in value` walks over: lists as-is, strings by character,
 * maps by key. */
static SigList* sig_iterable(SigValue v, int line) {
    switch (v.t) {
    case T_LIST: return v.u.l;
    case T_STR: {
        SigList* l = sig_list_new((int64_t)v.u.s->len);
        for (size_t i = 0; i < v.u.s->len; i++) l->items[i] = sig_str_lit(sig_sd(v.u.s) + i, 1);
        l->len = (int64_t)v.u.s->len;
        return l;
    }
    case T_MAP: {
        SigList* l = sig_list_new(v.u.m->count);
        for (int64_t i = 0; i < v.u.m->len; i++)
            if (v.u.m->entries[i].key) l->items[l->len++] = sig_strv(v.u.m->entries[i].key);
        return l;
    }
    case T_NUM: sig_fail(line, "can't loop over a num - did you mean  grind i in 0..%s ?", sig_short(v));
    default: sig_fail(line, "can't loop over %s %s", sig_tname(v.t), sig_short(v));
    }
}

/* ------------------------------------------------------------------ */
/* Functions                                                            */
/* ------------------------------------------------------------------ */

static SigValue sig_call(SigValue f, int argc, SigValue* argv, int line) {
    if (SIG_UNLIKELY(f.t != T_FN)) sig_fail(line, "can't call %s %s - it isn't a function", sig_tname(f.t), sig_short(f));
    if (SIG_UNLIKELY(f.u.f->arity != argc))
        sig_fail(line, "%s expects %d argument%s, got %d", f.u.f->name, f.u.f->arity, f.u.f->arity == 1 ? "" : "s", argc);
    return f.u.f->fn(f.u.f, argc, argv, line);
}

static SigValue sig_make_fn(SigFnPtr fn, int arity, const char* name, int ncaps, const SigValue* caps) {
    SigFn* f = (SigFn*)sig_alloc(sizeof(SigFn) + (size_t)ncaps * sizeof(SigValue));
    f->fn = fn;
    f->arity = arity;
    f->ncaps = ncaps;
    f->name = name;
    if (ncaps) memcpy(f->caps, caps, (size_t)ncaps * sizeof(SigValue));
    return sig_fnv(f);
}

/* Callbacks for map/filter/each get (item) or (item, index). */
static SigValue sig_call_item(SigValue f, SigValue item, int64_t idx, int line) {
    if (f.t == T_FN && f.u.f->arity == 2) {
        SigValue a[2] = {item, sig_num((double)idx)};
        return sig_call(f, 2, a, line);
    }
    return sig_call(f, 1, &item, line);
}

/* obj.name(args) where name isn't a built-in method: call a function stored
 * in a map field. */
static SigValue sig_m_fallback(SigValue self, const char* name, int argc, SigValue* argv, int line) {
    if (self.t == T_MAP) {
        SigValue f;
        if (sig_map_get(self.u.m, sig_str_new(name, strlen(name)), &f)) {
            if (f.t == T_FN) return sig_call(f, argc, argv, line);
            sig_fail(line, ".%s is %s %s, not a function", name, sig_tname(f.t), sig_short(f));
        }
        sig_fail(line, "this map has no method or field '%s'", name);
    }
    sig_fail(line, "%s has no .%s() method", sig_tname(self.t), name);
}

static SigValue sig_m_call(SigValue self, SigValue name, int argc, SigValue* argv, int line) {
    return sig_m_fallback(self, sig_cstr(name.u.s), argc, argv, line);
}

/* ------------------------------------------------------------------ */
/* Sorting (stable merge sort)                                          */
/* ------------------------------------------------------------------ */

typedef struct { SigValue k, v; } SigPair;

static void sig_msort(SigPair* a, SigPair* tmp, int64_t n, int dir, int line) {
    if (n < 2) return;
    if (n <= 16) {
        for (int64_t i = 1; i < n; i++) {
            SigPair x = a[i];
            int64_t j = i;
            while (j > 0 && sig_cmp(a[j - 1].k, x.k, line) * dir > 0) { a[j] = a[j - 1]; j--; }
            a[j] = x;
        }
        return;
    }
    int64_t m = n / 2, i = 0, j = m, k = 0;
    sig_msort(a, tmp, m, dir, line);
    sig_msort(a + m, tmp, n - m, dir, line);
    while (i < m && j < n) tmp[k++] = (sig_cmp(a[j].k, a[i].k, line) * dir < 0) ? a[j++] : a[i++];
    while (i < m) tmp[k++] = a[i++];
    while (j < n) tmp[k++] = a[j++];
    memcpy(a, tmp, (size_t)n * sizeof(SigPair));
}

/* ------------------------------------------------------------------ */
/* Methods: list, str, map                                              */
/* ------------------------------------------------------------------ */

#define SIG_FALLBACK(name, n, ...)                       \
    do {                                                 \
        SigValue av_[] = {sig_nil(), ##__VA_ARGS__};     \
        return sig_m_fallback(self, name, n, av_ + 1, line); \
    } while (0)

static SigValue sig_m_push(SigValue self, SigValue a, int line) {
    if (SIG_LIKELY(self.t == T_LIST)) { sig_list_push(self.u.l, a); return sig_nil(); }
    SIG_FALLBACK("push", 1, a);
}

static SigValue sig_m_pop(SigValue self, int line) {
    if (self.t == T_LIST) {
        if (self.u.l->len == 0) sig_fail(line, "can't pop() from an empty list");
        return self.u.l->items[--self.u.l->len];
    }
    SIG_FALLBACK("pop", 0);
}

static SigValue sig_m_insert(SigValue self, SigValue i, SigValue x, int line) {
    if (self.t == T_LIST) {
        SigList* l = self.u.l;
        int64_t at = (i.t == T_NUM && i.u.n == (double)l->len) ? l->len : sig_norm_index(i, l->len + 1, line, "list");
        if (l->len == l->cap) sig_list_grow(l, l->len + 1);
        memmove(l->items + at + 1, l->items + at, (size_t)(l->len - at) * sizeof(SigValue));
        l->items[at] = x;
        l->len++;
        return sig_nil();
    }
    SIG_FALLBACK("insert", 2, i, x);
}

static SigValue sig_m_del(SigValue self, SigValue k, int line) {
    if (self.t == T_LIST) {
        SigList* l = self.u.l;
        int64_t at = sig_norm_index(k, l->len, line, "list");
        SigValue v = l->items[at];
        memmove(l->items + at, l->items + at + 1, (size_t)(l->len - at - 1) * sizeof(SigValue));
        l->len--;
        return v;
    }
    if (self.t == T_MAP) return sig_bool(sig_map_del(self.u.m, sig_key(k, line)));
    SIG_FALLBACK("del", 1, k);
}

static SigValue sig_m_has(SigValue self, SigValue x, int line) {
    switch (self.t) {
    case T_LIST:
        for (int64_t i = 0; i < self.u.l->len; i++)
            if (sig_eq(self.u.l->items[i], x)) return sig_bool(true);
        return sig_bool(false);
    case T_STR: {
        SigStr* n = sig_as_str(x, line, ".has() on a string");
        return sig_bool(sig_memfind(sig_sd(self.u.s), self.u.s->len, sig_sd(n), n->len, 0) >= 0);
    }
    case T_MAP: return sig_bool(sig_map_find(self.u.m, sig_key(x, line)) >= 0);
    default: SIG_FALLBACK("has", 1, x);
    }
}

static SigValue sig_m_find(SigValue self, SigValue x, int line) {
    if (self.t == T_LIST) {
        for (int64_t i = 0; i < self.u.l->len; i++)
            if (sig_eq(self.u.l->items[i], x)) return sig_num((double)i);
        return sig_num(-1);
    }
    if (self.t == T_STR) {
        SigStr* n = sig_as_str(x, line, ".find() on a string");
        return sig_num((double)sig_memfind(sig_sd(self.u.s), self.u.s->len, sig_sd(n), n->len, 0));
    }
    SIG_FALLBACK("find", 1, x);
}

static SigValue sig_m_join(SigValue self, SigValue sep, int line) {
    if (self.t == T_LIST) {
        SigStr* s = sep.t == T_NIL ? NULL : sig_to_str(sep);
        SigSB b = {0};
        for (int64_t i = 0; i < self.u.l->len; i++) {
            if (i && s) sig_sb_put(&b, sig_sd(s), s->len);
            sig_write(&b, self.u.l->items[i], false, 0);
        }
        return sig_strv(sig_sb_finish(&b));
    }
    SIG_FALLBACK("join", 1, sep);
}

static SigValue sig_m_reverse(SigValue self, int line) {
    if (self.t == T_LIST) {
        int64_t n = self.u.l->len;
        SigList* r = sig_list_new(n);
        for (int64_t i = 0; i < n; i++) r->items[i] = self.u.l->items[n - 1 - i];
        r->len = n;
        return sig_listv(r);
    }
    if (self.t == T_STR) {
        size_t n = self.u.s->len;
        SigStr* r = sig_str_make(n, n);
        for (size_t i = 0; i < n; i++) r->buf->data[i] = sig_sd(self.u.s)[n - 1 - i];
        return sig_strv(r);
    }
    SIG_FALLBACK("reverse", 0);
}

/* sort()  sort("desc")  sort(key_fn)  sort(key_fn, "desc") - returns a new list. */
static SigValue sig_m_sort(SigValue self, SigValue a, SigValue b, int line) {
    if (self.t != T_LIST) SIG_FALLBACK("sort", 2, a, b);
    SigValue key = sig_nil();
    int dir = 1;
    SigValue opts[2] = {a, b};
    for (int i = 0; i < 2; i++) {
        SigValue o = opts[i];
        if (o.t == T_FN) key = o;
        else if (o.t == T_STR) {
            const char* s = sig_cstr(o.u.s);
            if (!strcmp(s, "desc")) dir = -1;
            else if (strcmp(s, "asc")) sig_fail(line, "sort() expects \"asc\" or \"desc\", got \"%s\"", s);
        } else if (o.t != T_NIL) sig_fail(line, "sort() expects a key function or \"asc\"/\"desc\", got %s", sig_tname(o.t));
    }
    int64_t n = self.u.l->len;
    SigPair* ps = (SigPair*)sig_xrealloc(NULL, (size_t)(n ? n : 1) * 2 * sizeof(SigPair));
    for (int64_t i = 0; i < n; i++) {
        ps[i].v = self.u.l->items[i];
        ps[i].k = key.t == T_FN ? sig_call(key, 1, &ps[i].v, line) : ps[i].v;
    }
    sig_msort(ps, ps + n, n, dir, line);
    SigList* r = sig_list_new(n);
    for (int64_t i = 0; i < n; i++) r->items[i] = ps[i].v;
    r->len = n;
    free(ps);
    return sig_listv(r);
}

static SigValue sig_m_map(SigValue self, SigValue f, int line) {
    if (self.t != T_LIST) SIG_FALLBACK("map", 1, f);
    int64_t n = self.u.l->len;
    SigList* r = sig_list_new(n);
    for (int64_t i = 0; i < n && i < self.u.l->len; i++) sig_list_push(r, sig_call_item(f, self.u.l->items[i], i, line));
    return sig_listv(r);
}

static SigValue sig_m_filter(SigValue self, SigValue f, int line) {
    if (self.t != T_LIST) SIG_FALLBACK("filter", 1, f);
    SigList* r = sig_list_new(4);
    for (int64_t i = 0; i < self.u.l->len; i++) {
        SigValue item = self.u.l->items[i];
        if (sig_truthy(sig_call_item(f, item, i, line))) sig_list_push(r, item);
    }
    return sig_listv(r);
}

static SigValue sig_m_each(SigValue self, SigValue f, int line) {
    if (self.t != T_LIST) SIG_FALLBACK("each", 1, f);
    for (int64_t i = 0; i < self.u.l->len; i++) sig_call_item(f, self.u.l->items[i], i, line);
    return sig_nil();
}

static SigValue sig_m_reduce(SigValue self, SigValue f, SigValue acc, int line) {
    if (self.t != T_LIST) SIG_FALLBACK("reduce", 2, f, acc);
    for (int64_t i = 0; i < self.u.l->len; i++) {
        SigValue a[2] = {acc, self.u.l->items[i]};
        acc = sig_call(f, 2, a, line);
    }
    return acc;
}

static int64_t sig_clamp_index(SigValue k, int64_t len, int line) {
    if (k.t != T_NUM) sig_type_err(line, "bad slice index", "num", k);
    int64_t i = (int64_t)k.u.n;
    if (i < 0) i += len;
    if (i < 0) i = 0;
    if (i > len) i = len;
    return i;
}

static SigValue sig_m_slice(SigValue self, SigValue a, SigValue b, int line) {
    int64_t len;
    if (self.t == T_LIST) len = self.u.l->len;
    else if (self.t == T_STR) len = (int64_t)self.u.s->len;
    else SIG_FALLBACK("slice", 2, a, b);
    int64_t s = sig_clamp_index(a, len, line);
    int64_t e = b.t == T_NIL ? len : sig_clamp_index(b, len, line);
    if (e < s) e = s;
    if (self.t == T_STR) return sig_str_lit(sig_sd(self.u.s) + s, (size_t)(e - s));
    return sig_list_of((int)(e - s), self.u.l->items + s);
}

static SigValue sig_m_sum(SigValue self, int line) {
    if (self.t != T_LIST) SIG_FALLBACK("sum", 0);
    double t = 0;
    for (int64_t i = 0; i < self.u.l->len; i++) t += sig_as_num(self.u.l->items[i], line);
    return sig_num(t);
}

static SigValue sig_m_copy(SigValue self, int line) {
    if (self.t == T_LIST) return sig_list_of((int)self.u.l->len, self.u.l->items);
    if (self.t == T_MAP) {
        SigMap* m = sig_map_new();
        for (int64_t i = 0; i < self.u.m->len; i++)
            if (self.u.m->entries[i].key) sig_map_set(m, self.u.m->entries[i].key, self.u.m->entries[i].val);
        return sig_mapv(m);
    }
    SIG_FALLBACK("copy", 0);
}

static SigValue sig_m_clear(SigValue self, int line) {
    if (self.t == T_LIST) { self.u.l->len = 0; return sig_nil(); }
    if (self.t == T_MAP) {
        self.u.m->len = self.u.m->count = 0;
        for (int64_t i = 0; i < self.u.m->nslots; i++) self.u.m->slots[i] = -1;
        return sig_nil();
    }
    SIG_FALLBACK("clear", 0);
}

static SigValue sig_str_map_chars(SigValue self, int (*fn)(int)) {
    size_t n = self.u.s->len;
    SigStr* r = sig_str_make(n, n);
    for (size_t i = 0; i < n; i++) r->buf->data[i] = (char)fn((unsigned char)sig_sd(self.u.s)[i]);
    return sig_strv(r);
}

static SigValue sig_m_upper(SigValue self, int line) {
    if (self.t == T_STR) return sig_str_map_chars(self, toupper);
    SIG_FALLBACK("upper", 0);
}

static SigValue sig_m_lower(SigValue self, int line) {
    if (self.t == T_STR) return sig_str_map_chars(self, tolower);
    SIG_FALLBACK("lower", 0);
}

static SigValue sig_m_trim(SigValue self, int line) {
    if (self.t != T_STR) SIG_FALLBACK("trim", 0);
    const char* d = sig_sd(self.u.s);
    size_t s = 0, e = self.u.s->len;
    while (s < e && isspace((unsigned char)d[s])) s++;
    while (e > s && isspace((unsigned char)d[e - 1])) e--;
    return sig_str_lit(d + s, e - s);
}

static SigValue sig_m_split(SigValue self, SigValue sep, int line) {
    if (self.t != T_STR) SIG_FALLBACK("split", 1, sep);
    const char* d = sig_sd(self.u.s);
    size_t n = self.u.s->len;
    SigList* r = sig_list_new(4);
    if (sep.t == T_NIL) { /* split on runs of whitespace */
        size_t i = 0;
        while (i < n) {
            while (i < n && isspace((unsigned char)d[i])) i++;
            size_t st = i;
            while (i < n && !isspace((unsigned char)d[i])) i++;
            if (i > st) sig_list_push(r, sig_str_lit(d + st, i - st));
        }
        return sig_listv(r);
    }
    SigStr* s = sig_as_str(sep, line, "split() separator");
    if (s->len == 0) {
        for (size_t i = 0; i < n; i++) sig_list_push(r, sig_str_lit(d + i, 1));
        return sig_listv(r);
    }
    size_t st = 0;
    for (;;) {
        int64_t at = sig_memfind(d, n, sig_sd(s), s->len, st);
        if (at < 0) break;
        sig_list_push(r, sig_str_lit(d + st, (size_t)at - st));
        st = (size_t)at + s->len;
    }
    sig_list_push(r, sig_str_lit(d + st, n - st));
    return sig_listv(r);
}

static SigValue sig_m_starts(SigValue self, SigValue p, int line) {
    if (self.t != T_STR) SIG_FALLBACK("starts", 1, p);
    SigStr* s = sig_as_str(p, line, "starts()");
    return sig_bool(s->len <= self.u.s->len && memcmp(sig_sd(self.u.s), sig_sd(s), s->len) == 0);
}

static SigValue sig_m_ends(SigValue self, SigValue p, int line) {
    if (self.t != T_STR) SIG_FALLBACK("ends", 1, p);
    SigStr* s = sig_as_str(p, line, "ends()");
    return sig_bool(s->len <= self.u.s->len &&
                    memcmp(sig_sd(self.u.s) + self.u.s->len - s->len, sig_sd(s), s->len) == 0);
}

static SigValue sig_m_replace(SigValue self, SigValue a, SigValue b, int line) {
    if (self.t != T_STR) SIG_FALLBACK("replace", 2, a, b);
    SigStr* from = sig_as_str(a, line, "replace()");
    SigStr* to = sig_to_str(b);
    if (from->len == 0) sig_fail(line, "replace() can't search for an empty string");
    const char* d = sig_sd(self.u.s);
    size_t n = self.u.s->len, st = 0;
    SigSB out = {0};
    for (;;) {
        int64_t at = sig_memfind(d, n, sig_sd(from), from->len, st);
        if (at < 0) break;
        sig_sb_put(&out, d + st, (size_t)at - st);
        sig_sb_put(&out, sig_sd(to), to->len);
        st = (size_t)at + from->len;
    }
    sig_sb_put(&out, d + st, n - st);
    return sig_strv(sig_sb_finish(&out));
}

static SigValue sig_m_repeat(SigValue self, SigValue k, int line) {
    if (self.t != T_STR) SIG_FALLBACK("repeat", 1, k);
    double c = sig_as_num(k, line);
    if (c < 0 || c != floor(c)) sig_fail(line, "repeat() needs a whole number >= 0");
    SigSB out = {0};
    for (int64_t i = 0; i < (int64_t)c; i++) sig_sb_put(&out, sig_sd(self.u.s), self.u.s->len);
    return sig_strv(sig_sb_finish(&out));
}

static SigValue sig_m_chars(SigValue self, int line) {
    if (self.t != T_STR) SIG_FALLBACK("chars", 0);
    return sig_listv(sig_iterable(self, line));
}

static SigValue sig_m_keys(SigValue self, int line) {
    if (self.t != T_MAP) SIG_FALLBACK("keys", 0);
    return sig_listv(sig_iterable(self, line));
}

static SigValue sig_m_values(SigValue self, int line) {
    if (self.t != T_MAP) SIG_FALLBACK("values", 0);
    SigList* l = sig_list_new(self.u.m->count);
    for (int64_t i = 0; i < self.u.m->len; i++)
        if (self.u.m->entries[i].key) l->items[l->len++] = self.u.m->entries[i].val;
    return sig_listv(l);
}

static SigValue sig_m_get(SigValue self, SigValue k, SigValue dflt, int line) {
    if (self.t == T_MAP) {
        SigValue out;
        if (sig_map_get(self.u.m, sig_key(k, line), &out)) return out;
        return dflt;
    }
    if (self.t == T_LIST) {
        if (k.t == T_NUM) {
            int64_t i = (int64_t)k.u.n;
            if (i < 0) i += self.u.l->len;
            if ((double)(int64_t)k.u.n == k.u.n && i >= 0 && i < self.u.l->len) return self.u.l->items[i];
        }
        return dflt;
    }
    SIG_FALLBACK("get", 2, k, dflt);
}

/* ------------------------------------------------------------------ */
/* Built-in functions                                                   */
/* ------------------------------------------------------------------ */

static void sig_print_val(SigValue v) {
    if (v.t == T_STR) { fwrite(sig_sd(v.u.s), 1, v.u.s->len, stdout); return; }
    if (v.t == T_NUM) {
        char tmp[40];
        fwrite(tmp, 1, (size_t)sig_fmt_num(tmp, v.u.n), stdout);
        return;
    }
    SigSB b = {0};
    sig_write(&b, v, false, 0);
    fwrite(b.d, 1, b.len, stdout);
    free(b.d);
}

static SigValue sig_yap(int n, const SigValue* vs) {
    for (int i = 0; i < n; i++) {
        if (i) putchar(' ');
        sig_print_val(vs[i]);
    }
    putchar('\n');
    return sig_nil();
}

static SigValue sig_yap1(SigValue v) { sig_print_val(v); putchar('\n'); return sig_nil(); }

static SigValue sig_yap_num(double x) {
    char tmp[40];
    int k = sig_fmt_num(tmp, x);
    tmp[k] = '\n';
    fwrite(tmp, 1, (size_t)k + 1, stdout);
    return sig_nil();
}

static SigValue sig_interp(int n, const SigValue* parts) {
    SigSB b = {0};
    for (int i = 0; i < n; i++) sig_write(&b, parts[i], false, 0);
    return sig_strv(sig_sb_finish(&b));
}

static SigValue sig_ask(SigValue prompt) {
    if (prompt.t != T_NIL) sig_print_val(prompt);
    fflush(stdout);
    char* line = NULL;
    size_t cap = 0;
    ssize_t n = getline(&line, &cap, stdin);
    if (n < 0) { free(line); return sig_nil(); }
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) n--;
    SigValue v = sig_str_lit(line, (size_t)n);
    free(line);
    return v;
}

static double sig_len(SigValue v, int line) {
    switch (v.t) {
    case T_STR: return (double)v.u.s->len;
    case T_LIST: return (double)v.u.l->len;
    case T_MAP: return (double)v.u.m->count;
    default: sig_fail(line, "len() needs a str, list or map, got %s %s", sig_tname(v.t), sig_short(v));
    }
}

static SigValue sig_type(SigValue v) {
    static SigValue cache[7];
    if (cache[v.t].t == T_NIL) cache[v.t] = sig_cstrv(sig_tname(v.t));
    return cache[v.t];
}

/* num("42") -> 42, num("hi") -> nil */
static SigValue sig_tonum(SigValue v) {
    switch (v.t) {
    case T_NUM: return v;
    case T_BOOL: return sig_num(v.u.b ? 1 : 0);
    case T_STR: {
        const char* s = sig_cstr(v.u.s);
        while (isspace((unsigned char)*s)) s++;
        if (!*s) return sig_nil();
        char* end;
        errno = 0;
        double d = strtod(s, &end);
        while (isspace((unsigned char)*end)) end++;
        if (*end || errno == ERANGE) return sig_nil();
        return sig_num(d);
    }
    default: return sig_nil();
    }
}

static SigValue sig_toint(SigValue v) {
    SigValue n = sig_tonum(v);
    if (n.t != T_NUM) return n;
    return sig_num(trunc(n.u.n));
}

static SigValue sig_tostr(SigValue v) { return sig_strv(sig_to_str(v)); }

static SigValue sig_minmax(int n, SigValue* vs, bool want_max, int line) {
    if (n == 1 && vs[0].t == T_LIST) {
        n = (int)vs[0].u.l->len;
        vs = vs[0].u.l->items;
    }
    if (n == 0) sig_fail(line, "%s() of an empty list", want_max ? "max" : "min");
    SigValue best = vs[0];
    for (int i = 1; i < n; i++) {
        int c = sig_cmp(vs[i], best, line);
        if (want_max ? c > 0 : c < 0) best = vs[i];
    }
    return best;
}

static uint64_t sig_rng;
static double sig_rand(void) {
    uint64_t x = sig_rng;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    sig_rng = x;
    return (double)((x * 2685821657736338717ULL) >> 11) * (1.0 / 9007199254740992.0);
}

static double sig_rand_int(double a, double b, int line) {
    (void)line;
    if (a > b) { double t = a; a = b; b = t; }
    a = ceil(a);
    b = floor(b);
    return a + floor(sig_rand() * (b - a + 1));
}

static double sig_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static SigValue sig_sleep(double s) {
    if (s > 0) {
        struct timespec ts;
        ts.tv_sec = (time_t)s;
        ts.tv_nsec = (long)((s - (double)ts.tv_sec) * 1e9);
        nanosleep(&ts, NULL);
    }
    return sig_nil();
}

static SigValue sig_args(void) {
    SigList* l = sig_list_new(sig_argc);
    for (int i = 1; i < sig_argc; i++) sig_list_push(l, sig_cstrv(sig_argv[i]));
    return sig_listv(l);
}

static SigValue sig_exit_v(double code) {
    fflush(stdout);
    exit((int)code);
}

static SigValue sig_read_file(SigValue path, int line) {
    const char* p = sig_cstr(sig_as_str(path, line, "read_file() path"));
    FILE* f = fopen(p, "rb");
    if (!f) sig_fail(line, "can't read file '%s': %s", p, strerror(errno));
    SigSB b = {0};
    char chunk[65536];
    size_t n;
    while ((n = fread(chunk, 1, sizeof chunk, f)) > 0) sig_sb_put(&b, chunk, n);
    fclose(f);
    return sig_strv(sig_sb_finish(&b));
}

static SigValue sig_write_file(SigValue path, SigValue data, bool append, int line) {
    const char* p = sig_cstr(sig_as_str(path, line, "write_file() path"));
    FILE* f = fopen(p, append ? "ab" : "wb");
    if (!f) sig_fail(line, "can't write file '%s': %s", p, strerror(errno));
    SigStr* s = sig_to_str(data);
    fwrite(sig_sd(s), 1, s->len, f);
    fclose(f);
    return sig_nil();
}

static bool sig_file_exists(SigValue path, int line) {
    return access(sig_cstr(sig_as_str(path, line, "file_exists() path")), F_OK) == 0;
}

static SigValue sig_vibecheck(SigValue cond, SigValue msg, int line) {
    if (sig_truthy(cond)) return sig_nil();
    if (msg.t == T_NIL) sig_fail(line, "vibe check failed");
    sig_fail(line, "vibe check failed: %s", sig_cstr(sig_to_str(msg)));
}

/* ------------------------------------------------------------------ */
/* Startup                                                              */
/* ------------------------------------------------------------------ */

static void sig_on_crash(int signo) {
    (void)signo;
    fflush(stdout);
    static const char msg[] = "error: crashed - this usually means infinite recursion (stack overflow)\n";
    ssize_t r = write(2, msg, sizeof msg - 1);
    (void)r;
    _exit(1);
}

static void sig_init(int argc, char** argv, const char** files) {
    sig_argc = argc;
    sig_argv = argv;
    sig_files = files;

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    sig_rng = ((uint64_t)ts.tv_nsec << 20) ^ (uint64_t)ts.tv_sec ^ ((uint64_t)getpid() * 0x9E3779B97F4A7C15ULL);
    if (!sig_rng) sig_rng = 88172645463325252ULL;

    static char outbuf[1 << 16];
    if (!isatty(1)) setvbuf(stdout, outbuf, _IOFBF, sizeof outbuf);

    static char altstack[1 << 16];
    stack_t ss;
    ss.ss_sp = altstack;
    ss.ss_size = sizeof altstack;
    ss.ss_flags = 0;
    sigaltstack(&ss, NULL);
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = sig_on_crash;
    sa.sa_flags = SA_ONSTACK;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
}

#endif /* SIGMA_RUNTIME_H */
