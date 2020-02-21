/*
 * Copyright (c) 2013, Google Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifdef USE_HOSTCC
#include "mkimage.h"
#include <time.h>
#else
#include <common.h>
#include <malloc.h>
#include <optee_include/OpteeClientInterface.h>
DECLARE_GLOBAL_DATA_PTR;
#endif /* !USE_HOSTCC*/
#include <image.h>
#include <u-boot/rsa.h>
#include <u-boot/rsa-checksum.h>

#define IMAGE_MAX_HASHED_NODES		100

static const uint8_t sha1_der_prefix_data[SHA1_DER_LEN] = {
	0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e,
	0x03, 0x02, 0x1a, 0x05, 0x00, 0x04, 0x14
};

static const uint8_t sha256_der_prefix_data[SHA256_DER_LEN] = {
	0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
	0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05,
	0x00, 0x04, 0x20
};

struct checksum_algo checksum_algos[] = {
	{
		.name = "sha1",
		.checksum_len = SHA1_SUM_LEN,
		.der_len = SHA1_DER_LEN,
		.der_prefix = sha1_der_prefix_data,
#if IMAGE_ENABLE_SIGN
		.calculate_sign = EVP_sha1,
#endif
		.calculate = hash_calculate,
	},
	{
		.name = "sha256",
		.checksum_len = SHA256_SUM_LEN,
		.der_len = SHA256_DER_LEN,
		.der_prefix = sha256_der_prefix_data,
#if IMAGE_ENABLE_SIGN
		.calculate_sign = EVP_sha256,
#endif
		.calculate = hash_calculate,
	}

};

struct crypto_algo crypto_algos[] = {
	{
		.name = "rsa2048",
		.key_len = RSA2048_BYTES,
		.sign = rsa_sign,
		.add_verify_data = rsa_add_verify_data,
		.verify = rsa_verify,
	},
	{
		.name = "rsa4096",
		.key_len = RSA4096_BYTES,
		.sign = rsa_sign,
		.add_verify_data = rsa_add_verify_data,
		.verify = rsa_verify,
	}

};

struct padding_algo padding_algos[] = {
	{
		.name = "pkcs-1.5",
		.verify = padding_pkcs_15_verify,
	},
#ifdef CONFIG_FIT_ENABLE_RSASSA_PSS_SUPPORT
	{
		.name = "pss",
		.verify = padding_pss_verify,
	}
#endif /* CONFIG_FIT_ENABLE_RSASSA_PSS_SUPPORT */
};

struct checksum_algo *image_get_checksum_algo(const char *full_name)
{
	int i;
	const char *name;

	for (i = 0; i < ARRAY_SIZE(checksum_algos); i++) {
		name = checksum_algos[i].name;
		/* Make sure names match and next char is a comma */
		if (!strncmp(name, full_name, strlen(name)) &&
		    full_name[strlen(name)] == ',')
			return &checksum_algos[i];
	}

	return NULL;
}

struct crypto_algo *image_get_crypto_algo(const char *full_name)
{
	int i;
	const char *name;

	/* Move name to after the comma */
	name = strchr(full_name, ',');
	if (!name)
		return NULL;
	name += 1;

	for (i = 0; i < ARRAY_SIZE(crypto_algos); i++) {
		if (!strcmp(crypto_algos[i].name, name))
			return &crypto_algos[i];
	}

	return NULL;
}

struct padding_algo *image_get_padding_algo(const char *name)
{
	int i;

	if (!name)
		return NULL;

	for (i = 0; i < ARRAY_SIZE(padding_algos); i++) {
		if (!strcmp(padding_algos[i].name, name))
			return &padding_algos[i];
	}

	return NULL;
}

#ifndef USE_HOSTCC
#if CONFIG_IS_ENABLED(FIT_ROLLBACK_PROTECT)
__weak int fit_read_otp_rollback_index(uint32_t fit_index, uint32_t *otp_index)
{
	*otp_index = 0;

	return 0;
}
__weak int fit_rollback_index_verify(const void *fit, uint32_t rollback_fd,
				     uint32_t *this_index, uint32_t *min_index)
{
	*this_index = 0;
	*min_index = 0;

	return 0;
}
#endif
#endif
