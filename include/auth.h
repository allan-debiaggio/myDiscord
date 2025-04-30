#ifndef AUTH_H
#define AUTH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"

// Function prototypes
void hash_password(const char *password, char *hash_output, size_t output_size);
bool verify_password(const char *password, const char *hash);

#endif /* AUTH_H */