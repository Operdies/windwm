/* See LICENSE file for copyright and license details. */

#pragma once
#define MAX(A, B)               ((A) > (B) ? (A) : (B))
#define MIN(A, B)               ((A) < (B) ? (A) : (B))
#define BETWEEN(X, A, B)        ((A) <= (X) && (X) <= (B))

#ifdef DEBUG
#define TRACEF(fmt, ...)        fprintf(stderr, "%s:%d:%s(): " fmt "\n", __FILE__, __LINE__, __func__, __VA_ARGS__)
#define TRACE(fmt) TRACEF("%s", fmt)
#else
#define TRACEF(fmt, ...)
#define TRACE(fmt)
#endif

void errormsg(const char *fmt, ...);
void die(const char *fmt, ...);
void *ecalloc(size_t nmemb, size_t size);
