#include "fcntl.h"
#include "lab.h"
#include "llm.h"
#include "types.h"
#include "user.h"
#include "vm.h"

static uint64 kib(uint64 n) { return n / 1024; }

static int read_full(int fd, void* dst, int n) {
    char* p = dst;
    int done = 0;

    while (done < n) {
        int r = read(fd, p + done, n - done);
        if (r <= 0)
            return -1;
        done += r;
    }
    return 0;
}

static uint64 checked_mul(uint64 a, uint64 b) {
    if (a != 0 && b > ((uint64)-1) / a)
        return (uint64)-1;
    return a * b;
}

static uint64 weight_count(llm_config_t* c) {
    int head_size = c->dim / c->n_heads;
    int kv_dim = c->n_kv_heads * head_size;
    uint64 layers = c->n_layers;
    uint64 n = 0;

    n += checked_mul(checked_mul(layers, c->dim), c->dim);
    n += checked_mul(checked_mul(layers, c->dim), kv_dim);
    n += checked_mul(checked_mul(layers, c->dim), kv_dim);
    n += checked_mul(checked_mul(layers, c->dim), c->dim);
    n += checked_mul(checked_mul(layers, c->hidden_dim), c->dim);
    n += checked_mul(checked_mul(layers, c->dim), c->hidden_dim);
    n += checked_mul(checked_mul(layers, c->hidden_dim), c->dim);
    n += checked_mul(layers, c->dim);
    n += checked_mul(layers, c->dim);
    n += c->dim;
    if (!c->shared_classifier)
        n += checked_mul(c->vocab_size, c->dim);
    n += checked_mul(c->vocab_size, c->dim);
    return n;
}

int main(int argc, char** argv) {
    char* path = argc >= 2 ? argv[1] : "model.bin";
    struct vmstat st;
    int fd;
    uint32 magic;
    uint32 version;
    int reserved[52];
    llm_config_t config;
    uint64 weights;
    uint64 kv;
    int head_size;
    int kv_dim;

    if (vmstat(&st) < 0) {
        lab_str("status", "vmstat_failed");
        exit(1);
    }

    lab_u64("free_kib", kib(st.free_bytes));
    lab_u64("proc_size_kib", kib(st.proc_size));
    lab_u64("page_size", st.page_size);

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        lab_str("status", "model_open_failed");
        exit(1);
    }

    if (read_full(fd, &magic, sizeof(magic)) < 0 ||
        read_full(fd, &version, sizeof(version)) < 0 ||
        read_full(fd, &config, sizeof(config)) < 0 ||
        read_full(fd, reserved, sizeof(reserved)) < 0) {
        close(fd);
        lab_str("status", "model_header_failed");
        exit(1);
    }
    close(fd);

    if (magic != MODEL_MAGIC || version != MODEL_VERSION ||
        config.dim <= 0 || config.n_heads <= 0 ||
        config.dim % config.n_heads != 0 || config.n_kv_heads <= 0) {
        lab_str("status", "model_header_bad");
        exit(1);
    }

    head_size = config.dim / config.n_heads;
    kv_dim = config.n_kv_heads * head_size;
    weights = checked_mul(weight_count(&config), sizeof(float));
    kv = checked_mul(
        checked_mul(checked_mul(config.n_layers, config.seq_len), kv_dim),
        2 * sizeof(float)
    );

    lab_u64("weight_kib", kib(weights));
    lab_u64("kv_kib", kib(kv));
    lab_u64("state_floor_kib", kib(weights + kv));
    lab_str("status", "ok");
    exit(0);
}
