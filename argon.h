/*
 * Part of Astonia Server 3.5 (c) Daniel Brockhaus. Please read license.txt.
 */

int argon2id_hash_password(char *out_encoded, size_t out_encoded_len, const char *password, const char *pepper_optional);
int argon2id_verify_password(const char *stored_encoded, const char *password, const char *pepper_optional);
