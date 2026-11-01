/*
 * DIGITAL DOCUMENT SEARCH SYSTEM
 * Advanced Data Structures in C - Project Based Learning
 *
 * Advanced concepts used:
 *   1. Standard Trie      -> inverted index (word -> documents), prefix search / autocomplete
 *   2. AVL Tree           -> self-balancing title index (sorted listing, title search, delete)
 *   3. Pattern Matching   -> Brute Force, KMP, Boyer-Moore (bad character) with comparison counts
 *   4. Merge Sort         -> ranking of search results by relevance
 *   5. File Organization  -> binary file (docs.dat) storing records, text-file import
 *
 * Compile : gcc doc_search.c -o doc_search
 * Run     : ./doc_search
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif

#define MAX_DOCS   1000
#define TITLE_LEN  100
#define DATA_FILE  "docs.dat"
#define ALPHA      36          /* a-z and 0-9 */

/* ====================================================================
 *  DOCUMENT STORE
 * ==================================================================== */
typedef struct {
    int   id;
    char  title[TITLE_LEN];
    char *content;
    int   len;
} Document;

Document *docs[MAX_DOCS];      /* docs[id] ; NULL if empty/deleted */
int nextId   = 1;
int docCount = 0;

/* ====================================================================
 *  INPUT HELPERS
 * ==================================================================== */
void read_line(const char *prompt, char *buf, int size) {
    printf("%s", prompt);
    if (!fgets(buf, size, stdin)) { buf[0] = '\0'; return; }
    buf[strcspn(buf, "\r\n")] = '\0';
}

int read_int(const char *prompt) {
    char b[64];
    read_line(prompt, b, sizeof b);
    return atoi(b);
}

void to_lower_copy(const char *src, char *dst) {
    int i;
    for (i = 0; src[i]; i++) dst[i] = (char)tolower((unsigned char)src[i]);
    dst[i] = '\0';
}

int ci_cmp(const char *a, const char *b) {
    while (*a && *b) {
        int x = tolower((unsigned char)*a), y = tolower((unsigned char)*b);
        if (x != y) return x - y;
        a++; b++;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}


/* ====================================================================
 *  UI HELPERS  (colours, banners, tables) - ASCII only, works everywhere
 * ==================================================================== */
int useColor = 1;
#define RESET  "\033[0m"
#define BOLD   "\033[1m"
#define DIM    "\033[2m"
#define RED    "\033[31m"
#define GREEN  "\033[32m"
#define YELLOW "\033[33m"
#define CYAN   "\033[36m"

void setc(const char *c) { if (useColor) fputs(c, stdout); }

void ui_init(void) {
#ifdef _WIN32
    HANDLE hd = GetStdHandle(STD_OUTPUT_HANDLE); DWORD mode = 0;
    if (hd == INVALID_HANDLE_VALUE || !GetConsoleMode(hd, &mode) ||
        !SetConsoleMode(hd, mode | 0x0004)) useColor = 0;
#endif
}
void ui_ok(const char *m)   { setc(GREEN);  printf("  [ OK ] "); setc(RESET); printf("%s\n", m); }
void ui_err(const char *m)  { setc(RED);    printf("  [ !! ] "); setc(RESET); printf("%s\n", m); }
void ui_info(const char *m) { setc(CYAN);   printf("  [ i  ] "); setc(RESET); printf("%s\n", m); }

void header(const char *t) {
    printf("\n"); setc(BOLD CYAN);
    printf("  %s\n  ", t);
    for (size_t i = 0; i < strlen(t); i++) putchar('-');
    setc(RESET); printf("\n\n");
}

void table_line(const int *w, int n) {
    printf("  +");
    for (int i = 0; i < n; i++) { for (int k = 0; k < w[i] + 2; k++) putchar('-'); putchar('+'); }
    printf("\n");
}

void pause_screen(void) {
    char b[16];
    setc(DIM); printf("\n  Press Enter to return to the menu..."); setc(RESET);
    if (!fgets(b, sizeof b, stdin)) b[0] = 0;
}

/* ====================================================================
 *  1. STANDARD TRIE  (inverted index)
 * ==================================================================== */
typedef struct Posting {
    int docId;
    int freq;
    struct Posting *next;
} Posting;

typedef struct TrieNode {
    struct TrieNode *child[ALPHA];
    Posting *post;
    int isEnd;
} TrieNode;

TrieNode *trieRoot = NULL;
int trieNodes = 0, trieWords = 0;

int cidx(char c) { return isdigit((unsigned char)c) ? 26 + (c - '0') : c - 'a'; }
char idxc(int i) { return i < 26 ? (char)('a' + i) : (char)('0' + i - 26); }

TrieNode *trie_new(void) {
    TrieNode *n = (TrieNode *)calloc(1, sizeof(TrieNode));
    trieNodes++;
    return n;
}

void trie_insert(const char *word, int docId) {
    TrieNode *cur = trieRoot;
    for (int i = 0; word[i]; i++) {
        int k = cidx(word[i]);
        if (!cur->child[k]) cur->child[k] = trie_new();
        cur = cur->child[k];
    }
    if (!cur->isEnd) { cur->isEnd = 1; trieWords++; }
    if (cur->post && cur->post->docId == docId) {
        cur->post->freq++;
    } else {
        Posting *p = (Posting *)malloc(sizeof(Posting));
        p->docId = docId; p->freq = 1; p->next = cur->post;
        cur->post = p;
    }
}

TrieNode *trie_find(const char *word) {
    TrieNode *cur = trieRoot;
    for (int i = 0; word[i]; i++) {
        if (!isalnum((unsigned char)word[i])) return NULL;
        cur = cur->child[cidx(word[i])];
        if (!cur) return NULL;
    }
    return cur;
}

void trie_free(TrieNode *n) {
    if (!n) return;
    for (int i = 0; i < ALPHA; i++) trie_free(n->child[i]);
    Posting *p = n->post;
    while (p) { Posting *t = p->next; free(p); p = t; }
    free(n);
}

/* tokenise a document and add every word to the trie */
void index_document(Document *d) {
    char w[64]; int n = 0;
    for (int i = 0; i <= d->len; i++) {
        unsigned char c = (i < d->len) ? (unsigned char)d->content[i] : ' ';
        if (isalnum(c)) { if (n < 63) w[n++] = (char)tolower(c); }
        else if (n) { w[n] = '\0'; trie_insert(w, d->id); n = 0; }
    }
}

void rebuild_trie(void) {
    trie_free(trieRoot);
    trieNodes = 0; trieWords = 0;
    trieRoot = trie_new();
    for (int i = 1; i < MAX_DOCS; i++)
        if (docs[i]) index_document(docs[i]);
}

/* DFS to list all words with a given prefix */
void trie_collect(TrieNode *n, char *buf, int depth, int *printed, int limit) {
    if (!n || *printed >= limit) return;
    if (n->isEnd) {
        int nd = 0;
        for (Posting *p = n->post; p; p = p->next) nd++;
        buf[depth] = '\0';
        printf("  | %-24s | %9d |\n", buf, nd);
        (*printed)++;
    }
    for (int i = 0; i < ALPHA; i++) {
        if (n->child[i]) {
            buf[depth] = idxc(i);
            trie_collect(n->child[i], buf, depth + 1, printed, limit);
        }
    }
}

/* ====================================================================
 *  2. AVL TREE  (title index)
 * ==================================================================== */
typedef struct AVL {
    char title[TITLE_LEN];
    int docId, height;
    struct AVL *left, *right;
} AVL;

AVL *avlRoot = NULL;

int h(AVL *n) { return n ? n->height : 0; }
int mx(int a, int b) { return a > b ? a : b; }
void upd(AVL *n) { n->height = 1 + mx(h(n->left), h(n->right)); }
int bal(AVL *n) { return n ? h(n->left) - h(n->right) : 0; }

AVL *rot_right(AVL *y) {
    AVL *x = y->left; y->left = x->right; x->right = y;
    upd(y); upd(x); return x;
}
AVL *rot_left(AVL *x) {
    AVL *y = x->right; x->right = y->left; y->left = x;
    upd(x); upd(y); return y;
}

AVL *rebalance(AVL *n) {
    upd(n);
    int b = bal(n);
    if (b > 1) {
        if (bal(n->left) < 0) n->left = rot_left(n->left);   /* LR */
        return rot_right(n);                                  /* LL */
    }
    if (b < -1) {
        if (bal(n->right) > 0) n->right = rot_right(n->right); /* RL */
        return rot_left(n);                                    /* RR */
    }
    return n;
}

AVL *avl_insert(AVL *n, const char *title, int id) {
    if (!n) {
        AVL *t = (AVL *)calloc(1, sizeof(AVL));
        strcpy(t->title, title); t->docId = id; t->height = 1;
        return t;
    }
    int c = ci_cmp(title, n->title);
    if (c < 0) n->left = avl_insert(n->left, title, id);
    else       n->right = avl_insert(n->right, title, id);
    return rebalance(n);
}

AVL *avl_min(AVL *n) { while (n->left) n = n->left; return n; }

AVL *avl_delete(AVL *n, const char *title) {
    if (!n) return NULL;
    int c = ci_cmp(title, n->title);
    if (c < 0) n->left = avl_delete(n->left, title);
    else if (c > 0) n->right = avl_delete(n->right, title);
    else {
        if (!n->left || !n->right) {
            AVL *t = n->left ? n->left : n->right;
            free(n);
            return t;
        }
        AVL *s = avl_min(n->right);
        strcpy(n->title, s->title); n->docId = s->docId;
        n->right = avl_delete(n->right, s->title);
    }
    return rebalance(n);
}

AVL *avl_search(AVL *n, const char *title, int *steps) {
    while (n) {
        (*steps)++;
        int c = ci_cmp(title, n->title);
        if (c == 0) return n;
        n = c < 0 ? n->left : n->right;
    }
    return NULL;
}

void avl_inorder(AVL *n) {
    if (!n) return;
    avl_inorder(n->left);
    printf("  | %3d | %-38.38s | %7d |\n", n->docId, n->title, docs[n->docId] ? docs[n->docId]->len : 0);
    avl_inorder(n->right);
}

void avl_prefix(AVL *n, const char *pre, int *found) {
    if (!n) return;
    avl_prefix(n->left, pre, found);
    if (ci_cmp(n->title, pre) >= 0) {
        char lt[TITLE_LEN], lp[TITLE_LEN];
        to_lower_copy(n->title, lt); to_lower_copy(pre, lp);
        if (strncmp(lt, lp, strlen(lp)) == 0) {
            printf("  | %3d | %-38.38s |\n", n->docId, n->title);
            (*found)++;
        }
    }
    avl_prefix(n->right, pre, found);
}

void avl_free(AVL *n) {
    if (!n) return;
    avl_free(n->left); avl_free(n->right); free(n);
}

/* ====================================================================
 *  3. PATTERN MATCHING  (Brute Force, KMP, Boyer-Moore)
 * ==================================================================== */
long cmpCount = 0;

int brute_force(const char *t, int n, const char *p, int m, int *first) {
    int count = 0;
    for (int i = 0; i <= n - m; i++) {
        int j = 0;
        while (j < m) {
            cmpCount++;
            if (t[i + j] != p[j]) break;
            j++;
        }
        if (j == m) { count++; if (*first < 0) *first = i; }
    }
    return count;
}

int kmp(const char *t, int n, const char *p, int m, int *first) {
    int *lps = (int *)calloc(m, sizeof(int));
    for (int i = 1, len = 0; i < m; ) {          /* build LPS table */
        if (p[i] == p[len]) lps[i++] = ++len;
        else if (len) len = lps[len - 1];
        else lps[i++] = 0;
    }
    int count = 0, i = 0, j = 0;
    while (i < n) {
        cmpCount++;
        if (t[i] == p[j]) {
            i++; j++;
            if (j == m) { count++; if (*first < 0) *first = i - m; j = lps[j - 1]; }
        } else if (j) j = lps[j - 1];
        else i++;
    }
    free(lps);
    return count;
}

int boyer_moore(const char *t, int n, const char *p, int m, int *first) {
    int bad[256];
    for (int i = 0; i < 256; i++) bad[i] = -1;
    for (int i = 0; i < m; i++) bad[(unsigned char)p[i]] = i;
    int count = 0, s = 0;
    while (s <= n - m) {
        int j = m - 1;
        while (j >= 0) {
            cmpCount++;
            if (p[j] != t[s + j]) break;
            j--;
        }
        if (j < 0) {
            count++; if (*first < 0) *first = s;
            s += (s + m < n) ? m - bad[(unsigned char)t[s + m]] : 1;
        } else {
            int sh = j - bad[(unsigned char)t[s + j]];
            s += sh > 1 ? sh : 1;
        }
    }
    return count;
}

const char *algoName[] = { "", "Brute Force", "KMP", "Boyer-Moore" };

int run_algo(int algo, const char *t, int n, const char *p, int m, int *first) {
    if (m == 0 || m > n) return 0;
    if (algo == 1) return brute_force(t, n, p, m, first);
    if (algo == 2) return kmp(t, n, p, m, first);
    return boyer_moore(t, n, p, m, first);
}

void print_snippet(const char *t, int n, int pos, int m) {
    int st = pos > 30 ? pos - 30 : 0;
    int e = pos + m + 30 < n ? pos + m + 30 : n;
    printf("        ");
    setc(DIM); printf("..."); setc(RESET);
    for (int i = st; i < e; i++) {
        if (i == pos) setc(BOLD YELLOW);
        if (i == pos + m) setc(RESET);
        putchar((t[i] == '\n' || t[i] == '\r') ? ' ' : t[i]);
    }
    setc(RESET); setc(DIM); printf("..."); setc(RESET); printf("\n");
}

/* ====================================================================
 *  4. MERGE SORT  (ranking)
 * ==================================================================== */
typedef struct { int id; int score; } Result;

int better(Result a, Result b) {            /* a should come before b */
    if (a.score != b.score) return a.score > b.score;
    return a.id < b.id;
}

void merge_sort(Result *a, Result *tmp, int lo, int hi) {
    if (lo >= hi) return;
    int mid = (lo + hi) / 2;
    merge_sort(a, tmp, lo, mid);
    merge_sort(a, tmp, mid + 1, hi);
    int i = lo, j = mid + 1, k = lo;
    while (i <= mid && j <= hi) tmp[k++] = better(a[j], a[i]) ? a[j++] : a[i++];
    while (i <= mid) tmp[k++] = a[i++];
    while (j <= hi)  tmp[k++] = a[j++];
    for (k = lo; k <= hi; k++) a[k] = tmp[k];
}

/* ====================================================================
 *  5. FILE ORGANIZATION  (binary file + text import)
 * ==================================================================== */
void save_docs(void) {
    FILE *f = fopen(DATA_FILE, "wb");
    if (!f) { ui_err("Could not write the data file."); return; }
    fwrite(&nextId, sizeof(int), 1, f);
    fwrite(&docCount, sizeof(int), 1, f);
    for (int i = 1; i < MAX_DOCS; i++) {
        if (!docs[i]) continue;
        fwrite(&docs[i]->id, sizeof(int), 1, f);
        fwrite(docs[i]->title, TITLE_LEN, 1, f);
        fwrite(&docs[i]->len, sizeof(int), 1, f);
        fwrite(docs[i]->content, 1, docs[i]->len, f);
    }
    fclose(f);
    { char m[128]; snprintf(m, sizeof m, "Saved %d document(s) to %s", docCount, DATA_FILE); ui_ok(m); }
}

int add_doc_internal(const char *title, char *content, int len, int id) {
    if (id < 0) id = nextId;
    if (id >= MAX_DOCS) { ui_err("Document limit reached."); free(content); return -1; }
    Document *d = (Document *)malloc(sizeof(Document));
    d->id = id;
    strncpy(d->title, title, TITLE_LEN - 1); d->title[TITLE_LEN - 1] = '\0';
    d->content = content; d->len = len;
    docs[id] = d;
    if (id >= nextId) nextId = id + 1;
    docCount++;
    avlRoot = avl_insert(avlRoot, d->title, id);
    index_document(d);
    return id;
}

void load_docs(void) {
    FILE *f = fopen(DATA_FILE, "rb");
    if (!f) return;
    int savedNext, count;
    if (fread(&savedNext, sizeof(int), 1, f) != 1 || fread(&count, sizeof(int), 1, f) != 1) { fclose(f); return; }
    for (int i = 0; i < count; i++) {
        int id, len; char title[TITLE_LEN];
        if (fread(&id, sizeof(int), 1, f) != 1) break;
        if (fread(title, TITLE_LEN, 1, f) != 1) break;
        if (fread(&len, sizeof(int), 1, f) != 1) break;
        char *c = (char *)malloc(len + 1);
        if ((int)fread(c, 1, len, f) != len) { free(c); break; }
        c[len] = '\0';
        add_doc_internal(title, c, len, id);
    }
    if (savedNext > nextId) nextId = savedNext;
    fclose(f);
    { char m[128]; snprintf(m, sizeof m, "Loaded %d saved document(s) from %s", docCount, DATA_FILE); ui_info(m); }
}

int title_exists(const char *title) {
    int steps = 0;
    return avl_search(avlRoot, title, &steps) != NULL;
}

/* ====================================================================
 *  MENU OPERATIONS
 * ==================================================================== */
int valid_id(int id) { return id > 0 && id < MAX_DOCS && docs[id]; }

void op_add(void) {
    header("ADD DOCUMENT");
    char title[TITLE_LEN];
    read_line("  Title : ", title, sizeof title);
    if (!title[0]) { ui_err("Title cannot be empty."); return; }
    if (title_exists(title)) { ui_err("A document with this title already exists."); return; }
    printf("  Content (type text, finish with a single '.' on its own line):\n\n");
    int cap = 256, len = 0; char *buf = (char *)malloc(cap); char line[1024];
    while (fgets(line, sizeof line, stdin)) {
        if (strcmp(line, ".\n") == 0 || strcmp(line, ".\r\n") == 0 || strcmp(line, ".") == 0) break;
        int l = (int)strlen(line);
        while (len + l + 1 > cap) { cap *= 2; buf = (char *)realloc(buf, cap); }
        memcpy(buf + len, line, l); len += l;
    }
    buf[len] = '\0';
    int id = add_doc_internal(title, buf, len, -1);
    if (id > 0) { char m[128]; snprintf(m, sizeof m, "Document added with ID %d and indexed.", id); printf("\n"); ui_ok(m); }
}

void op_import(void) {
    header("IMPORT FROM TEXT FILE");
    char path[256], title[TITLE_LEN];
    read_line("  File path : ", path, sizeof path);
    FILE *f = fopen(path, "rb");
    if (!f) { ui_err("Cannot open that file."); return; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc(sz + 1);
    int len = (int)fread(buf, 1, sz, f); buf[len] = '\0'; fclose(f);
    read_line("  Title (Enter = file name) : ", title, sizeof title);
    if (!title[0]) {
        const char *sl = strrchr(path, '/'); if (!sl) sl = strrchr(path, '\\');
        strncpy(title, sl ? sl + 1 : path, TITLE_LEN - 1); title[TITLE_LEN - 1] = '\0';
    }
    if (title_exists(title)) { ui_err("A document with this title already exists."); free(buf); return; }
    int id = add_doc_internal(title, buf, len, -1);
    if (id > 0) { char m[160]; snprintf(m, sizeof m, "Imported as ID %d (%d characters).", id, len); printf("\n"); ui_ok(m); }
}

void op_list(void) {
    header("ALL DOCUMENTS  (sorted by title - AVL in-order traversal)");
    if (!docCount) { ui_info("No documents yet. Use option 12 to load samples."); return; }
    int w[] = {3, 38, 7};
    table_line(w, 3);
    printf("  | %-3s | %-38s | %-7s |\n", "ID", "Title", "Chars");
    table_line(w, 3);
    avl_inorder(avlRoot);
    table_line(w, 3);
    printf("  %d document(s)\n", docCount);
}

void op_view(void) {
    header("VIEW DOCUMENT");
    int id = read_int("  Document ID : ");
    if (!valid_id(id)) { ui_err("No such document."); return; }
    printf("\n"); setc(BOLD); printf("  %s", docs[id]->title); setc(RESET);
    setc(DIM); printf("   (ID %d, %d chars)\n", id, docs[id]->len); setc(RESET);
    printf("  ----------------------------------------------------------\n");
    printf("%s\n", docs[id]->content);
}

void op_delete(void) {
    header("DELETE DOCUMENT");
    int id = read_int("  Document ID to delete : ");
    if (!valid_id(id)) { ui_err("No such document."); return; }
    char m[160]; snprintf(m, sizeof m, "Deleted \"%s\" (removed from AVL, trie rebuilt).", docs[id]->title);
    avlRoot = avl_delete(avlRoot, docs[id]->title);
    free(docs[id]->content); free(docs[id]); docs[id] = NULL; docCount--;
    rebuild_trie();
    ui_ok(m);
}

/* keyword search: AND of all words, ranked by total frequency */
void op_keyword(void) {
    header("KEYWORD SEARCH  (Trie inverted index + Merge Sort ranking)");
    char q[256], shown[256];
    read_line("  Keywords (all must appear) : ", q, sizeof q);
    strcpy(shown, q);
    int score[MAX_DOCS] = {0}, hits[MAX_DOCS] = {0}, nw = 0;
    char *tok = strtok(q, " \t");
    while (tok) {
        char w[64]; to_lower_copy(tok, w); nw++;
        TrieNode *n = trie_find(w);
        if (n && n->isEnd)
            for (Posting *p = n->post; p; p = p->next) { hits[p->docId]++; score[p->docId] += p->freq; }
        tok = strtok(NULL, " \t");
    }
    if (!nw) { ui_err("Empty query."); return; }
    Result *res = (Result *)malloc(sizeof(Result) * MAX_DOCS), *tmp = (Result *)malloc(sizeof(Result) * MAX_DOCS);
    int cnt = 0;
    for (int i = 1; i < MAX_DOCS; i++)
        if (docs[i] && hits[i] == nw) { res[cnt].id = i; res[cnt].score = score[i]; cnt++; }
    printf("\n");
    if (!cnt) { ui_info("No document contains all the keywords."); free(res); free(tmp); return; }
    merge_sort(res, tmp, 0, cnt - 1);
    int w[] = {4, 3, 34, 5, 12};
    table_line(w, 5);
    printf("  | %-4s | %-3s | %-34s | %-5s | %-12s |\n", "Rank", "ID", "Title", "Score", "Relevance");
    table_line(w, 5);
    int top = res[0].score;
    for (int i = 0; i < cnt; i++) {
        int bar = res[i].score * 12 / (top ? top : 1); if (bar < 1) bar = 1;
        char b[16]; int k; for (k = 0; k < bar; k++) b[k] = '#'; b[k] = 0;
        printf("  | %-4d | %-3d | %-34.34s | %-5d | ", i + 1, res[i].id, docs[res[i].id]->title, res[i].score);
        setc(GREEN); printf("%-12s", b); setc(RESET); printf(" |\n");
    }
    table_line(w, 5);
    printf("  %d result(s) for \"%s\"\n", cnt, shown);
    free(res); free(tmp);
}

void op_prefix(void) {
    header("PREFIX SEARCH / AUTOCOMPLETE  (Trie)");
    char q[64], w[64], buf[128];
    read_line("  Prefix : ", q, sizeof q);
    to_lower_copy(q, w);
    TrieNode *cur = trieRoot;
    for (int i = 0; w[i] && cur; i++) {
        if (!isalnum((unsigned char)w[i])) { cur = NULL; break; }
        cur = cur->child[cidx(w[i])];
    }
    printf("\n");
    if (!w[0] || !cur) { ui_info("No words match that prefix."); return; }
    strcpy(buf, w);
    int ws[] = {24, 9};
    table_line(ws, 2);
    printf("  | %-24s | %-9s |\n", "Word", "Documents");
    table_line(ws, 2);
    int printed = 0;
    trie_collect(cur, buf, (int)strlen(w), &printed, 20);
    table_line(ws, 2);
    printf("  showing up to 20 suggestions\n");
}

int choose_algo(void) {
    printf("\n    1  Brute Force\n    2  Knuth-Morris-Pratt (KMP)\n    3  Boyer-Moore\n\n");
    int a = read_int("  Choose algorithm [default 2] : ");
    return (a >= 1 && a <= 3) ? a : 2;
}

void op_pattern(void) {
    header("PHRASE SEARCH  (pattern matching inside document text)");
    char p[256], lp[256];
    read_line("  Phrase / pattern : ", p, sizeof p);
    if (!p[0]) return;
    to_lower_copy(p, lp);
    int algo = choose_algo(), m = (int)strlen(lp), total = 0, docsHit = 0;
    cmpCount = 0;
    printf("\n  Algorithm: "); setc(BOLD); printf("%s\n\n", algoName[algo]); setc(RESET);
    for (int i = 1; i < MAX_DOCS; i++) {
        if (!docs[i]) continue;
        char *lt = (char *)malloc(docs[i]->len + 1);
        to_lower_copy(docs[i]->content, lt);
        int first = -1, c = run_algo(algo, lt, docs[i]->len, lp, m, &first);
        if (c) {
            setc(BOLD); printf("  [ID %d] %s", i, docs[i]->title); setc(RESET);
            printf("  -  %d match(es)\n", c);
            print_snippet(docs[i]->content, docs[i]->len, first, m);
            printf("\n");
            total += c; docsHit++;
        }
        free(lt);
    }
    if (!total) ui_info("Pattern not found in any document.");
    printf("  Summary: %d match(es) in %d document(s) | %ld character comparisons\n", total, docsHit, cmpCount);
}

void compare_pass(int a, const char *lp, int m, long *total) {
    *total = 0;
    for (int i = 1; i < MAX_DOCS; i++) {
        if (!docs[i]) continue;
        char *lt = (char *)malloc(docs[i]->len + 1);
        to_lower_copy(docs[i]->content, lt);
        int first = -1;
        *total += run_algo(a, lt, docs[i]->len, lp, m, &first);
        free(lt);
    }
}

void op_compare(void) {
    header("ALGORITHM COMPARISON  (Brute Force vs KMP vs Boyer-Moore)");
    char p[256], lp[256];
    read_line("  Pattern to benchmark : ", p, sizeof p);
    if (!p[0]) return;
    to_lower_copy(p, lp);
    int m = (int)strlen(lp);
    long cm[4], tot[4]; double ms[4]; long top = 1;
    for (int a = 1; a <= 3; a++) {
        cmpCount = 0;
        compare_pass(a, lp, m, &tot[a]);
        cm[a] = cmpCount; if (cm[a] > top) top = cm[a];
        clock_t st = clock();
        for (int rep = 0; rep < 200; rep++) compare_pass(a, lp, m, &tot[a]);
        ms[a] = (double)(clock() - st) * 1000.0 / CLOCKS_PER_SEC;
    }
    printf("\n");
    int w[] = {12, 7, 11, 8, 20};
    table_line(w, 5);
    printf("  | %-12s | %-7s | %-11s | %-8s | %-20s |\n", "Algorithm", "Matches", "Comparisons", "Time ms", "Work done");
    table_line(w, 5);
    for (int a = 1; a <= 3; a++) {
        int bar = (int)(cm[a] * 20 / top); if (bar < 1) bar = 1;
        char b[24]; int k; for (k = 0; k < bar; k++) b[k] = '#'; b[k] = 0;
        printf("  | %-12s | %-7ld | %-11ld | %-8.2f | ", algoName[a], tot[a], cm[a], ms[a]);
        setc(a == 3 ? GREEN : (a == 2 ? YELLOW : RED)); printf("%-20s", b); setc(RESET); printf(" |\n");
    }
    table_line(w, 5);
    printf("  Time covers 200 repeated passes over all documents.\n");
}

void op_title(void) {
    header("TITLE SEARCH  (AVL tree)");
    char t[TITLE_LEN];
    read_line("  Title or starting letters : ", t, sizeof t);
    int steps = 0;
    AVL *n = avl_search(avlRoot, t, &steps);
    printf("\n");
    if (n) { char m[200]; snprintf(m, sizeof m, "Exact match: ID %d - %s  (found in %d AVL step(s))", n->docId, n->title, steps); ui_ok(m); }
    else   { char m[100]; snprintf(m, sizeof m, "No exact match (%d AVL step(s) taken).", steps); ui_info(m); }
    printf("\n  Titles starting with \"%s\":\n", t);
    int w[] = {3, 38};
    table_line(w, 2);
    int found = 0;
    avl_prefix(avlRoot, t, &found);
    if (!found) printf("  | %-45s |\n", "none");
    table_line(w, 2);
}

void op_stats(void) {
    header("SYSTEM STATISTICS");
    long chars = 0;
    for (int i = 1; i < MAX_DOCS; i++) if (docs[i]) chars += docs[i]->len;
    int w[] = {30, 12};
    table_line(w, 2);
    printf("  | %-30s | %-12ld |\n", "Documents stored", (long)docCount);
    printf("  | %-30s | %-12ld |\n", "Total characters", chars);
    printf("  | %-30s | %-12ld |\n", "Unique words indexed (trie)", (long)trieWords);
    printf("  | %-30s | %-12ld |\n", "Trie nodes", (long)trieNodes);
    printf("  | %-30s | %-12ld |\n", "AVL tree height", (long)h(avlRoot));
    table_line(w, 2);
}

void op_sample(void) {
    header("LOAD SAMPLE DOCUMENTS");
    const char *titles[] = {
        "Introduction to AVL Trees",
        "Understanding Tries",
        "String Matching Algorithms",
        "Hashing and Collision Resolution",
        "File Organization Basics"
    };
    const char *texts[] = {
        "An AVL tree is a self balancing binary search tree. After every insertion or deletion the heights "
        "of the two child subtrees differ by at most one. Rotations restore balance, so search, insert and "
        "delete all take logarithmic time.\n",
        "A trie is a tree used for storing strings. Each edge represents a character and every path from the "
        "root spells a word. Tries make prefix search and autocomplete very fast, and search time depends only "
        "on the length of the word.\n",
        "String matching finds a pattern inside a text. Brute force checks every position. The KMP algorithm "
        "uses a prefix table to avoid rechecking characters. Boyer Moore compares from the right and skips "
        "ahead using the bad character rule, which is fast in practice for search in documents.\n",
        "Hashing maps a key to a table index using a hash function. When two keys map to the same index a "
        "collision occurs. Collision resolution uses linear probing, quadratic probing, double hashing or "
        "chaining with linked lists.\n",
        "File organization describes how records are stored on disk. A file can be sequential, indexed or "
        "hashed. Text files store characters while binary files store raw bytes. Indexing a file makes "
        "document search faster because the index points directly to the record.\n"
    };
    int added = 0;
    for (int i = 0; i < 5; i++) {
        if (title_exists(titles[i])) continue;
        int len = (int)strlen(texts[i]);
        char *c = (char *)malloc(len + 1); memcpy(c, texts[i], len + 1);
        if (add_doc_internal(titles[i], c, len, -1) > 0) added++;
    }
    char m[100]; snprintf(m, sizeof m, "Added %d sample document(s).", added); ui_ok(m);
}

/* ====================================================================
 *  MAIN
 * ==================================================================== */
void clear_screen(void) { if (useColor) printf("\033[2J\033[H"); else printf("\n\n"); }

void menu(void) {
    clear_screen();
    setc(BOLD CYAN);
    printf("\n  +==========================================================+\n");
    printf("  |            DIGITAL DOCUMENT SEARCH SYSTEM                 |\n");
    printf("  |     Advanced Data Structures in C  -  PBL Mini Project   |\n");
    printf("  +==========================================================+\n");
    setc(RESET);

#define ITEM(n, label, tag) do { setc(BOLD); printf("   %2d  ", n); setc(RESET); \
        printf("%-34s", label); setc(DIM); printf("%s\n", tag); setc(RESET); } while (0)
#define SECTION(t) do { setc(YELLOW); printf("\n   %s\n", t); setc(RESET); } while (0)

    SECTION("DOCUMENTS");
    ITEM(1,  "Add document",              "[File organization]");
    ITEM(2,  "Import from text file",     "[Text file I/O]");
    ITEM(3,  "List all documents",        "[AVL in-order]");
    ITEM(4,  "View a document",           "");
    ITEM(5,  "Delete a document",         "[AVL delete + trie rebuild]");
    SECTION("SEARCH");
    ITEM(6,  "Keyword search",            "[Trie + Merge Sort]");
    ITEM(7,  "Prefix search / autocomplete", "[Trie]");
    ITEM(8,  "Phrase search",             "[Brute Force / KMP / Boyer-Moore]");
    ITEM(9,  "Compare algorithms",        "[Pattern matching analysis]");
    ITEM(10, "Search by title",           "[AVL search]");
    SECTION("SYSTEM");
    ITEM(11, "Statistics",                "");
    ITEM(12, "Load sample documents",     "");
    ITEM(13, "Save to file",              "[Binary file]");
    ITEM(0,  "Save and exit",             "");

    setc(DIM);
    printf("\n  ----------------------------------------------------------\n");
    printf("   Documents: %d   |   Words indexed: %d   |   AVL height: %d\n", docCount, trieWords, h(avlRoot));
    printf("  ----------------------------------------------------------\n");
    setc(RESET);
}

int main(void) {
    ui_init();
    trieRoot = trie_new();
    load_docs();
    int ch;
    do {
        menu();
        ch = read_int("\n  Enter choice > ");
        switch (ch) {
            case 1:  op_add();     break;
            case 2:  op_import();  break;
            case 3:  op_list();    break;
            case 4:  op_view();    break;
            case 5:  op_delete();  break;
            case 6:  op_keyword(); break;
            case 7:  op_prefix();  break;
            case 8:  op_pattern(); break;
            case 9:  op_compare(); break;
            case 10: op_title();   break;
            case 11: op_stats();   break;
            case 12: op_sample();  break;
            case 13: save_docs();  break;
            case 0:  header("SAVING"); save_docs(); printf("\n  Goodbye!\n\n"); break;
            default: ui_err("Invalid choice.");
        }
        if (ch != 0) pause_screen();
    } while (ch != 0);
    return 0;
}
