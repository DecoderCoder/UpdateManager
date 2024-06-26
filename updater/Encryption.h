#pragma once
#include <string>
#include <openssl/aes.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include "openssl/engine.h"
#include "Base64.hpp"

using namespace std;

static string GetIV(string sha, string keyName) {
	unsigned char out_key[32];
	memset(out_key, 0, sizeof(out_key));
	PKCS5_PBKDF2_HMAC(sha.c_str(), sha.size(), (const unsigned char*)keyName.c_str(), keyName.size(), 1000, EVP_sha512(), 32, out_key);
	return string((char*)out_key, 32);
}

static string DecryptAES(string cipherText, string keyValue, string IV) {
	// SetKeyIV
	// 40 53 56 57 41 56 41 57 48 83 EC 70 48 8B 05 ? ? ? ? 48 33 C4 48 89 44 24 ? 49 8B F1

	// GetIV
	// E8 ? ? ? ? 48 8B 76 18 
	int actual_size = 0, final_size = 0;
	EVP_CIPHER_CTX* d_ctx = EVP_CIPHER_CTX_new();
	string keyIn = base64::from_base64(keyValue);

	unsigned char* out = (unsigned char*)malloc(cipherText.size());
	memset(out, 0, cipherText.size());
	EVP_DecryptInit(d_ctx, EVP_aes_128_gcm(), 0, 0);
	EVP_CIPHER_CTX_ctrl(d_ctx, EVP_CTRL_GCM_SET_IVLEN, IV.size(), 0);
	EVP_DecryptInit(d_ctx, 0, (const unsigned char*)keyIn.c_str(), (const unsigned char*)IV.c_str());
	EVP_DecryptUpdate(d_ctx, out, &actual_size, (const unsigned char*)cipherText.c_str(), cipherText.size()); // E8 ? ? ? ? 0F B6 4E 06 
	EVP_DecryptFinal(d_ctx, out, &final_size);
	return string((char*)out, actual_size);
}

static string EncryptAES(string text, string keyValue, string IV) {
	int actual_size = 0, final_size = 0;
	EVP_CIPHER_CTX* d_ctx = EVP_CIPHER_CTX_new();
	string keyIn = base64::from_base64(keyValue);

	unsigned char* out = (unsigned char*)malloc(text.size());
	memset(out, 0, text.size());

	EVP_EncryptInit(d_ctx, EVP_aes_128_gcm(), 0, 0);
	EVP_CIPHER_CTX_ctrl(d_ctx, EVP_CTRL_GCM_SET_IVLEN, IV.size(), 0);

	EVP_EncryptInit(d_ctx, 0, (const unsigned char*)keyIn.c_str(), (const unsigned char*)IV.c_str());
	EVP_EncryptUpdate(d_ctx, out, &actual_size, (const unsigned char*)text.c_str(), text.size()); // E8 ? ? ? ? 0F B6 4E 06 
	EVP_EncryptFinal(d_ctx, out, &final_size);
	return string((char*)out, actual_size);
}