#ifndef HASH_H
#define HASH_H

#include <stdint.h>
#include <stddef.h>
#include <string>

#include <assert.h>
#include <openssl/evp.h>
namespace Hash {

    struct sha1sum_ctx {
		EVP_MD_CTX *ctx;
		const EVP_MD *md;
		uint8_t *salt;
		size_t len;
	};

    struct sha1sum_ctx * sha1sum_create(const uint8_t *salt, size_t len);

    int sha1sum_update(struct sha1sum_ctx*, const uint8_t *payload, size_t len);

    int sha1sum_finish(struct sha1sum_ctx*, const uint8_t *payload, size_t len, uint8_t *out);

    int sha1sum_reset(struct sha1sum_ctx*);

    int sha1sum_destroy(struct sha1sum_ctx*);

    uint64_t sha1sum_truncated_head(uint8_t *sha1_hash);

	// get the len byte truncated SHA1 hash of the payload
    // return as a string
    std::string truncated_sha1_hash(std::string payload, size_t len);
}

#endif
