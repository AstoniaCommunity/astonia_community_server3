/*
 * Part of Astonia Server 3.5 (c) Daniel Brockhaus. Please read license.txt.
 */

// Build: gcc -O2 -Wall -Wextra -std=c11 password_hash.c -largon2
// Ubuntu: sudo apt install libargon2-dev

#define _GNU_SOURCE
#include <argon2.h>

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/random.h>
#include <unistd.h>

#include "argon.h"

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

// -------------------------
// CSPRNG (Linux getrandom + /dev/urandom fallback)
// -------------------------
static int csprng_bytes(void *buf, size_t len) {
    uint8_t *p = (uint8_t *)buf;

    // Try getrandom() first
    while (len > 0) {
        ssize_t n = getrandom(p, len, 0);
        if (n > 0) {
            p += (size_t)n;
            len -= (size_t)n;
            continue;
        }
        if (n < 0 && errno == EINTR) continue;

        // getrandom not available or failed: fall back
        break;
    }
    if (len == 0) return 0;

    // Fallback: /dev/urandom
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) return -1;

    while (len > 0) {
        ssize_t n = read(fd, p, len);
        if (n > 0) {
            p += (size_t)n;
            len -= (size_t)n;
            continue;
        }
        if (n < 0 && errno == EINTR) continue;
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

// Optional: add a "pepper" (server secret) to reduce damage if DB leaks.
// Keep pepper OUT of the DB (config file/env var). If you don't want it, set to NULL.
static void build_peppered_password(const char *password,
                                    const char *pepper,
                                    char *out,
                                    size_t out_len) {
    if (!pepper || pepper[0] == '\0') {
        // No pepper: just copy password
        snprintf(out, out_len, "%s", password);
        return;
    }
    // Combine as password + '\0' + pepper (or any unambiguous scheme you like).
    // Here: password + ":" + pepper
    snprintf(out, out_len, "%s:%s", password, pepper);
}

// -------------------------
// Public API
// -------------------------

// Returns 0 on success, -1 on failure.
// out_encoded should be stored in DB (VARCHAR(255) recommended).
int argon2id_hash_password(char *out_encoded, size_t out_encoded_len, const char *password, const char *pepper_optional) {
    if (!out_encoded || out_encoded_len == 0 || !password) return -1;

    // --- Parameters (tune these) ---
    // Start conservative; increase later to hit a target time (e.g. 50-200ms).
    const uint32_t t_cost = 2; // iterations
    const uint32_t m_cost_kib = 19 * 1024; // memory in KiB (19 MiB baseline-ish)
    const uint32_t parallelism = 1; // lanes/threads
    const size_t salt_len = 16;
    const size_t hash_len = 32;

    uint8_t salt[16];
    if (csprng_bytes(salt, salt_len) != 0) {
        return -1;
    }

    // Optionally pepper the password (if you don’t want pepper, pass NULL).
    // Note: This buffer should be large enough for password + ":" + pepper.
    // Adjust if you allow very long passwords/pepper.
    char pwbuf[512];
    build_peppered_password(password, pepper_optional, pwbuf, sizeof(pwbuf));

    // Compute the required encoded length and ensure caller buffer is big enough.
    size_t need = (size_t)argon2_encodedlen(
        t_cost, m_cost_kib, parallelism,
        (uint32_t)salt_len, (uint32_t)hash_len,
        Argon2_id);
    if (need + 1 > out_encoded_len) { // +1 just to be safe for '\0'
        // Caller buffer too small
        return -1;
    }

    int rc = argon2id_hash_encoded(
        t_cost, m_cost_kib, parallelism,
        pwbuf, strlen(pwbuf),
        salt, salt_len,
        hash_len,
        out_encoded, out_encoded_len);

    // Clear pwbuf from memory (best-effort)
    memset(pwbuf, 0, sizeof(pwbuf));

    if (rc != ARGON2_OK) {
        return -1;
    }
    return 0;
}

// Returns 1 if correct, 0 if incorrect, -1 on error.
int argon2id_verify_password(const char *stored_encoded, const char *password, const char *pepper_optional) {
    if (!stored_encoded || !password) return -1;

    char pwbuf[512];
    build_peppered_password(password, pepper_optional, pwbuf, sizeof(pwbuf));

    int rc = argon2id_verify(stored_encoded, pwbuf, strlen(pwbuf));

    memset(pwbuf, 0, sizeof(pwbuf));

    if (rc == ARGON2_OK) return 1;
    if (rc == ARGON2_VERIFY_MISMATCH) return 0;

    // Other errors: malformed hash string, out of memory, etc.
    return -1;
}
