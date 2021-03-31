#pragma once
#include <3ds.h>

void encryptAES(u8 *plaintext, u32 size, u8 *key, u8 *iv, u8 *output);
void decryptAES(u8 *ciphertext, u32 size, u8 *key, u8 *iv, u8 *output);
void calculateCMAC(u8 *input, u32 size, u8 *key, u8 *output);
void calculateSha256(u8 *input, u32 size, u8 *output);
