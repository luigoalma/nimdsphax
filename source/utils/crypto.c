#include <3ds.h>
#include "aes-cbc-cmac.h"
#include <utils/crypto.h>

void encryptAES(u8 *plaintext, u32 size, u8 *key, u8 *iv, u8 *output) {
	AES_CBC_ENC(iv, key, plaintext, size, output, size);
}

void decryptAES(u8 *ciphertext, u32 size, u8 *key, u8 *iv, u8 *output) {
	AES_CBC_DEC(iv, key, ciphertext, size, output, size);
}

void calculateCMAC(u8 *input, u32 size, u8 *key, u8 *output) {
	AES_CMAC(key, input, size, output);
}

void calculateSha256(u8 *input, u32 size, u8 *output) {
	FSUSER_UpdateSha256Context(input, size, output);
}
