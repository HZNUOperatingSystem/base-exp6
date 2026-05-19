#ifndef XV6_USER_LLM_H
#define XV6_USER_LLM_H

#include "types.h"

#define MODEL_MAGIC 0x464e494cU
#define MODEL_VERSION 3

#define TOKENIZER_MAGIC 0x4e4b4f54U
#define TOKENIZER_VERSION 3

typedef struct {
    int dim;
    int hidden_dim;
    int n_layers;
    int n_heads;
    int n_kv_heads;
    int vocab_size;
    int seq_len;
    float rope_theta;
    float rms_norm_eps;
    int shared_classifier;
} llm_config_t;

typedef struct {
    float* wq;
    float* wk;
    float* wv;
    float* wo;
    float* w1;
    float* w2;
    float* w3;
    float* rms_att_weight;
    float* rms_ffn_weight;
    float* rms_final_weight;
    float* wcls;
    float* token_embedding_table;
} llm_weights_t;

typedef struct {
    float* x;
    float* xb;
    float* xb2;
    float* hb;
    float* hb2;
    float* q;
    float* att;
    float* logits;
    float* key_cache;
    float* value_cache;
} llm_state_t;

typedef struct {
    llm_config_t config;
    llm_weights_t weights;
    llm_state_t state;
    float* weight_data;
    uint64 weight_count;
} llm_model_t;

typedef struct {
    int left;
    int right;
    int out;
} llm_merge_t;

typedef struct {
    char** pieces;
    int* piece_lens;
    int* initial_ids;
    int* special_ids;
    llm_merge_t* merges;
    int vocab_size;
    int max_piece_len;
    int bos_id;
    int eos_id;
    int unk_id;
    int special_count;
    int merge_count;
} llm_tokenizer_t;

int llm_model_load(llm_model_t* m, const char* path);
void llm_model_free(llm_model_t* m);
float* llm_forward(llm_model_t* m, int token, int pos);

int llm_tokenizer_load(llm_tokenizer_t* t, const char* path);
void llm_tokenizer_free(llm_tokenizer_t* t);
int llm_encode(
    llm_tokenizer_t* t,
    const char* text,
    int add_bos,
    int add_eos,
    int* tokens,
    int max_tokens
);
char* llm_decode_piece(llm_tokenizer_t* t, int token);
void llm_print_piece(char* piece);

#endif
