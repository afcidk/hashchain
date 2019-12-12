#include <inttypes.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#define DEFAULT_RANGE 10

/**
 * @brief Represents a hash chain.
 *
 * Everything here is required to be able to comprehend a hash chain. The
 * digest_size tells us how big each hash is, so that we can index properly. The
 * chain_length tells us how many hashes there are. The data pointer points at a
 * buffer of length chain_length * digest_size.
 */
struct hash_chain {
    int digest_size;
    int chain_length;
    uint8_t *data;
};

/**
 * @brief Print a hash chain into file f.
 * @param chain The chain to print.
 * @param f The file to write to.
 */
void hash_chain_print(struct hash_chain chain, FILE *f)
{
    BIO *out, *b64, *bio;
    b64 = BIO_new(BIO_f_base64());
    out = BIO_new_fp(f, BIO_NOCLOSE);
    bio = BIO_push(b64, out);

    BIO_write(bio, chain.data, chain.digest_size);
    BIO_flush(bio);

    BIO_free_all(bio);
}

/**
 * @brief Generate and return a hash chain.
 * @param base Pointer to seed data for first hash.
 * @param baselen Number of bytes in seed data.
 * @param type The hash algorithm to use.
 * @param chain_index Index of hashes to create.
 * @param chain_size  Size of hashes from index to create.
 * @returns A struct hash_chain with the last hash.
 */
struct hash_chain hash_chain_create(void *base,
                                    int baselen,
                                    const EVP_MD *type,
                                    int chain_index,
                                    int chain_size)
{
    EVP_MD_CTX *ctx;
    struct hash_chain output;

    // Allocate space for our hash chain.
    output.digest_size = EVP_MD_size(type);
    output.chain_length = 1;
    output.data = malloc(output.digest_size);

    // Hash the base data.
    ctx = EVP_MD_CTX_create();
    EVP_DigestInit_ex(ctx, type, NULL);
    EVP_DigestUpdate(ctx, base, baselen);
    EVP_DigestFinal_ex(ctx, output.data, NULL);

    /* For each remaining item in the chain, hash the previous digest.
     * We don't need the hash before the index
     */
    for (int idx = 1; idx < chain_index; idx++) {
        EVP_DigestInit_ex(ctx, type, NULL);
        EVP_DigestUpdate(ctx, output.data, output.digest_size);
        EVP_DigestFinal_ex(ctx, output.data, NULL);
    }

    for (int idx = 1; idx <= chain_size; idx++) {
        hash_chain_print(output, stdout);
        EVP_DigestInit_ex(ctx, type, NULL);
        EVP_DigestUpdate(ctx, output.data, output.digest_size);
        EVP_DigestFinal_ex(ctx, output.data, NULL);
    }

    // Cleanup and return the chain.
    EVP_MD_CTX_destroy(ctx);
    return output;
}

/**
 * @brief Verify that h comes directly before tip in a hash chain.
 * @param h Pointer to a hash.
 * @param tip Pointer to the "tip" hash.
 * @param hash Hash algorithm to use.
 * @param range Maximum test range.
 * @returns True if hash(h) == tip in range of hashes
 */
bool hash_chain_verify(const void *h,
                       const void *tip,
                       const EVP_MD *hash,
                       int range)
{
    EVP_MD_CTX *ctx;
    int result;
    int digest_len = EVP_MD_size(hash);
    if (memcmp(h, tip, digest_len) == 0)
        return true;

    void *data = malloc(digest_len);
    if (!data) {
        fprintf(stderr, "error: Malloc failed\n");
        return false;
    }
    memcpy(data, h, digest_len);

    ctx = EVP_MD_CTX_create();
    while (range--) {
        EVP_DigestInit_ex(ctx, hash, NULL);
        EVP_DigestUpdate(ctx, data, digest_len);
        EVP_DigestFinal_ex(ctx, data, NULL);

        result = memcmp(data, tip, digest_len);
        if (!result)
            break;
    }

    EVP_MD_CTX_destroy(ctx);
    free(data);
    return result == 0;
}

/**
 * @brief Base64 decode a string. You must free the return value.
 * @param str Some base64 encoded data.
 * @param explen The expected length of the data you're reading.
 * @returns Newly allocated pointer to buffer of length explen.
 */
void *base64_decode(char *str, int explen)
{
    uint8_t *buf = malloc(explen);
    BIO *b = BIO_new_mem_buf(str, -1);
    BIO *b64 = BIO_new(BIO_f_base64());
    BIO_push(b64, b);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_read(b64, buf, explen);
    BIO_free_all(b64);
    return buf;
}

/**
 * @brief Command for creating a hash chain.
 */
int cmd_create(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "error: too few args\n");
        fprintf(stderr, "usage: %s ALGORITHM INDEX SIZE SEED\n", argv[0]);
        fprintf(stderr, "       %s ALGORITHM LENGTH SEED\n", argv[0]);
        return EXIT_FAILURE;
    }

    const EVP_MD *hash = EVP_get_digestbyname(argv[1]);
    if (hash == NULL) {
        fprintf(stderr, "error: hash %s doesn't exist\n", argv[1]);
        return EXIT_FAILURE;
    }

    if (argc == 5) {
        int index;
        if (sscanf(argv[2], "%d", &index) != 1) {
            fprintf(stderr, "error: can't convert %s to integer\n", argv[2]);
            return EXIT_FAILURE;
        }

        int size;
        if (sscanf(argv[3], "%d", &size) != 1) {
            fprintf(stderr, "error: can't convert %s to integer\n", argv[2]);
            return EXIT_FAILURE;
        }

        struct hash_chain chain =
            hash_chain_create(argv[4], strlen(argv[4]), hash, index, size);
        free(chain.data);
    } else if (argc == 4) {
        int length;
        if (sscanf(argv[2], "%d", &length) != 1) {
            fprintf(stderr, "error: can't convert %s to integer\n", argv[2]);
            return EXIT_FAILURE;
        }
        struct hash_chain chain =
            hash_chain_create(argv[3], strlen(argv[3]), hash, 0, length);
        free(chain.data);
    } else {
        fprintf(stderr, "usage: %s ALGORITHM INDEX SIZE SEED\n", argv[0]);
        fprintf(stderr, "       %s ALGORITHM LENGTH SEED\n", argv[0]);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/**
 * @brief Command for verifying a hash.
 */
int cmd_verify(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "error: too few args\n");
        fprintf(stderr, "usage: %s ALGO QUERY ANCHOR [MAX_RANGE]\n", argv[0]);
        return EXIT_FAILURE;
    }

    const EVP_MD *hash = EVP_get_digestbyname(argv[1]);
    const int range = (argc == 4) ? DEFAULT_RANGE : atoi(argv[4]);
    if (hash == NULL) {
        fprintf(stderr, "error: hash %s doesn't exist\n", argv[1]);
        return EXIT_FAILURE;
    }

    int digest_len = EVP_MD_size(hash);
    void *qhash = base64_decode(argv[2], digest_len);
    void *thash = base64_decode(argv[3], digest_len);

    bool res = hash_chain_verify(qhash, thash, hash, range);
    free(qhash);
    free(thash);
    if (res) {
        printf("success\n");
        return EXIT_SUCCESS;
    }
    printf("failure\n");
    return EXIT_FAILURE;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "error: subcommand required\n");
        return EXIT_FAILURE;
    }

    OpenSSL_add_all_digests();
    int rv;

    if (strcmp(argv[1], "create") == 0) {
        rv = cmd_create(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "verify") == 0) {
        rv = cmd_verify(argc - 1, argv + 1);
    } else {
        fprintf(stderr, "error: subcommand %s not found\n", argv[1]);
        rv = EXIT_FAILURE;
    }

    EVP_cleanup();
    return rv;
}
