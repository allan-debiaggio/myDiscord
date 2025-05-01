#include <bcrypt.h>
#include <openssl/evp.h>

char *hash_password(const char *password)
{
  char salt[BCRYPT_HASHSIZE];
  char hash[BCRYPT_HASHSIZE];
  bcrypt_gensalt(12, salt);
  bcrypt_hashpw(password, salt, hash);
  return strdup(hash);
}

int verify_password(const char *password, const char *hash)
{
  return bcrypt_checkpw(password, hash) == 0;
}

void encrypt_message(char *plaintext, size_t len, unsigned char *key)
{
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, NULL);

  int outlen;
  EVP_EncryptUpdate(ctx, plaintext, &outlen, plaintext, len);
  EVP_EncryptFinal_ex(ctx, plaintext + outlen, &outlen);
  EVP_CIPHER_CTX_free(ctx);
}

void decrypt_message(char *ciphertext, size_t len, unsigned char *key)
{
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, NULL);

  int outlen;
  EVP_DecryptUpdate(ctx, ciphertext, &outlen, ciphertext, len);
  EVP_DecryptFinal_ex(ctx, ciphertext + outlen, &outlen);
  EVP_CIPHER_CTX_free(ctx);
}