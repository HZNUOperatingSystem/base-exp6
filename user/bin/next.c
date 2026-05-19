#include "llm.h"
#include "types.h"
#include "user.h"

static void usage(void) {
    fprintf(2, "usage: next model.linf tokenizer.ltok [steps] [prompt]\n");
    exit(1);
}

static int sample_argmax(float* logits, int n) {
    int best = 0;
    float best_val = logits[0];

    for (int i = 1; i < n; i++) {
        if (logits[i] > best_val) {
            best = i;
            best_val = logits[i];
        }
    }
    return best;
}

int main(int argc, char** argv) {
    llm_model_t model;
    llm_tokenizer_t tokenizer;
    char* prompt = "";
    int steps = 64;
    int max_tokens;
    int n_tokens;
    int* tokens;
    int pos = 0;
    float* logits = 0;

    if (argc < 3)
        usage();
    if (argc >= 4)
        steps = atoi(argv[3]);
    if (argc >= 5)
        prompt = argv[4];
    if (steps < 0)
        steps = 0;

    if (!llm_model_load(&model, argv[1]))
        exit(1);
    if (!llm_tokenizer_load(&tokenizer, argv[2])) {
        llm_model_free(&model);
        exit(1);
    }
    if (tokenizer.vocab_size != model.config.vocab_size) {
        fprintf(2, "next: tokenizer/model vocab mismatch\n");
        exit(1);
    }

    max_tokens = strlen(prompt) + 8;
    tokens = malloc(max_tokens * sizeof(int));
    if (tokens == 0) {
        fprintf(2, "next: malloc failed\n");
        exit(1);
    }
    n_tokens = llm_encode(&tokenizer, prompt, 1, 0, tokens, max_tokens);
    if (n_tokens <= 0) {
        fprintf(2, "next: prompt encoding failed\n");
        exit(1);
    }

    for (int i = 0; i < n_tokens && pos < model.config.seq_len; i++)
        logits = llm_forward(&model, tokens[i], pos++);

    for (int i = 0; i < steps && pos < model.config.seq_len && logits; i++) {
        int token = sample_argmax(logits, model.config.vocab_size);
        char* piece;

        if (token == tokenizer.eos_id)
            break;
        piece = llm_decode_piece(&tokenizer, token);
        llm_print_piece(piece);
        logits = llm_forward(&model, token, pos++);
    }
    printf("\n");

    free(tokens);
    llm_tokenizer_free(&tokenizer);
    llm_model_free(&model);
    exit(0);
}
