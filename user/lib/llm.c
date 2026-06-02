#include "llm.h"
#include "ctype.h"
#include "fcntl.h"
#include "io.h"
#include "math.h"
#include "stat.h"
#include "types.h"
#include "user.h"

// MARK: - helpers

static uint64 checked_mul(uint64 a, uint64 b) {
    if (a != 0 && b > ((uint64)-1) / a) {
        fprintf(2, "llm: size overflow\n");
        exit(1);
    }
    return a * b;
}

static uint checked_bytes(uint64 count, uint item_size) {
    uint64 bytes = checked_mul(count, item_size);

    if (bytes > (uint64)0xffffffff) {
        fprintf(2, "llm: allocation too large\n");
        exit(1);
    }
    return (uint)bytes;
}

static void freep(void* p) {
    if (p)
        free(p);
}

static void* xreserve(uint n) {
    void* p = malloc(n);

    if (p == 0) {
        fprintf(2, "malloc failed\n");
        exit(1);
    }
    return p;
}

// MARK: - model loader

static void map_weights(llm_model_t* m) {
    llm_config_t* p = &m->config;
    llm_weights_t* w = &m->weights;
    float* ptr = m->weight_data;
    uint64 layers = p->n_layers;
    int head_size = p->dim / p->n_heads;
    int kv_dim = p->n_kv_heads * head_size;

    w->wq = ptr;
    ptr += checked_mul(checked_mul(layers, p->dim), p->dim);
    w->wk = ptr;
    ptr += checked_mul(checked_mul(layers, p->dim), kv_dim);
    w->wv = ptr;
    ptr += checked_mul(checked_mul(layers, p->dim), kv_dim);
    w->wo = ptr;
    ptr += checked_mul(checked_mul(layers, p->dim), p->dim);
    w->w1 = ptr;
    ptr += checked_mul(checked_mul(layers, p->hidden_dim), p->dim);
    w->w2 = ptr;
    ptr += checked_mul(checked_mul(layers, p->dim), p->hidden_dim);
    w->w3 = ptr;
    ptr += checked_mul(checked_mul(layers, p->hidden_dim), p->dim);
    w->rms_att_weight = ptr;
    ptr += checked_mul(layers, p->dim);
    w->rms_ffn_weight = ptr;
    ptr += checked_mul(layers, p->dim);
    w->rms_final_weight = ptr;
    ptr += p->dim;
    if (!p->shared_classifier) {
        w->wcls = ptr;
        ptr += checked_mul(p->vocab_size, p->dim);
    }
    w->token_embedding_table = ptr;
    if (p->shared_classifier)
        w->wcls = w->token_embedding_table;
}

static uint64 expected_weight_count(llm_config_t* p) {
    int head_size = p->dim / p->n_heads;
    int kv_dim = p->n_kv_heads * head_size;
    uint64 layers = p->n_layers;
    uint64 n = 0;

    n += checked_mul(checked_mul(layers, p->dim), p->dim);
    n += checked_mul(checked_mul(layers, p->dim), kv_dim);
    n += checked_mul(checked_mul(layers, p->dim), kv_dim);
    n += checked_mul(checked_mul(layers, p->dim), p->dim);
    n += checked_mul(checked_mul(layers, p->hidden_dim), p->dim);
    n += checked_mul(checked_mul(layers, p->dim), p->hidden_dim);
    n += checked_mul(checked_mul(layers, p->hidden_dim), p->dim);
    n += checked_mul(layers, p->dim);
    n += checked_mul(layers, p->dim);
    n += p->dim;
    if (!p->shared_classifier)
        n += checked_mul(p->vocab_size, p->dim);
    n += checked_mul(p->vocab_size, p->dim);
    return n;
}

static int alloc_state(llm_state_t* s, llm_config_t* p) {
    int head_size = p->dim / p->n_heads;
    int kv_dim = p->n_kv_heads * head_size;
    uint64 cache_items =
        checked_mul(checked_mul(p->n_layers, p->seq_len), kv_dim);

    s->x = xmalloc(checked_bytes(p->dim, sizeof(float)));
    s->xb = xmalloc(checked_bytes(p->dim, sizeof(float)));
    s->xb2 = xmalloc(checked_bytes(p->dim, sizeof(float)));
    s->hb = xmalloc(checked_bytes(p->hidden_dim, sizeof(float)));
    s->hb2 = xmalloc(checked_bytes(p->hidden_dim, sizeof(float)));
    s->q = xmalloc(checked_bytes(p->dim, sizeof(float)));
    s->att =
        xmalloc(checked_bytes((uint64)p->n_heads * p->seq_len, sizeof(float)));
    s->logits = xmalloc(checked_bytes(p->vocab_size, sizeof(float)));
    s->key_cache = xreserve(checked_bytes(cache_items, sizeof(float)));
    s->value_cache = xreserve(checked_bytes(cache_items, sizeof(float)));
    return 1;
}

static void free_state(llm_state_t* s) {
    freep(s->x);
    freep(s->xb);
    freep(s->xb2);
    freep(s->hb);
    freep(s->hb2);
    freep(s->q);
    freep(s->att);
    freep(s->logits);
    freep(s->key_cache);
    freep(s->value_cache);
    memset(s, 0, sizeof(*s));
}

int llm_model_load(llm_model_t* m, const char* path) {
    int fd = open(path, O_RDONLY);
    uint32 magic;
    uint32 version;
    int reserved[52];

    memset(m, 0, sizeof(*m));
    if (fd < 0) {
        fprintf(2, "llm: cannot open %s\n", path);
        return 0;
    }
    if (read_exact(fd, &magic, sizeof(magic)) < 0 ||
        read_exact(fd, &version, sizeof(version)) < 0 ||
        read_exact(fd, &m->config, sizeof(m->config)) < 0 ||
        read_exact(fd, reserved, sizeof(reserved)) < 0) {
        fprintf(2, "llm: bad model header\n");
        close(fd);
        return 0;
    }
    if (magic != MODEL_MAGIC || version != MODEL_VERSION) {
        fprintf(2, "llm: unsupported model\n");
        close(fd);
        return 0;
    }
    if (m->config.dim <= 0 || m->config.n_heads <= 0 ||
        m->config.dim % m->config.n_heads != 0 || m->config.n_kv_heads <= 0 ||
        m->config.n_heads % m->config.n_kv_heads != 0) {
        fprintf(2, "llm: invalid model config\n");
        close(fd);
        return 0;
    }

    m->weight_count = expected_weight_count(&m->config);
    m->weight_data = xmalloc(checked_bytes(m->weight_count, sizeof(float)));
    if (read_exact(
            fd, m->weight_data, checked_bytes(m->weight_count, sizeof(float))
        ) < 0) {
        fprintf(2, "llm: short model read\n");
        close(fd);
        llm_model_free(m);
        return 0;
    }
    close(fd);

    map_weights(m);
    return alloc_state(&m->state, &m->config);
}

void llm_model_free(llm_model_t* m) {
    free_state(&m->state);
    freep(m->weight_data);
    memset(&m->weights, 0, sizeof(m->weights));
    m->weight_data = 0;
    m->weight_count = 0;
}

// MARK: - forward

static void rmsnorm(float* out, float* x, float* weight, int size, float eps) {
    float ss = 0.0f;

    for (int i = 0; i < size; i++)
        ss += x[i] * x[i];
    ss = 1.0f / sqrt_approx(ss / size + eps);
    for (int i = 0; i < size; i++)
        out[i] = weight[i] * (ss * x[i]);
}

static void matmul(float* out, float* x, float* w, int n, int d) {
    for (int i = 0; i < d; i++) {
        float val = 0.0f;

        for (int j = 0; j < n; j++)
            val += w[(uint64)i * n + j] * x[j];
        out[i] = val;
    }
}

float* llm_forward(llm_model_t* m, int token, int pos) {
    llm_config_t* p = &m->config;
    llm_weights_t* w = &m->weights;
    llm_state_t* s = &m->state;
    float* x = s->x;
    int dim = p->dim;
    int head_size = dim / p->n_heads;
    int kv_dim = p->n_kv_heads * head_size;
    int kv_mul = p->n_heads / p->n_kv_heads;
    int hidden_dim = p->hidden_dim;

    memcpy(
        x, w->token_embedding_table + (uint64)token * dim, dim * sizeof(float)
    );

    for (int l = 0; l < p->n_layers; l++) {
        uint64 loff = (uint64)l * p->seq_len * kv_dim;
        float* k = s->key_cache + loff + (uint64)pos * kv_dim;
        float* v = s->value_cache + loff + (uint64)pos * kv_dim;

        rmsnorm(
            s->xb, x, w->rms_att_weight + (uint64)l * dim, dim, p->rms_norm_eps
        );
        matmul(s->q, s->xb, w->wq + (uint64)l * dim * dim, dim, dim);
        matmul(k, s->xb, w->wk + (uint64)l * dim * kv_dim, dim, kv_dim);
        matmul(v, s->xb, w->wv + (uint64)l * dim * kv_dim, dim, kv_dim);

        for (int h = 0; h < dim; h += 2) {
            int head_dim = h % head_size;
            float freq =
                1.0f / pow_approx(p->rope_theta, head_dim / (float)head_size);
            float val = pos * freq;
            float fcr = cos_approx(val);
            float fci = sin_approx(val);
            int rotn = h < kv_dim ? 2 : 1;

            for (int vi = 0; vi < rotn; vi++) {
                float* vec = vi == 0 ? s->q : k;
                float v0 = vec[h];
                float v1 = vec[h + 1];

                vec[h] = v0 * fcr - v1 * fci;
                vec[h + 1] = v0 * fci + v1 * fcr;
            }
        }

        for (int h = 0; h < p->n_heads; h++) {
            float* q = s->q + h * head_size;
            float* att = s->att + (uint64)h * p->seq_len;
            float* xb = s->xb + h * head_size;

            for (int t = 0; t <= pos; t++) {
                float* kt = s->key_cache + loff + (uint64)t * kv_dim +
                            (h / kv_mul) * head_size;
                float score = 0.0f;

                for (int i = 0; i < head_size; i++)
                    score += q[i] * kt[i];
                att[t] = score / sqrt_approx((float)head_size);
            }
            softmax(att, pos + 1);

            memset(xb, 0, head_size * sizeof(float));
            for (int t = 0; t <= pos; t++) {
                float* vt = s->value_cache + loff + (uint64)t * kv_dim +
                            (h / kv_mul) * head_size;
                float a = att[t];

                for (int i = 0; i < head_size; i++)
                    xb[i] += a * vt[i];
            }
        }

        matmul(s->xb2, s->xb, w->wo + (uint64)l * dim * dim, dim, dim);
        for (int i = 0; i < dim; i++)
            x[i] += s->xb2[i];

        rmsnorm(
            s->xb, x, w->rms_ffn_weight + (uint64)l * dim, dim, p->rms_norm_eps
        );
        matmul(
            s->hb, s->xb, w->w1 + (uint64)l * hidden_dim * dim, dim, hidden_dim
        );
        matmul(
            s->hb2, s->xb, w->w3 + (uint64)l * hidden_dim * dim, dim, hidden_dim
        );
        for (int i = 0; i < hidden_dim; i++) {
            float val = s->hb[i];

            val *= 1.0f / (1.0f + exp_approx(-val));
            s->hb[i] = val * s->hb2[i];
        }
        matmul(
            s->xb, s->hb, w->w2 + (uint64)l * dim * hidden_dim, hidden_dim, dim
        );
        for (int i = 0; i < dim; i++)
            x[i] += s->xb[i];
    }

    rmsnorm(x, x, w->rms_final_weight, dim, p->rms_norm_eps);
    matmul(s->logits, x, w->wcls, dim, p->vocab_size);
    return s->logits;
}

// MARK: - tokenizer loader

static int read_tokenizer_header(int fd, llm_tokenizer_t* t) {
    uint32 magic;
    uint32 version;
    int header[8];
    int reserved[54];

    if (read_exact(fd, &magic, sizeof(magic)) < 0 ||
        read_exact(fd, &version, sizeof(version)) < 0 ||
        read_exact(fd, header, sizeof(header)) < 0 ||
        read_exact(fd, reserved, sizeof(reserved)) < 0)
        return 0;
    if (magic != TOKENIZER_MAGIC || version != TOKENIZER_VERSION)
        return 0;

    t->vocab_size = header[0];
    t->max_piece_len = header[1];
    t->bos_id = header[2];
    t->eos_id = header[3];
    t->unk_id = header[4];
    t->special_count = header[5];
    t->merge_count = header[6];
    return t->vocab_size > 0 && t->max_piece_len > 0 && t->special_count >= 0 &&
           t->merge_count >= 0;
}

static int load_pieces(int fd, llm_tokenizer_t* t) {
    t->pieces = xmalloc(checked_bytes(t->vocab_size, sizeof(char*)));
    t->piece_lens = xmalloc(checked_bytes(t->vocab_size, sizeof(int)));

    for (int i = 0; i < t->vocab_size; i++) {
        int len;

        if (read_exact(fd, &len, sizeof(len)) < 0 || len < 0)
            return 0;
        t->piece_lens[i] = len;
        t->pieces[i] = xmalloc(len + 1);
        if (len > 0 && read_exact(fd, t->pieces[i], len) < 0)
            return 0;
        t->pieces[i][len] = 0;
    }
    return 1;
}

static int load_bpe(int fd, llm_tokenizer_t* t) {
    t->initial_ids = xmalloc(256 * sizeof(int));
    if (t->special_count > 0)
        t->special_ids = xmalloc(checked_bytes(t->special_count, sizeof(int)));
    if (t->merge_count > 0)
        t->merges = xmalloc(checked_bytes(t->merge_count, sizeof(llm_merge_t)));

    if (read_exact(fd, t->initial_ids, 256 * sizeof(int)) < 0)
        return 0;
    if (t->special_count > 0 &&
        read_exact(fd, t->special_ids, t->special_count * sizeof(int)) < 0)
        return 0;
    for (int i = 0; i < t->merge_count; i++) {
        int triple[3];

        if (read_exact(fd, triple, sizeof(triple)) < 0)
            return 0;
        t->merges[i].left = triple[0];
        t->merges[i].right = triple[1];
        t->merges[i].out = triple[2];
    }
    return 1;
}

int llm_tokenizer_load(llm_tokenizer_t* t, const char* path) {
    int fd = open(path, O_RDONLY);
    int ok;

    memset(t, 0, sizeof(*t));
    if (fd < 0) {
        fprintf(2, "llm: cannot open %s\n", path);
        return 0;
    }
    ok = read_tokenizer_header(fd, t) && load_pieces(fd, t) && load_bpe(fd, t);
    close(fd);
    if (!ok) {
        fprintf(2, "llm: bad tokenizer\n");
        llm_tokenizer_free(t);
        return 0;
    }
    return 1;
}

void llm_tokenizer_free(llm_tokenizer_t* t) {
    if (t->pieces) {
        for (int i = 0; i < t->vocab_size; i++)
            freep(t->pieces[i]);
    }
    freep(t->pieces);
    freep(t->piece_lens);
    freep(t->initial_ids);
    freep(t->special_ids);
    freep(t->merges);
    memset(t, 0, sizeof(*t));
}

// MARK: - inference

static int starts_with(char* s, char* prefix, int len) {
    for (int i = 0; i < len; i++) {
        if (s[i] != prefix[i])
            return 0;
    }
    return 1;
}

int llm_encode(
    llm_tokenizer_t* t,
    const char* text,
    int add_bos,
    int add_eos,
    int* tokens,
    int max_tokens
) {
    char* p = (char*)text;
    int n = 0;
    int merged;

    if (add_bos && t->bos_id >= 0) {
        if (n >= max_tokens)
            return -1;
        tokens[n++] = t->bos_id;
    }

    while (*p) {
        int special = -1;
        int special_len = 0;

        for (int i = 0; i < t->special_count; i++) {
            int id = t->special_ids[i];
            int len;

            if (id < 0 || id >= t->vocab_size)
                continue;
            len = t->piece_lens[id];
            if (len > special_len && len > 0 &&
                starts_with(p, t->pieces[id], len)) {
                special = id;
                special_len = len;
            }
        }

        if (special >= 0) {
            if (n >= max_tokens)
                return -1;
            tokens[n++] = special;
            p += special_len;
        } else {
            int id = t->initial_ids[(uchar)*p];

            if (id < 0)
                id = t->unk_id;
            if (id < 0 || n >= max_tokens)
                return -1;
            tokens[n++] = id;
            p++;
        }
    }

    do {
        int best_rank = t->merge_count + 1;
        int best_idx = -1;
        int best_out = -1;

        merged = 0;
        for (int i = 0; i < n - 1; i++) {
            for (int rank = 0; rank < t->merge_count; rank++) {
                if (t->merges[rank].left == tokens[i] &&
                    t->merges[rank].right == tokens[i + 1]) {
                    if (rank < best_rank) {
                        best_rank = rank;
                        best_idx = i;
                        best_out = t->merges[rank].out;
                    }
                    break;
                }
            }
        }
        if (best_idx >= 0) {
            tokens[best_idx] = best_out;
            for (int i = best_idx + 1; i < n - 1; i++)
                tokens[i] = tokens[i + 1];
            n--;
            merged = 1;
        }
    } while (merged);

    if (add_eos && t->eos_id >= 0) {
        if (n >= max_tokens)
            return -1;
        tokens[n++] = t->eos_id;
    }
    return n;
}

char* llm_decode_piece(llm_tokenizer_t* t, int token) {
    if (token < 0 || token >= t->vocab_size)
        return 0;
    return t->pieces[token];
}

void llm_print_piece(char* piece) {
    if (piece == 0 || piece[0] == 0)
        return;
    if (piece[1] == 0 && !char_printable(piece[0]) &&
        !char_whitespace(piece[0]))
        return;
    write(1, piece, strlen(piece));
}
