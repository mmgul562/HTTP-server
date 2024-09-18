#include "generate_token.h"
#include <openssl/rand.h>


bool generate_token(char *token) {
    unsigned char random_bytes[SESSION_TOKEN_LENGTH];
    if (RAND_bytes(random_bytes, SESSION_TOKEN_LENGTH) != 1) {
        return false;
    }
    for (int i = 0; i < SESSION_TOKEN_LENGTH; ++i) {
        sprintf(token + (i * 2), "%02x", random_bytes[i]);
    }
    return true;
}

