#include "fcntl.h"
#include "io.h"
#include "stat.h"
#include "types.h"
#include "user.h"

#define PI 3.1415926535f
#define LOG_10000 9.2103403719f

typedef struct {
    int dim;
    int hidden_dim;
    int n_layers;
    int n_heads;
    int n_kv_heads;
    int vocab_size;
    int seq_len;
} config_s;

typedef struct {
    float* token_embedding_table;
    float* rms_att_weight;
    float* rms_ffn_weight;
    float* wq;
    float* wk;
    float* wv;
    float* wo;
    float* w1;
    float* w2;
    float* w3;
    float* rms_final_weight;
    float* wcls;
} weights_s;

typedef struct {
    float* x;
    float* xb;
    float* xb2;
    float* hb;
    float* hb2;
    float* q;
    float* k;
    float* v;
    float* att;
    float* logits;
    float* key_cache;
    float* value_cache;
} state_s;

typedef struct {
    config_s config;
    weights_s weights;
    state_s state;
    float* data;
} transformer_s;

typedef struct {
    char** vocab;
    float* vocab_scores;
    int vocab_size;
    int max_token_length;
    char byte_pieces[512];
} tokenizer_s;

float absf(float x) { return x < 0.0f ? -x : x; }

float sqrt_approx(float x) {
    float r;

    if (x <= 0.0f)
        return 0.0f;
    r = x > 1.0f ? x : 1.0f;
    for (int i = 0; i < 12; i++)
        r = 0.5f * (r + x / r);
    return r;
}

float exp_unit(float x) {
    float term = 1.0f;
    float sum = 1.0f;

    for (int i = 1; i <= 10; i++) {
        term *= x / i;
        sum += term;
    }
    return sum;
}

float exp_approx(float x) {
    int neg = 0;
    int scale = 0;
    float y;

    if (x < 0.0f) {
        neg = 1;
        x = -x;
    }
    if (x > 20.0f)
        x = 20.0f;
    while (x > 1.0f) {
        x *= 0.5f;
        scale++;
    }
    y = exp_unit(x);
    while (scale-- > 0)
        y *= y;
    return neg ? 1.0f / y : y;
}

float sin_approx(float x) {
    float x2;

    while (x > PI)
        x -= 2.0f * PI;
    while (x < -PI)
        x += 2.0f * PI;
    x2 = x * x;

    float t1 = 1.0f / 5040.0f;          // 1/7!
    float t2 = 1.0f / 120.0f - x2 * t1; // 1/5! - x²/7!
    float t3 = 1.0f / 6.0f - x2 * t2;   // 1/3! - x²*(...)

    return x * (1.0f - x2 * t3);
}

float cos_approx(float x) {
    float x2;

    while (x > PI)
        x -= 2.0f * PI;
    while (x < -PI)
        x += 2.0f * PI;
    x2 = x * x;
    return 1.0f -
           x2 * (1.0f / 2.0f - x2 * (1.0f / 24.0f - x2 * (1.0f / 720.0f)));
}

int printable(char c) { return c >= 32 && c <= 126; }

int whitespace(char c) {
    return c == '\n' || c == '\r' || c == '\t' || c == ' ';
}

void map_weights(weights_s* w, config_s* p, float* ptr, int shared_weights) {
    int head_size = p->dim / p->n_heads;
    int kv_dim = (p->dim * p->n_kv_heads) / p->n_heads;
    uint64 n_layers = p->n_layers;

    w->token_embedding_table = ptr;
    ptr += p->vocab_size * p->dim;
    w->rms_att_weight = ptr;
    ptr += n_layers * p->dim;
    w->wq = ptr;
    ptr += n_layers * p->dim * p->dim;
    w->wk = ptr;
    ptr += n_layers * p->dim * kv_dim;
    w->wv = ptr;
    ptr += n_layers * p->dim * kv_dim;
    w->wo = ptr;
    ptr += n_layers * p->dim * p->dim;
    w->rms_ffn_weight = ptr;
    ptr += n_layers * p->dim;
    w->w1 = ptr;
    ptr += n_layers * p->dim * p->hidden_dim;
    w->w2 = ptr;
    ptr += n_layers * p->hidden_dim * p->dim;
    w->w3 = ptr;
    ptr += n_layers * p->dim * p->hidden_dim;
    w->rms_final_weight = ptr;
    ptr += p->dim;
    ptr += p->seq_len * head_size / 2;
    ptr += p->seq_len * head_size / 2;
    w->wcls = shared_weights ? w->token_embedding_table : ptr;
}

void alloc_state(state_s* s, config_s* p) {
    int kv_dim = (p->dim * p->n_kv_heads) / p->n_heads;
    uint64 cache_bytes = p->n_layers * p->seq_len * kv_dim * sizeof(float);

    fprintf(
        2,
        "llminfer: alloc state dim=%d hidden=%d layers=%d seq=%d kv_cache=%d "
        "KiB\n",
        p->dim,
        p->hidden_dim,
        p->n_layers,
        p->seq_len,
        (int)((cache_bytes * 2) / 1024)
    );

    s->x = xmalloc(p->dim * sizeof(float));
    s->xb = xmalloc(p->dim * sizeof(float));
    s->xb2 = xmalloc(p->dim * sizeof(float));
    s->hb = xmalloc(p->hidden_dim * sizeof(float));
    s->hb2 = xmalloc(p->hidden_dim * sizeof(float));
    s->q = xmalloc(p->dim * sizeof(float));
    s->key_cache = xmalloc(p->n_layers * p->seq_len * kv_dim * sizeof(float));
    s->value_cache = xmalloc(p->n_layers * p->seq_len * kv_dim * sizeof(float));
    s->att = xmalloc(p->n_heads * p->seq_len * sizeof(float));
    s->logits = xmalloc(p->vocab_size * sizeof(float));
    fprintf(2, "llminfer: state ready\n");
}

void read_checkpoint(transformer_s* t, char* path) {
    struct stat st;
    int fd = open(path, O_RDONLY);
    int shared_weights;
    uint data_size;

    if (fd < 0) {
        fprintf(2, "llminfer: cannot open %s\n", path);
        exit(1);
    }
    if (fstat(fd, &st) < 0) {
        fprintf(2, "llminfer: cannot stat %s\n", path);
        exit(1);
    }
    fprintf(
        2,
        "llminfer: open checkpoint %s (%d KiB)\n",
        path,
        (int)(st.size / 1024)
    );
    if (read_exact(fd, &t->config, sizeof(config_s)) < 0) {
        fprintf(2, "llminfer: bad checkpoint header\n");
        exit(1);
    }
    shared_weights = t->config.vocab_size > 0;
    if (t->config.vocab_size < 0)
        t->config.vocab_size = -t->config.vocab_size;
    fprintf(
        2,
        "llminfer: config dim=%d hidden=%d layers=%d heads=%d kv_heads=%d "
        "vocab=%d seq=%d shared=%d\n",
        t->config.dim,
        t->config.hidden_dim,
        t->config.n_layers,
        t->config.n_heads,
        t->config.n_kv_heads,
        t->config.vocab_size,
        t->config.seq_len,
        shared_weights
    );

    data_size = (uint)(st.size - sizeof(config_s));
    fprintf(
        2,
        "llminfer: alloc checkpoint weights %d KiB\n",
        (int)(data_size / 1024)
    );
    t->data = xmalloc(data_size);
    if (read_exact_progress(fd, t->data, data_size, "llminfer", "checkpoint") <
        0) {
        fprintf(2, "llminfer: short checkpoint read\n");
        exit(1);
    }
    close(fd);

    map_weights(&t->weights, &t->config, t->data, shared_weights);
    fprintf(2, "llminfer: weights mapped\n");
    alloc_state(&t->state, &t->config);
}

void rmsnorm(float* out, float* x, float* weight, int n) {
    float ss = 0.0f;

    for (int i = 0; i < n; i++)
        ss += x[i] * x[i];
    ss = 1.0f / sqrt_approx(ss / n + 1e-5f);
    for (int i = 0; i < n; i++)
        out[i] = weight[i] * (ss * x[i]);
}

void matmul(float* out, float* x, float* w, int n, int d) {
    for (int i = 0; i < d; i++) {
        float v = 0.0f;

        for (int j = 0; j < n; j++)
            v += w[i * n + j] * x[j];
        out[i] = v;
    }
}

void softmax(float* x, int n) {
    float max = x[0];
    float sum = 0.0f;

    for (int i = 1; i < n; i++) {
        if (x[i] > max)
            max = x[i];
    }
    for (int i = 0; i < n; i++) {
        x[i] = exp_approx(x[i] - max);
        sum += x[i];
    }
    for (int i = 0; i < n; i++)
        x[i] /= sum;
}

float* forward(transformer_s* t, int token, int pos) {
    config_s* p = &t->config;
    weights_s* w = &t->weights;
    state_s* s = &t->state;
    int dim = p->dim;
    int hidden_dim = p->hidden_dim;
    int head_size = dim / p->n_heads;
    int kv_dim = (p->dim * p->n_kv_heads) / p->n_heads;
    int kv_mul = p->n_heads / p->n_kv_heads;
    float* x = s->x;

    memcpy(x, w->token_embedding_table + token * dim, dim * sizeof(float));

    for (int l = 0; l < p->n_layers; l++) {
        int loff = l * p->seq_len * kv_dim;

        rmsnorm(s->xb, x, w->rms_att_weight + l * dim, dim);
        s->k = s->key_cache + loff + pos * kv_dim;
        s->v = s->value_cache + loff + pos * kv_dim;

        matmul(s->q, s->xb, w->wq + l * dim * dim, dim, dim);
        matmul(s->k, s->xb, w->wk + l * dim * kv_dim, dim, kv_dim);
        matmul(s->v, s->xb, w->wv + l * dim * kv_dim, dim, kv_dim);

        for (int i = 0; i < dim; i += 2) {
            int head_dim = i % head_size;
            float freq = exp_approx(-LOG_10000 * head_dim / (float)head_size);
            float val = pos * freq;
            float fcr = cos_approx(val);
            float fci = sin_approx(val);
            int rotn = i < kv_dim ? 2 : 1;

            for (int v = 0; v < rotn; v++) {
                float* vec = v == 0 ? s->q : s->k;
                float v0 = vec[i];
                float v1 = vec[i + 1];

                vec[i] = v0 * fcr - v1 * fci;
                vec[i + 1] = v0 * fci + v1 * fcr;
            }
        }

        for (int h = 0; h < p->n_heads; h++) {
            float* q = s->q + h * head_size;
            float* att = s->att + h * p->seq_len;
            float* xb = s->xb + h * head_size;

            for (int ts = 0; ts <= pos; ts++) {
                float* k = s->key_cache + loff + ts * kv_dim +
                           (h / kv_mul) * head_size;
                float score = 0.0f;

                for (int i = 0; i < head_size; i++)
                    score += q[i] * k[i];
                att[ts] = score / sqrt_approx(head_size);
            }
            softmax(att, pos + 1);
            memset(xb, 0, head_size * sizeof(float));
            for (int ts = 0; ts <= pos; ts++) {
                float* v = s->value_cache + loff + ts * kv_dim +
                           (h / kv_mul) * head_size;
                float a = att[ts];

                for (int i = 0; i < head_size; i++)
                    xb[i] += a * v[i];
            }
        }

        matmul(s->xb2, s->xb, w->wo + l * dim * dim, dim, dim);
        for (int i = 0; i < dim; i++)
            x[i] += s->xb2[i];

        rmsnorm(s->xb, x, w->rms_ffn_weight + l * dim, dim);
        matmul(s->hb, s->xb, w->w1 + l * dim * hidden_dim, dim, hidden_dim);
        matmul(s->hb2, s->xb, w->w3 + l * dim * hidden_dim, dim, hidden_dim);
        for (int i = 0; i < hidden_dim; i++) {
            float v = s->hb[i];

            v *= 1.0f / (1.0f + exp_approx(-v));
            s->hb[i] = v * s->hb2[i];
        }
        matmul(s->xb, s->hb, w->w2 + l * dim * hidden_dim, hidden_dim, dim);
        for (int i = 0; i < dim; i++)
            x[i] += s->xb[i];
    }

    rmsnorm(x, x, w->rms_final_weight, dim);
    matmul(s->logits, x, w->wcls, p->dim, p->vocab_size);
    return s->logits;
}

int sample_argmax(float* logits, int n) {
    int best = 0;
    float best_v = logits[0];

    for (int i = 1; i < n; i++) {
        if (logits[i] > best_v) {
            best = i;
            best_v = logits[i];
        }
    }
    return best;
}

int str_eq_len(char* a, char* b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i])
            return 0;
    }
    return a[n] == 0;
}

int vocab_lookup(tokenizer_s* t, char* s) {
    for (int i = 0; i < t->vocab_size; i++) {
        if (strcmp(t->vocab[i], s) == 0)
            return i;
    }
    return -1;
}

void read_tokenizer(tokenizer_s* t, char* path, int vocab_size) {
    int fd = open(path, O_RDONLY);

    if (fd < 0) {
        fprintf(2, "llminfer: cannot open %s\n", path);
        exit(1);
    }
    fprintf(2, "llminfer: open tokenizer %s vocab=%d\n", path, vocab_size);
    t->vocab_size = vocab_size;
    t->vocab = xmalloc(vocab_size * sizeof(char*));
    t->vocab_scores = xmalloc(vocab_size * sizeof(float));
    for (int i = 0; i < 256; i++) {
        t->byte_pieces[i * 2] = i;
        t->byte_pieces[i * 2 + 1] = 0;
    }
    if (read_exact(fd, &t->max_token_length, sizeof(int)) < 0)
        goto bad;
    for (int i = 0; i < vocab_size; i++) {
        int len;

        if (read_exact(fd, &t->vocab_scores[i], sizeof(float)) < 0)
            goto bad;
        if (read_exact(fd, &len, sizeof(int)) < 0)
            goto bad;
        t->vocab[i] = xmalloc(len + 1);
        if (read_exact(fd, t->vocab[i], len) < 0)
            goto bad;
        t->vocab[i][len] = 0;
        if ((i + 1) % 4096 == 0 || i + 1 == vocab_size)
            fprintf(
                2, "llminfer: tokenizer %d/%d entries\n", i + 1, vocab_size
            );
    }
    close(fd);
    fprintf(2, "llminfer: tokenizer ready max_token=%d\n", t->max_token_length);
    return;

bad:
    fprintf(2, "llminfer: bad tokenizer\n");
    exit(1);
}

int hex_val(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

char* decode(tokenizer_s* t, int prev_token, int token) {
    char* piece = t->vocab[token];

    if (prev_token == 1 && piece[0] == ' ')
        piece++;
    if (piece[0] == '<' && piece[1] == '0' && piece[2] == 'x' &&
        piece[5] == '>' && piece[6] == 0) {
        int hi = hex_val(piece[3]);
        int lo = hex_val(piece[4]);

        if (hi >= 0 && lo >= 0)
            piece = t->byte_pieces + (hi * 16 + lo) * 2;
    }
    return piece;
}

void safe_print(char* piece) {
    if (piece == 0 || piece[0] == 0)
        return;
    if (piece[1] == 0 && !printable(piece[0]) && !whitespace(piece[0]))
        return;
    printf("%s", piece);
}

void encode(tokenizer_s* t, char* text, int* tokens, int* n_tokens) {
    char piece[8];
    int dummy;

    *n_tokens = 0;
    tokens[(*n_tokens)++] = 1;
    dummy = vocab_lookup(t, " ");
    if (text[0] != 0 && dummy >= 0)
        tokens[(*n_tokens)++] = dummy;

    for (int i = 0; text[i]; i++) {
        int len = 1;
        int id;

        if (((uchar)text[i] & 0x80) != 0) {
            if (((uchar)text[i] & 0xE0) == 0xC0)
                len = 2;
            else if (((uchar)text[i] & 0xF0) == 0xE0)
                len = 3;
            else if (((uchar)text[i] & 0xF8) == 0xF0)
                len = 4;
        }
        memmove(piece, text + i, len);
        piece[len] = 0;
        id = vocab_lookup(t, piece);
        if (id >= 0) {
            tokens[(*n_tokens)++] = id;
        } else {
            for (int j = 0; j < len; j++)
                tokens[(*n_tokens)++] = (uchar)text[i + j] + 3;
        }
        i += len - 1;
    }

    while (1) {
        float best_score = -1e10f;
        int best_id = -1;
        int best_idx = -1;
        char merged[256];

        for (int i = 0; i < *n_tokens - 1; i++) {
            int len =
                strlen(t->vocab[tokens[i]]) + strlen(t->vocab[tokens[i + 1]]);
            int id;

            if (len >= sizeof(merged))
                continue;
            strcpy(merged, t->vocab[tokens[i]]);
            strcpy(merged + strlen(merged), t->vocab[tokens[i + 1]]);
            id = vocab_lookup(t, merged);
            if (id >= 0 && t->vocab_scores[id] > best_score) {
                best_score = t->vocab_scores[id];
                best_id = id;
                best_idx = i;
            }
        }
        if (best_idx < 0)
            break;
        tokens[best_idx] = best_id;
        for (int i = best_idx + 1; i < *n_tokens - 1; i++)
            tokens[i] = tokens[i + 1];
        (*n_tokens)--;
    }
}

void generate(
    transformer_s* t,
    tokenizer_s* tokenizer,
    char* prompt,
    int steps
) {
    int* prompt_tokens = xmalloc((strlen(prompt) + 3) * sizeof(int));
    int n_prompt_tokens = 0;
    int token;
    int next = 0;
    int pos = 0;
    int generated = 0;

    encode(tokenizer, prompt, prompt_tokens, &n_prompt_tokens);
    token = prompt_tokens[0];
    while (pos < steps) {
        float* logits = forward(t, token, pos);

        if (pos < n_prompt_tokens - 1)
            next = prompt_tokens[pos + 1];
        else
            next = sample_argmax(logits, t->config.vocab_size);
        pos++;
        if (next == 1)
            break;
        if (pos >= n_prompt_tokens) {
            char* piece = decode(tokenizer, token, next);

            safe_print(piece);
            generated++;
        }
        token = next;
    }
    printf("\n");
    fprintf(2, "llminfer: summary steps=%d generated=%d\n", pos, generated);
    free(prompt_tokens);
}

void usage(void) {
    fprintf(2, "usage: llminfer checkpoint tokenizer [steps] [prompt]\n");
    exit(1);
}

int main(int argc, char** argv) {
    transformer_s transformer;
    tokenizer_s tokenizer;
    int steps = 32;
    char* prompt = "";

    if (argc < 3)
        usage();
    if (argc >= 4)
        steps = atoi(argv[3]);
    if (argc >= 5)
        prompt = argv[4];

    memset(&transformer, 0, sizeof(transformer));
    memset(&tokenizer, 0, sizeof(tokenizer));
    read_checkpoint(&transformer, argv[1]);
    if (steps <= 0 || steps > transformer.config.seq_len)
        steps = transformer.config.seq_len;
    read_tokenizer(&tokenizer, argv[2], transformer.config.vocab_size);
    generate(&transformer, &tokenizer, prompt, steps);
    exit(0);
}
