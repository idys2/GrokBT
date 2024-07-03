#include <string.h>
#include <strings.h>

#include "hash.h"

/* third party libraries */
#include <openssl/evp.h>

/* You shouldn't have to be looking at this file, but have fun! */

namespace Hash {

	struct sha1sum_ctx * sha1sum_create(const uint8_t *salt, size_t len) {
		struct sha1sum_ctx *csm = (struct sha1sum_ctx *)malloc(sizeof(*csm));
		if (!csm) {
			goto err;
		}
		bzero(csm, sizeof(*csm));
		csm->ctx = EVP_MD_CTX_new();
		csm->md = EVP_sha1();
		csm->len = len;
		if (len > 0) {
			csm->salt = (uint8_t *)malloc(len);
			if (!csm->salt) {
				goto err;
			}
			memcpy(csm->salt, salt, len);
		}
		if (sha1sum_reset(csm)) {
			goto err;
		}


		return csm;

	err:
		if (csm) {
			if (csm->salt) {
				free(csm->salt);
			}
			bzero(csm, sizeof(*csm));
			free(csm);
		}
		csm = NULL;

		return csm;
	}

	int sha1sum_update(struct sha1sum_ctx *csm, const uint8_t *payload, size_t len) {
		return EVP_DigestUpdate(csm->ctx, payload, len) != 1;
	}

	int sha1sum_finish(struct sha1sum_ctx *csm, const uint8_t *payload, size_t len, uint8_t *out) {
		int ret = 1;
		if (len) {
			ret = EVP_DigestUpdate(csm->ctx, payload, len);
		}
		if (ret == 1) {
			return EVP_DigestFinal_ex(csm->ctx, out, NULL) != 1;
		} else {
			return 1;
		}
	}

	int sha1sum_reset(struct sha1sum_ctx *csm) {
		EVP_DigestInit_ex(csm->ctx, csm->md, NULL);
		if (csm->len) {
			return EVP_DigestUpdate(csm->ctx, csm->salt, csm->len) != 1;
		} else {
			return 0;
		}
		
	}

	int sha1sum_destroy(struct sha1sum_ctx *csm) {
		EVP_MD_CTX_free(csm->ctx);
		free(csm->salt);
		bzero(csm, sizeof(*csm));
		free(csm);
		return 0;
	}

	uint64_t sha1sum_truncated_head(uint8_t *sha1_hash)
	{
		uint64_t truncated_sha1 = 0;
		memcpy(&truncated_sha1, sha1_hash, sizeof(uint64_t));

		return truncated_sha1;
	}

    std::string truncated_sha1_hash(std::string payload, size_t len) {
		sha1sum_ctx *ctx = sha1sum_create(NULL, 0);
		char checksum[20];
        int error = sha1sum_finish(ctx, (const uint8_t *) payload.c_str(), payload.length(), (uint8_t *)checksum);
        assert(!error);
        assert(sha1sum_destroy(ctx) == 0);
        return std::string(checksum, len);
	}
}