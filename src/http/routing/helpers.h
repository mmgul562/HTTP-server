#ifndef HTTP_SERVER_HELPERS_H
#define HTTP_SERVER_HELPERS_H

#include "route.h"

#define MAX_TEMPLATE_SIZE 8192
#define MAX_PATH_LENGTH 304
#define DOCUMENT_ROOT "../src/http/www"


bool send_email(const char *to, const char *subject, const char *template_file, const char *placeholder, const char *body);

bool extract_url_param(const char *src, const char *key, char *dest, int max_len);

void skip_placeholder(char *buffer, const char *placeholder, char **remainder);

char *read_template(const char *filename, const char *placeholder, char **remainder);

bool parse_url_data(const char *body, const char **expected_keys, int n_expected_keys, bool *found_keys);

int is_path_safe(const char *path);

bool validate_url_id(const char *url_id, int *id);

bool is_valid_email(const char *email);

bool is_valid_password(const char *password, char *msg);


#endif
