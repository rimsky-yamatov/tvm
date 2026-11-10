/* tvc.c - compiler for a small C-like language.   usage: tvc in.tc [out.tas]
 *
 * Types: int, char (unsigned byte), int*, char*.  Global arrays (int a[N], char s[N]), string literals,
 * functions, if/else, while, for, break, continue, return, #define NAME tokens, and built-in functions
 * for every SYS call.  Output is tvm assembly: assemble it with tasm.
 *
 * Limits: no local arrays (use globals), no & operator, no ++/-- inside expressions (statements only),
 * pointers are plain addresses (p[i] scales by the element type; there is no pointer arithmetic).
 * Locals are zeroed once on function entry.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

/* ------------------------------------------------------------ buffers / output */
typedef struct { char *p; size_t n, cap; } Buf;

static void bcat(Buf *b, const char *s)
{
    size_t l = strlen(s);
    if (b->n + l + 1 > b->cap) {
        b->cap = (b->n + l + 1) * 2;
        b->p = realloc(b->p, b->cap);
        if (!b->p) { fprintf(stderr, "tvc: out of memory\n"); exit(1); }
    }
    memcpy(b->p + b->n, s, l + 1);
    b->n += l;
}

static void bf(Buf *b, const char *fmt, ...)
{
    char t[2048]; va_list ap;
    va_start(ap, fmt); vsnprintf(t, sizeof t, fmt, ap); va_end(ap);
    bcat(b, t);
}

static Buf *out;                         /* where code is currently emitted */
static int lbl;

static void emit(const char *fmt, ...)
{
    char t[600]; va_list ap;
    va_start(ap, fmt); vsnprintf(t, sizeof t, fmt, ap); va_end(ap);
    bcat(out, "        "); bcat(out, t); bcat(out, "\n");
}
static int  newlbl(void) { return lbl++; }
static void label(int n) { bf(out, "L%d:\n", n); }

static const char *fname = "input";
static void die(int line, const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "%s:%d: error: ", fname, line);
    va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
    fprintf(stderr, "\n");
    exit(1);
}

static char *dupn(const char *s, size_t n) { char *r = malloc(n + 1); memcpy(r, s, n); r[n] = 0; return r; }

/* ------------------------------------------------------------ lexer */
enum { T_EOF, T_NUM, T_STR, T_ID, T_OP };
typedef struct { int k; long v; char *s; int len; int line; } Tok;
static Tok *tk; static int ntk, captk;

static void addtok(Tok t)
{
    if (ntk + 2 > captk) { captk = captk ? captk * 2 : 1024; tk = realloc(tk, (size_t)captk * sizeof *tk); }
    tk[ntk++] = t;
}
static void seal(void)                   /* keep an EOF sentinel after the last token */
{
    if (ntk + 2 > captk) { captk = captk ? captk * 2 : 1024; tk = realloc(tk, (size_t)captk * sizeof *tk); }
    memset(&tk[ntk], 0, sizeof tk[ntk]);
    tk[ntk].k = T_EOF; tk[ntk].line = ntk ? tk[ntk - 1].line : 1;
}

typedef struct { char name[40]; Tok *t; int n; } Macro;
static Macro mac[256]; static int nmac;

static int esc_char(const char **ps, int line)
{
    const char *s = *ps; int c = *s++;
    switch (c) {
    case 'n': c = 10; break;
    case 't': c = 9; break;
    case 'r': c = 13; break;
    case '0': c = 0; break;
    case '\\': case '\'': case '"': break;
    default: die(line, "unknown escape sequence");
    }
    *ps = s;
    return c;
}

static int scan(const char **ps, int *line, Tok *t, int stop_nl)
{
    const char *s = *ps;
    for (;;) {
        while (*s == ' ' || *s == '\t' || *s == '\r') s++;
        if (*s == '\n') { if (stop_nl) break; (*line)++; s++; continue; }
        if (s[0] == '/' && s[1] == '/') { while (*s && *s != '\n') s++; continue; }
        if (s[0] == '/' && s[1] == '*') {
            s += 2;
            while (*s && !(s[0] == '*' && s[1] == '/')) { if (*s == '\n') (*line)++; s++; }
            if (!*s) die(*line, "unterminated comment");
            s += 2;
            continue;
        }
        break;
    }
    *ps = s;
    if (!*s || *s == '\n') return 0;
    t->line = *line; t->s = NULL; t->len = 0; t->v = 0;
    if (isdigit((unsigned char)*s)) {
        char *e;
        t->k = T_NUM; t->v = (long)(int)strtoul(s, &e, 0); s = e;
        if (isalnum((unsigned char)*s) || *s == '_') die(*line, "bad number");
    } else if (isalpha((unsigned char)*s) || *s == '_') {
        const char *b = s;
        while (isalnum((unsigned char)*s) || *s == '_') s++;
        t->k = T_ID; t->s = dupn(b, (size_t)(s - b));
    } else if (*s == '\'') {
        s++; t->k = T_NUM;
        if (*s == '\\') { s++; t->v = esc_char(&s, *line); } else t->v = (unsigned char)*s++;
        if (*s != '\'') die(*line, "bad character literal");
        s++;
    } else if (*s == '"') {
        char *b = malloc(strlen(s) + 1); int n = 0;
        s++;
        while (*s != '"') {
            if (!*s || *s == '\n') die(*line, "unterminated string");
            if (*s == '\\') { s++; b[n++] = (char)esc_char(&s, *line); } else b[n++] = *s++;
        }
        s++; b[n] = 0; t->k = T_STR; t->s = b; t->len = n;
    } else {
        static const char *ops3[] = { "<<=", ">>=", NULL };
        static const char *ops2[] = { "==", "!=", "<=", ">=", "&&", "||", "<<", ">>", "+=", "-=", "*=", "/=",
                                      "%=", "&=", "|=", "^=", "++", "--", NULL };
        int i, l = 1;
        if (!strchr("+-*/%&|^~!<>=?:;,(){}[]#", *s)) die(*line, "unexpected character '%c'", *s);
        for (i = 0; ops3[i]; i++) if (!strncmp(s, ops3[i], 3)) l = 3;
        if (l == 1) for (i = 0; ops2[i]; i++) if (!strncmp(s, ops2[i], 2)) l = 2;
        t->k = T_OP; t->s = dupn(s, (size_t)l); s += l;
    }
    *ps = s;
    return 1;
}

static Macro *find_macro(const char *n)
{
    int i;
    for (i = 0; i < nmac; i++) if (!strcmp(mac[i].name, n)) return &mac[i];
    return NULL;
}

static void lex(const char *src)
{
    const char *s = src; int line = 1, lastline = 0; Tok t;
    while (scan(&s, &line, &t, 0)) {
        int bol = t.line != lastline; Macro *m;
        lastline = t.line;
        if (t.k == T_OP && !strcmp(t.s, "#") && bol) {                  /* #define NAME tokens... */
            Tok d, nm; Macro *nw;
            if (!scan(&s, &line, &d, 1) || d.k != T_ID || strcmp(d.s, "define")) die(t.line, "only #define is supported");
            if (!scan(&s, &line, &nm, 1) || nm.k != T_ID) die(t.line, "macro name expected");
            if (nmac >= 256) die(t.line, "too many macros");
            nw = &mac[nmac++];
            snprintf(nw->name, sizeof nw->name, "%s", nm.s); nw->t = NULL; nw->n = 0;
            while (scan(&s, &line, &d, 1)) {
                Macro *o = d.k == T_ID ? find_macro(d.s) : NULL;
                if (o && o != nw) {                                      /* expand earlier macros now */
                    int j;
                    nw->t = realloc(nw->t, (size_t)(nw->n + o->n) * sizeof(Tok));
                    for (j = 0; j < o->n; j++) nw->t[nw->n++] = o->t[j];
                } else {
                    nw->t = realloc(nw->t, (size_t)(nw->n + 1) * sizeof(Tok));
                    nw->t[nw->n++] = d;
                }
            }
            continue;
        }
        if (t.k == T_ID && (m = find_macro(t.s)) != NULL) {
            int j;
            for (j = 0; j < m->n; j++) { Tok x = m->t[j]; x.line = t.line; addtok(x); }
            continue;
        }
        addtok(t);
    }
}

/* ------------------------------------------------------------ tables */
typedef struct { const char *name; int nargs, kind, num; const char *op; int ret; } Bi;
static const Bi bi[] = {
    /* kind 0: SYS num | kind 1: instruction | kind 2: special */
    {"putc",1,0,0,0,0}, {"puti",1,0,1,0,0}, {"getc",0,0,2,0,1},
    {"win",2,0,10,0,1}, {"wsel",1,0,11,0,0}, {"wclose",1,0,12,0,0}, {"wtitle",1,0,13,0,0},
    {"getpixel",2,0,14,0,1}, {"blit",6,0,15,0,0}, {"circle",4,0,16,0,0}, {"fcircle",4,0,17,0,0},
    {"recto",5,0,18,0,0}, {"pixel",3,0,20,0,0}, {"rect",5,0,21,0,0}, {"line",5,0,22,0,0},
    {"text",4,0,23,0,0}, {"clear",1,0,24,0,0}, {"pal",2,0,25,0,0}, {"flip",0,0,26,0,0},
    {"tick",0,0,31,0,1}, {"sleep",1,0,32,0,0}, {"rnd",1,0,33,0,1}, {"key",1,0,35,0,1},
    {"sin",1,0,40,0,1}, {"cos",1,0,41,0,1}, {"sqrt",1,0,42,0,1}, {"beep",2,0,50,0,0},
    {"fopen",2,0,60,0,1}, {"fclose",1,0,61,0,1}, {"fread",3,0,62,0,1}, {"fwrite",3,0,63,0,1},
    {"fgetc",1,0,64,0,1}, {"fputc",2,0,65,0,1}, {"fseek",2,0,66,0,1}, {"fsize",1,0,67,0,1},
    {"fremove",1,0,68,0,1}, {"exit",1,0,99,0,0},
    {"abs",1,1,0,"ABS",1}, {"min",2,1,0,"MIN",1}, {"max",2,1,0,"MAX",1}, {"fmul",2,1,0,"FMUL",1},
    {"fdiv",2,1,0,"FDIV",1}, {"memset",3,1,0,"MEMSET",0}, {"memcpy",3,1,0,"MEMCPY",0}, {"halt",0,1,0,"HALT",0},
    {"poll",0,2,0,0,1}, {"mouse",0,2,0,0,0},
};
#define NBI ((int)(sizeof bi / sizeof bi[0]))

static int find_bi(const char *n)
{
    int i;
    for (i = 0; i < NBI; i++) if (!strcmp(bi[i].name, n)) return i;
    return -1;
}

/* library functions written in the language itself; only included when used */
static const struct { const char *name, *code; } prelude[] = {
    { "strlen", "int strlen(char *s) { int n = 0; while (s[n] != 0) { n++; } return n; }" },
    { "strcmp", "int strcmp(char *a, char *b) { int i = 0; while (a[i] != 0 && a[i] == b[i]) { i++; } return a[i] - b[i]; }" },
    { "print",  "void print(char *s) { int i = 0; while (s[i] != 0) { putc(s[i]); i++; } }" },
    { "puts",   "void puts(char *s) { print(s); putc(10); }" },
    { "itoa",   "void itoa(int n, char *buf) { int i = 0; int neg = 0; int j; int t; if (n < 0) { neg = 1; n = 0 - n; } "
                "if (n == 0) { buf[0] = 48; i = 1; } while (n > 0) { buf[i] = 48 + n % 10; n = n / 10; i++; } "
                "if (neg) { buf[i] = 45; i++; } buf[i] = 0; j = 0; i--; "
                "while (j < i) { t = buf[j]; buf[j] = buf[i]; buf[i] = t; j++; i--; } }" },
};
#define NPRE ((int)(sizeof prelude / sizeof prelude[0]))

typedef struct { char name[40]; int nparams, ret, defined; Buf code; int callee[128]; int ncallee, reach; } Func;
static Func fn[256]; static int nfn;
static Func *cf;                         /* function being compiled */

static Func *find_fn(const char *n)
{
    int i;
    for (i = 0; i < nfn; i++) if (!strcmp(fn[i].name, n)) return &fn[i];
    return NULL;
}

typedef struct { char name[40]; int elem, off; } Local;      /* elem: 0 int, 1 int*, 2 char* */
static Local loc[256]; static int nloc, nslots;

typedef struct { char name[40]; int elem, isarr, initk, sidx, nlist; long len, ival, *list; } Glob;
static Glob gl[512]; static int ngl;

typedef struct { char *s; int len; } Str;
static Str strs[1024]; static int nstr;

static int add_str(const char *s, int len)
{
    int i;
    for (i = 0; i < nstr; i++) if (strs[i].len == len && !memcmp(strs[i].s, s, (size_t)len)) return i;
    if (nstr >= 1024) die(0, "too many string literals");
    strs[nstr].s = dupn(s, (size_t)len); strs[nstr].len = len;
    return nstr++;
}

static int find_local(const char *n) { int i; for (i = nloc - 1; i >= 0; i--) if (!strcmp(loc[i].name, n)) return i; return -1; }
static int find_glob(const char *n)  { int i; for (i = 0; i < ngl; i++) if (!strcmp(gl[i].name, n)) return i; return -1; }

static int add_glob(const char *name, int line)
{
    if (find_glob(name) >= 0) die(line, "'%s' declared twice", name);
    if (ngl >= 512) die(line, "too many global variables");
    memset(&gl[ngl], 0, sizeof gl[ngl]);
    snprintf(gl[ngl].name, sizeof gl[ngl].name, "%s", name);
    return ngl++;
}

/* ------------------------------------------------------------ parser helpers */
static int p;
static int op_at(int i, const char *s) { return tk[i].k == T_OP && !strcmp(tk[i].s, s); }
static int is_op(const char *s) { return op_at(p, s); }
static int is_id(const char *s) { return tk[p].k == T_ID && !strcmp(tk[p].s, s); }
static void next(void) { if (tk[p].k != T_EOF) p++; }
static int accept(const char *s) { if (is_op(s)) { p++; return 1; } return 0; }
static void expect(const char *s) { if (!accept(s)) die(tk[p].line, "expected '%s'", s); }
static int is_type(void) { return is_id("int") || is_id("char") || is_id("void"); }

/* ------------------------------------------------------------ pre-pass: collect function signatures */
static void reg_fn(const char *name, int np, int ret, int line, int body)
{
    Func *f;
    if (find_bi(name) >= 0) die(line, "'%s' is a built-in function", name);
    f = find_fn(name);
    if (!f) {
        if (nfn >= 256) die(line, "too many functions");
        f = &fn[nfn++]; memset(f, 0, sizeof *f);
        snprintf(f->name, sizeof f->name, "%s", name); f->nparams = np; f->ret = ret;
    } else if (f->nparams != np || f->ret != ret) die(line, "conflicting declaration of '%s'", name);
    if (body) { if (f->defined) die(line, "function '%s' defined twice", name); f->defined = 1; }
}

static void prepass(int from)
{
    int i = from;
    while (tk[i].k != T_EOF) {
        if (tk[i].k == T_ID && (!strcmp(tk[i].s, "int") || !strcmp(tk[i].s, "char") || !strcmp(tk[i].s, "void"))) {
            int j = i + 1, ret = strcmp(tk[i].s, "void") != 0;
            while (op_at(j, "*")) j++;
            if (tk[j].k == T_ID && op_at(j + 1, "(")) {
                int k = j + 2, commas = 0, d = 1, body;
                int empty = op_at(k, ")") || (tk[k].k == T_ID && !strcmp(tk[k].s, "void") && op_at(k + 1, ")"));
                while (d > 0 && tk[k].k != T_EOF) {
                    if (op_at(k, "(")) d++; else if (op_at(k, ")")) d--; else if (d == 1 && op_at(k, ",")) commas++;
                    k++;
                }
                body = op_at(k, "{");
                reg_fn(tk[j].s, empty ? 0 : commas + 1, ret, tk[j].line, body);
                if (body) {
                    int bd = 0;
                    while (tk[k].k != T_EOF) {
                        if (op_at(k, "{")) bd++;
                        else if (op_at(k, "}")) { bd--; if (bd == 0) { k++; break; } }
                        k++;
                    }
                } else { while (tk[k].k != T_EOF && !op_at(k, ";")) k++; if (tk[k].k != T_EOF) k++; }
                i = k;
                continue;
            }
            { int bd = 0;                                               /* global declaration: skip to ';' */
              while (tk[i].k != T_EOF) {
                  if (op_at(i, "{")) bd++; else if (op_at(i, "}")) bd--;
                  else if (bd == 0 && op_at(i, ";")) { i++; break; }
                  i++;
              } }
            continue;
        }
        i++;
    }
}

static int used(const char *name)
{
    int i;
    for (i = 0; tk[i].k != T_EOF; i++) if (tk[i].k == T_ID && !strcmp(tk[i].s, name) && op_at(i + 1, "(")) return 1;
    return 0;
}

static void add_prelude(void)
{
    int changed = 1, i;
    while (changed) {
        changed = 0;
        for (i = 0; i < NPRE; i++) {
            Func *f = find_fn(prelude[i].name);
            if ((f && f->defined) || !used(prelude[i].name)) continue;
            { int from = ntk, saved = nmac;
              nmac = 0;                                                  /* user macros must not leak into the library */
              lex(prelude[i].code); seal(); nmac = saved;
              prepass(from); changed = 1; }
        }
    }
}

/* ------------------------------------------------------------ constant expressions (array sizes, initialisers) */
static long cexpr(int minp);

static long cprimary(void)
{
    long v;
    if (accept("-")) return -cprimary();
    if (accept("+")) return cprimary();
    if (accept("~")) return ~cprimary();
    if (accept("(")) { v = cexpr(1); expect(")"); return v; }
    if (tk[p].k == T_NUM) { v = tk[p].v; next(); return v; }
    die(tk[p].line, "constant expression expected");
    return 0;
}

static int cprec(void)
{
    const char *s;
    if (tk[p].k != T_OP) return 0;
    s = tk[p].s;
    if (!strcmp(s, "|")) return 1;
    if (!strcmp(s, "^")) return 2;
    if (!strcmp(s, "&")) return 3;
    if (!strcmp(s, "<<") || !strcmp(s, ">>")) return 4;
    if (!strcmp(s, "+") || !strcmp(s, "-")) return 5;
    if (!strcmp(s, "*") || !strcmp(s, "/") || !strcmp(s, "%")) return 6;
    return 0;
}

static long cexpr(int minp)
{
    long v = cprimary();
    for (;;) {
        int pr = cprec(), line = tk[p].line; char o[4]; long r;
        if (!pr || pr < minp) return v;
        snprintf(o, sizeof o, "%s", tk[p].s); next();
        r = cexpr(pr + 1);
        if (!strcmp(o, "|")) v |= r; else if (!strcmp(o, "^")) v ^= r; else if (!strcmp(o, "&")) v &= r;
        else if (!strcmp(o, "<<")) v <<= (r & 31); else if (!strcmp(o, ">>")) v >>= (r & 31);
        else if (!strcmp(o, "+")) v += r; else if (!strcmp(o, "-")) v -= r; else if (!strcmp(o, "*")) v *= r;
        else { if (!r) die(line, "division by zero in constant expression"); v = o[0] == '/' ? v / r : v % r; }
    }
}

/* ------------------------------------------------------------ expressions */
typedef struct { int is_local, idx; } VarRef;
static void expr(void);
static int call_expr(void);

static VarRef lookup(const char *n, int line)
{
    VarRef r; int i = find_local(n);
    if (i >= 0) { r.is_local = 1; r.idx = i; return r; }
    i = find_glob(n);
    if (i < 0) die(line, "undeclared identifier '%s'", n);
    r.is_local = 0; r.idx = i;
    return r;
}
static int var_elem(VarRef r)  { return r.is_local ? loc[r.idx].elem : gl[r.idx].elem; }
static int var_isarr(VarRef r) { return r.is_local ? 0 : gl[r.idx].isarr; }

static void push_var_value(VarRef r)      /* scalar value, or the address of an array */
{
    if (r.is_local) emit("LDL %d", loc[r.idx].off);
    else if (gl[r.idx].isarr) emit("PUSH g_%s", gl[r.idx].name);
    else { emit("PUSH g_%s", gl[r.idx].name); emit("LD"); }
}
static void store_var(VarRef r)           /* value is on top of the stack */
{
    if (r.is_local) emit("STL %d", loc[r.idx].off);
    else { emit("PUSH g_%s", gl[r.idx].name); emit("ST"); }
}
static void index_addr(VarRef r, int line)   /* at '[': leaves the element address on the stack */
{
    int e = var_elem(r);
    if (e == 0) die(line, "not an array or pointer");
    push_var_value(r);
    expect("["); expr(); expect("]");
    if (e == 1) { emit("PUSH 2"); emit("SHL"); }
    emit("ADD");
}
static void load_elem(int e) { emit(e == 1 ? "LD" : "LD8"); }

static int bprec(const char *s)
{
    if (!strcmp(s, "||")) return 1;
    if (!strcmp(s, "&&")) return 2;
    if (!strcmp(s, "|")) return 3;
    if (!strcmp(s, "^")) return 4;
    if (!strcmp(s, "&")) return 5;
    if (!strcmp(s, "==") || !strcmp(s, "!=")) return 6;
    if (!strcmp(s, "<") || !strcmp(s, ">") || !strcmp(s, "<=") || !strcmp(s, ">=")) return 7;
    if (!strcmp(s, "<<") || !strcmp(s, ">>")) return 8;
    if (!strcmp(s, "+") || !strcmp(s, "-")) return 9;
    if (!strcmp(s, "*") || !strcmp(s, "/") || !strcmp(s, "%")) return 10;
    return 0;
}

static const char *binop(const char *s)
{
    static const char *m[][2] = {
        {"+","ADD"},{"-","SUB"},{"*","MUL"},{"/","DIV"},{"%","MOD"},{"&","AND"},{"|","OR"},{"^","XOR"},
        {"<<","SHL"},{">>","SHR"},{"==","EQ"},{"!=","NE"},{"<","LT"},{">","GT"},{"<=","LE"},{">=","GE"} };
    size_t i;
    for (i = 0; i < sizeof m / sizeof m[0]; i++) if (!strcmp(m[i][0], s)) return m[i][1];
    return "NOP";
}

static void unary(void)
{
    Tok *t = &tk[p];
    if (t->k == T_NUM) { emit("PUSH %ld", t->v); next(); return; }
    if (t->k == T_STR) { emit("PUSH s_%d", add_str(t->s, t->len)); next(); return; }
    if (t->k == T_ID) {
        char name[40]; int line = t->line; VarRef r;
        if (op_at(p + 1, "(")) { if (!call_expr()) die(line, "void function used as a value"); return; }
        snprintf(name, sizeof name, "%s", t->s); next();
        r = lookup(name, line);
        if (is_op("[")) { index_addr(r, line); load_elem(var_elem(r)); }
        else push_var_value(r);
        return;
    }
    if (t->k == T_OP) {
        int line = t->line;
        if (!strcmp(t->s, "(")) { next(); expr(); expect(")"); return; }
        if (!strcmp(t->s, "-")) {
            next();
            if (tk[p].k == T_NUM) { emit("PUSH %ld", -tk[p].v); next(); } else { unary(); emit("NEG"); }
            return;
        }
        if (!strcmp(t->s, "+")) { next(); unary(); return; }
        if (!strcmp(t->s, "!")) { next(); unary(); emit("PUSH 0"); emit("EQ"); return; }
        if (!strcmp(t->s, "~")) { next(); unary(); emit("NOT"); return; }
        if (!strcmp(t->s, "*")) {                                       /* *p */
            VarRef r;
            next();
            if (tk[p].k != T_ID) die(line, "pointer variable expected after '*'");
            r = lookup(tk[p].s, line); next();
            if (var_elem(r) == 0) die(line, "not a pointer");
            push_var_value(r); load_elem(var_elem(r));
            return;
        }
    }
    die(t->line, "expression expected");
}

static void binary(int minp)
{
    unary();
    for (;;) {
        Tok *t = &tk[p]; int pr; char o[4];
        if (t->k != T_OP || !(pr = bprec(t->s)) || pr < minp) return;
        snprintf(o, sizeof o, "%s", t->s); next();
        if (!strcmp(o, "&&")) {
            int lf = newlbl(), le = newlbl();
            emit("JZ L%d", lf); binary(pr + 1); emit("JZ L%d", lf);
            emit("PUSH 1"); emit("JMP L%d", le); label(lf); emit("PUSH 0"); label(le);
        } else if (!strcmp(o, "||")) {
            int lt = newlbl(), le = newlbl();
            emit("JNZ L%d", lt); binary(pr + 1); emit("JNZ L%d", lt);
            emit("PUSH 0"); emit("JMP L%d", le); label(lt); emit("PUSH 1"); label(le);
        } else { binary(pr + 1); emit("%s", binop(o)); }
    }
}

static void expr(void)
{
    binary(1);
    if (accept("?")) {
        int lf = newlbl(), le = newlbl();
        emit("JZ L%d", lf); expr(); emit("JMP L%d", le); expect(":"); label(lf); expr(); label(le);
    }
}

static int call_expr(void)                /* at identifier '(' ; returns 1 if the call leaves a value */
{
    char name[40]; int line = tk[p].line, argc = 0, n, bidx; Func *f = NULL;
    snprintf(name, sizeof name, "%s", tk[p].s); next(); expect("(");
    bidx = find_bi(name);
    if (bidx < 0) { f = find_fn(name); if (!f) die(line, "undefined function '%s'", name); }
    n = bidx >= 0 ? bi[bidx].nargs : f->nparams;
    if (!is_op(")")) { do { expr(); argc++; } while (accept(",")); }
    expect(")");
    if (argc != n) die(line, "'%s' expects %d argument(s), got %d", name, n, argc);
    if (bidx >= 0) {
        const Bi *b = &bi[bidx];
        if (b->kind == 0) emit("SYS %d", b->num);
        else if (b->kind == 1) emit("%s", b->op);
        else if (!strcmp(b->name, "poll")) {                            /* sets ev_win, ev_type, ev_a, ev_b; returns the type */
            emit("SYS 30"); emit("PUSH g_ev_b"); emit("ST"); emit("PUSH g_ev_a"); emit("ST");
            emit("PUSH g_ev_type"); emit("ST"); emit("PUSH g_ev_win"); emit("ST");
            emit("PUSH g_ev_type"); emit("LD");
        } else {                                                        /* mouse(): sets mouse_win, mouse_x, mouse_y, mouse_btn */
            emit("SYS 34"); emit("PUSH g_mouse_btn"); emit("ST"); emit("PUSH g_mouse_y"); emit("ST");
            emit("PUSH g_mouse_x"); emit("ST"); emit("PUSH g_mouse_win"); emit("ST");
        }
        return b->ret;
    }
    emit("CALL f_%s", name);
    { int i, k = 0, id = (int)(f - fn);
      for (i = 0; i < cf->ncallee; i++) if (cf->callee[i] == id) k = 1;
      if (!k) { if (cf->ncallee >= 128) die(line, "function calls too many different functions"); cf->callee[cf->ncallee++] = id; } }
    return f->ret;
}

/* ------------------------------------------------------------ statements */
typedef struct { int kind, elem; VarRef r; } LV;       /* kind 0: variable, 1: memory (address on the stack) */

static LV lvalue(void)
{
    LV lv; int line = tk[p].line;
    memset(&lv, 0, sizeof lv);
    if (accept("*")) {
        if (tk[p].k != T_ID) die(line, "pointer variable expected after '*'");
        lv.r = lookup(tk[p].s, line); next();
        if (var_elem(lv.r) == 0) die(line, "not a pointer");
        push_var_value(lv.r); lv.kind = 1; lv.elem = var_elem(lv.r);
        return lv;
    }
    if (tk[p].k != T_ID) die(line, "statement expected");
    lv.r = lookup(tk[p].s, line); next();
    if (is_op("[")) { index_addr(lv.r, line); lv.kind = 1; lv.elem = var_elem(lv.r); }
    else if (var_isarr(lv.r)) die(line, "cannot assign to an array");
    return lv;
}
static void lv_load(LV *lv)  { if (lv->kind == 0) push_var_value(lv->r); else { emit("DUP"); load_elem(lv->elem); } }
static void lv_store(LV *lv) { if (lv->kind == 0) store_var(lv->r); else { emit("SWAP"); emit(lv->elem == 1 ? "ST" : "ST8"); } }

static void simple_stmt(void)             /* call, assignment, compound assignment, ++ / -- (no trailing ';') */
{
    int line = tk[p].line; LV lv; const char *ins = NULL; Tok *t;
    if (tk[p].k == T_ID && op_at(p + 1, "(")) { if (call_expr()) emit("DROP"); return; }
    if (is_op("++") || is_op("--")) {
        ins = is_op("++") ? "INC" : "DEC"; next();
        lv = lvalue(); lv_load(&lv); emit("%s", ins); lv_store(&lv);
        return;
    }
    lv = lvalue(); t = &tk[p];
    if (accept("=")) { expr(); lv_store(&lv); return; }
    if (t->k == T_OP) {
        const char *s = t->s;
        if (!strcmp(s, "+=")) ins = "ADD"; else if (!strcmp(s, "-=")) ins = "SUB"; else if (!strcmp(s, "*=")) ins = "MUL";
        else if (!strcmp(s, "/=")) ins = "DIV"; else if (!strcmp(s, "%=")) ins = "MOD"; else if (!strcmp(s, "&=")) ins = "AND";
        else if (!strcmp(s, "|=")) ins = "OR"; else if (!strcmp(s, "^=")) ins = "XOR"; else if (!strcmp(s, "<<=")) ins = "SHL";
        else if (!strcmp(s, ">>=")) ins = "SHR";
        else if (!strcmp(s, "++") || !strcmp(s, "--")) {
            ins = s[0] == '+' ? "INC" : "DEC"; next();
            lv_load(&lv); emit("%s", ins); lv_store(&lv);
            return;
        }
    }
    if (!ins) die(line, "assignment or function call expected");
    next(); lv_load(&lv); expr(); emit("%s", ins); lv_store(&lv);
}

static void local_decl(void)
{
    int line = tk[p].line, ptr_elem = is_id("char") ? 2 : 1;
    if (is_id("void")) die(line, "variables cannot be void");
    next();
    do {
        int elem = 0, i;
        while (accept("*")) elem = ptr_elem;
        if (tk[p].k != T_ID) die(tk[p].line, "variable name expected");
        if (nslots >= 120 || nloc >= 250) die(line, "too many local variables");
        i = nloc++;
        snprintf(loc[i].name, sizeof loc[i].name, "%s", tk[p].s); loc[i].elem = elem; loc[i].off = nslots++;
        next();
        if (is_op("[")) die(line, "local arrays are not supported; declare the array as a global");
        if (accept("=")) { expr(); emit("STL %d", loc[i].off); }
    } while (accept(","));
    expect(";");
}

static int loop_cont[32], loop_brk[32], nloop;

static void stmt(void)
{
    int line = tk[p].line;
    if (accept("{")) {
        int save = nloc;
        while (!is_op("}")) { if (tk[p].k == T_EOF) die(line, "missing '}'"); stmt(); }
        next(); nloc = save;
        return;
    }
    if (accept(";")) return;
    if (is_id("if")) {
        int le = newlbl();
        next(); expect("("); expr(); expect(")"); emit("JZ L%d", le);
        stmt();
        if (is_id("else")) { int lend = newlbl(); next(); emit("JMP L%d", lend); label(le); stmt(); label(lend); }
        else label(le);
        return;
    }
    if (is_id("while")) {
        int lc = newlbl(), lb = newlbl();
        if (nloop >= 32) die(line, "loops nested too deeply");
        next(); label(lc); expect("("); expr(); expect(")"); emit("JZ L%d", lb);
        loop_cont[nloop] = lc; loop_brk[nloop++] = lb; stmt(); nloop--;
        emit("JMP L%d", lc); label(lb);
        return;
    }
    if (is_id("for")) {
        int ltop = newlbl(), lc = newlbl(), lb = newlbl(), save = nloc; Buf step = {0}, *sv;
        if (nloop >= 32) die(line, "loops nested too deeply");
        next(); expect("(");
        if (is_type()) local_decl(); else { if (!is_op(";")) simple_stmt(); expect(";"); }
        label(ltop);
        if (!is_op(";")) { expr(); emit("JZ L%d", lb); }
        expect(";");
        sv = out; out = &step; if (!is_op(")")) simple_stmt(); out = sv; expect(")");
        loop_cont[nloop] = lc; loop_brk[nloop++] = lb; stmt(); nloop--;
        label(lc); if (step.p) bcat(out, step.p);
        emit("JMP L%d", ltop); label(lb);
        nloc = save;
        return;
    }
    if (is_id("break") || is_id("continue")) {
        int brk = is_id("break");
        if (!nloop) die(line, "'%s' outside of a loop", brk ? "break" : "continue");
        emit("JMP L%d", brk ? loop_brk[nloop - 1] : loop_cont[nloop - 1]);
        next(); expect(";");
        return;
    }
    if (is_id("return")) {
        next();
        if (cf->ret) {
            if (is_op(";")) die(line, "return value expected");
            expr(); expect(";"); emit("RETV %d", cf->nparams);
        } else {
            if (!is_op(";")) die(line, "void function cannot return a value");
            next(); emit("RET %d", cf->nparams);
        }
        return;
    }
    if (is_type()) { local_decl(); return; }
    simple_stmt(); expect(";");
}

/* ------------------------------------------------------------ declarations */
static void func_def(void)
{
    int line = tk[p].line, np = 0, i, ptr_elem = is_id("char") ? 2 : 1; char name[40]; Buf body = {0};
    Func *f;
    next();
    while (accept("*")) ;
    snprintf(name, sizeof name, "%s", tk[p].s); next();
    f = find_fn(name); cf = f;
    expect("(");
    nloc = 0; nslots = 0; nloop = 0;
    if (is_id("void") && op_at(p + 1, ")")) next();
    else if (!is_op(")")) {
        do {
            int elem = 0, pe = is_id("char") ? 2 : 1;
            if (!is_type() || is_id("void")) die(tk[p].line, "parameter type expected");
            next();
            while (accept("*")) elem = pe;
            if (tk[p].k != T_ID) die(tk[p].line, "parameter name expected");
            if (np >= 100) die(line, "too many parameters");
            snprintf(loc[np].name, sizeof loc[np].name, "%s", tk[p].s); loc[np].elem = elem;
            np++; next();
        } while (accept(","));
    }
    (void)ptr_elem;
    expect(")");
    for (i = 0; i < np; i++) loc[i].off = i - np;                       /* first argument at -np ... last at -1 */
    nloc = np;
    expect("{");
    out = &body;
    while (!is_op("}")) { if (tk[p].k == T_EOF) die(line, "missing '}' at end of function '%s'", name); stmt(); }
    next();
    if (f->ret) { emit("PUSH 0"); emit("RETV %d", np); } else emit("RET %d", np);
    out = &f->code;
    bf(out, "f_%s:\n", name);
    if (nslots) emit("ENTER %d", nslots);
    bcat(out, body.p);
}

static void global_decl(void)
{
    int line = tk[p].line, base_char = is_id("char"), ptr_elem = base_char ? 2 : 1;
    if (is_id("void")) die(line, "variables cannot be void");
    next();
    do {
        int elem = 0, g, isarr = 0; long len = -1; Glob *G;
        while (accept("*")) elem = ptr_elem;
        if (tk[p].k != T_ID) die(tk[p].line, "variable name expected");
        g = add_glob(tk[p].s, tk[p].line); G = &gl[g]; next();
        if (accept("[")) {
            if (elem) die(line, "arrays of pointers are not supported");
            isarr = 1; elem = ptr_elem;
            if (!is_op("]")) len = cexpr(1);
            expect("]");
        }
        G->elem = elem; G->isarr = isarr;
        if (accept("=")) {
            if (isarr) {
                if (tk[p].k == T_STR) {
                    if (!base_char) die(line, "string initializer needs a char array");
                    G->initk = 3; G->sidx = add_str(tk[p].s, tk[p].len); G->nlist = tk[p].len + 1; next();
                } else if (accept("{")) {
                    G->initk = 4;
                    while (!is_op("}")) {
                        G->list = realloc(G->list, (size_t)(G->nlist + 1) * sizeof(long));
                        G->list[G->nlist++] = cexpr(1);
                        if (!accept(",")) break;
                    }
                    expect("}");
                } else die(line, "array initializer expected");
                if (len < 0) len = G->nlist;
                if (G->nlist > len) die(line, "too many initializers for '%s'", G->name);
            } else if (tk[p].k == T_STR) {
                if (!elem) die(line, "string initializer needs a pointer or char array");
                G->initk = 2; G->sidx = add_str(tk[p].s, tk[p].len); next();
            } else {
                G->ival = cexpr(1);
                if (G->ival) G->initk = 1;
            }
        }
        if (isarr && (len < 1 || len * (elem == 1 ? 4 : 1) > 65536)) die(line, "bad array size for '%s'", G->name);
        G->len = len;
    } while (accept(","));
    expect(";");
}

/* ------------------------------------------------------------ output */
static void emit_string(Buf *b, const char *s, int len)
{
    int i, ok = len <= 120;
    for (i = 0; i < len && ok; i++) {
        unsigned char c = (unsigned char)s[i];
        if (!(c >= 32 && c <= 126) && c != 10 && c != 9 && c != 13 && c != 0) ok = 0;
    }
    if (ok) {
        bcat(b, "        .str \"");
        for (i = 0; i < len; i++) {
            unsigned char c = (unsigned char)s[i];
            if (c == '"') bcat(b, "\\\""); else if (c == '\\') bcat(b, "\\\\"); else if (c == 10) bcat(b, "\\n");
            else if (c == 9) bcat(b, "\\t"); else if (c == 13) bcat(b, "\\r"); else if (c == 0) bcat(b, "\\0");
            else { char t[2]; t[0] = (char)c; t[1] = 0; bcat(b, t); }
        }
        bcat(b, "\"\n");
    } else {
        for (i = 0; i < len; i += 16) {
            int j;
            bcat(b, "        .byte ");
            for (j = i; j < len && j < i + 16; j++) bf(b, "%s%d", j > i ? "," : "", (unsigned char)s[j]);
            bcat(b, "\n");
        }
        bcat(b, "        .byte 0\n");
    }
}

static void mark(Func *f)
{
    int i;
    if (f->reach) return;
    if (!f->defined) die(0, "function '%s' is declared but never defined", f->name);
    f->reach = 1;
    for (i = 0; i < f->ncallee; i++) mark(&fn[f->callee[i]]);
}

int main(int argc, char **argv)
{
    FILE *fp; char *src, oname[1024], *dot; long n; Func *mf; Buf o = {0}; int i, j, k;
    static const char *evn[] = { "ev_win", "ev_type", "ev_a", "ev_b", "mouse_win", "mouse_x", "mouse_y", "mouse_btn" };
    if (argc < 2) { fprintf(stderr, "usage: tvc in.tc [out.tas]\n"); return 2; }
    fname = argv[1];
    if (argc > 2) snprintf(oname, sizeof oname, "%s", argv[2]);
    else {
        snprintf(oname, sizeof oname, "%s", argv[1]);
        dot = strrchr(oname, '.');
        if (dot && !strchr(dot, '/') && !strchr(dot, '\\')) *dot = 0;
        strncat(oname, ".tas", sizeof oname - strlen(oname) - 1);
    }
    if (!(fp = fopen(argv[1], "rb"))) { perror(argv[1]); return 1; }
    fseek(fp, 0, SEEK_END); n = ftell(fp); fseek(fp, 0, SEEK_SET);
    src = malloc((size_t)n + 1);
    if (!src || fread(src, 1, (size_t)n, fp) != (size_t)n) { fprintf(stderr, "read error\n"); return 1; }
    src[n] = 0; fclose(fp);

    lex(src); seal();
    prepass(0);
    add_prelude();
    for (i = 0; i < 8; i++) add_glob(evn[i], 0);

    p = 0;
    while (tk[p].k != T_EOF) {
        int q;
        if (!is_type()) die(tk[p].line, "declaration expected");
        q = p + 1;
        while (op_at(q, "*")) q++;
        if (tk[q].k == T_ID && op_at(q + 1, "(")) {
            int d = 0, r = q + 1;
            while (tk[r].k != T_EOF) { if (op_at(r, "(")) d++; else if (op_at(r, ")")) { d--; if (!d) { r++; break; } } r++; }
            if (op_at(r, "{")) func_def();
            else { while (tk[p].k != T_EOF && !is_op(";")) next(); next(); }          /* prototype */
        } else global_decl();
    }

    mf = find_fn("main");
    if (!mf || !mf->defined) die(0, "no main function");
    if (mf->nparams) die(0, "main must not take parameters");
    mark(mf);

    out = &o;
    bf(&o, "; generated by tvc from %s\n", fname);
    emit("CALL f_main");
    if (mf->ret) emit("SYS 99"); else emit("HALT");                      /* main's value becomes the exit code */
    for (i = 0; i < nfn; i++) if (fn[i].reach) bcat(&o, fn[i].code.p);
    for (i = 0; i < nstr; i++) { bf(&o, "s_%d:\n", i); emit_string(&o, strs[i].s, strs[i].len); }
    for (i = 0; i < ngl; i++) {                                           /* initialised data first ... */
        Glob *G = &gl[i];
        if (!G->initk) continue;
        bf(&o, "g_%s:\n", G->name);
        if (G->initk == 1) bf(&o, "        .word %ld\n", G->ival);
        else if (G->initk == 2) bf(&o, "        .word s_%d\n", G->sidx);
        else if (G->initk == 3) {
            emit_string(&o, strs[G->sidx].s, strs[G->sidx].len);
            if (G->len > G->nlist) bf(&o, "        .space %ld\n", G->len - G->nlist);
        } else {
            for (j = 0; j < G->nlist; j += 8) {
                bf(&o, "        %s ", G->elem == 1 ? ".word" : ".byte");
                for (k = j; k < G->nlist && k < j + 8; k++) bf(&o, "%s%ld", k > j ? "," : "", G->list[k]);
                bcat(&o, "\n");
            }
            if (G->len > G->nlist) bf(&o, "        .space %ld\n", (G->len - G->nlist) * (G->elem == 1 ? 4 : 1));
        }
    }
    for (i = 0; i < ngl; i++) {                                           /* ... zeroed data last (trailing zeros are not stored) */
        Glob *G = &gl[i];
        if (G->initk) continue;
        bf(&o, "g_%s:\n        .space %ld\n", G->name, G->isarr ? G->len * (G->elem == 1 ? 4 : 1) : 4L);
    }
    if (!(fp = fopen(oname, "wb"))) { perror(oname); return 1; }
    fwrite(o.p, 1, o.n, fp);
    fclose(fp);
    return 0;
}
