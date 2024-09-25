#include "helpers.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "../../db/todos.h"
#include "../../db/users.h"
#include <curl/curl.h>



struct upload_status {
    const char *data;
    size_t bytes_read;
};


static size_t read_callback(char *ptr, size_t size, size_t nmemb, void *userp) {
    struct upload_status *upload_ctx = (struct upload_status *)userp;
    size_t room = size * nmemb;

    if ((size == 0) || (nmemb == 0) || ((size * nmemb) < 1)) {
        return 0;
    }

    size_t len = strlen(upload_ctx->data) - upload_ctx->bytes_read;
    if (len > room)
        len = room;

    memcpy(ptr, upload_ctx->data + upload_ctx->bytes_read, len);
    upload_ctx->bytes_read += len;

    return len;
}


bool send_email(const char *to, const char *subject, const char *template_file, const char *placeholder, const char *body) {
    CURL *curl;
    CURLcode res = CURLE_OK;
    struct curl_slist *recipients = NULL;
    struct curl_slist *headers = NULL;

    char *smtp_server = getenv("SMTP_SERVER");
    char *sender = getenv("FROM_EMAIL");
    char *app_password = getenv("EMAIL_APP_PASSWD");

    if (!smtp_server || !sender || !app_password) {
        fprintf(stderr, "Missing environment variables for email sending\n");
        return false;
    }

    char *remainder = NULL;
    char *html_content = read_template(template_file, placeholder, &remainder);
    if (!html_content) {
        return false;
    }

    const char *email_template =
        "To: %s\r\n"
        "From: %s\r\n"
        "Subject: %s\r\n"
        "MIME-Version: 1.0\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "\r\n"
        "%s%s%s";

    size_t full_email_size = strlen(email_template) + strlen(to) + strlen(sender) + strlen(subject) + strlen(html_content) + strlen(body) + strlen(remainder) + 1;
    char *full_email = malloc(full_email_size);
    if (!full_email) {
        free(html_content);
        return false;
    }
    snprintf(full_email, full_email_size, email_template, to, sender, subject, html_content, body, remainder);

    struct upload_status upload_ctx = { full_email, 0 };

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, smtp_server);
        curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
        curl_easy_setopt(curl, CURLOPT_USERNAME, sender);
        curl_easy_setopt(curl, CURLOPT_PASSWORD, app_password);
        char angle_from[256];
        snprintf(angle_from, sizeof(angle_from), "<%s>", sender);
        curl_easy_setopt(curl, CURLOPT_MAIL_FROM, angle_from);

        char angle_to[256];
        snprintf(angle_to, sizeof(angle_to), "<%s>", to);
        recipients = curl_slist_append(recipients, angle_to);
        curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

        curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
        curl_easy_setopt(curl, CURLOPT_READDATA, &upload_ctx);
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }

        curl_slist_free_all(recipients);
        curl_easy_cleanup(curl);
    }

    free(html_content);
    return res == CURLE_OK;
}


static char hex_to_char(char c) {
    if (c >= '0' && c <= '9') return (char)(c - '0');
    if (c >= 'a' && c <= 'f') return (char)(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return (char)(c - 'A' + 10);
    return -1;
}


static void url_decode(const char *src, size_t src_len, char *dest) {
    int i, j;

    for (i = 0, j = 0; i < src_len; ++i, ++j) {
        if (src[i] == '%' && i + 2 < src_len) {
            char high = hex_to_char(src[i + 1]);
            char low = hex_to_char(src[i + 2]);
            if (high >= 0 && low >= 0) {
                dest[j] = (char)((high << 4) | low);
                i += 2;
            } else {
                dest[j] = src[i];
            }
        } else if (src[i] == '+') {
            dest[j] = ' ';
        } else {
            dest[j] = src[i];
        }
    }
    dest[j] = '\0';
}


bool extract_url_param(const char *src, const char *key, char *dest, int max_len) {
    char search_key[256];
    snprintf(search_key, sizeof(search_key), "%s=", key);

    char *start = strstr(src, search_key);
    if (!start) return NULL;
    start += strlen(search_key);

    char *end = strchr(start, '&');
    if (!end) end = start + strlen(start);

    size_t len = end - start;
    if (len > max_len) {
        dest[0] = '\0';
        return false;
    }
    char encoded[len + 1];
    strncpy(encoded, start, len);
    encoded[len] = '\0';

    url_decode(encoded, len, dest);
    return true;
}


void skip_placeholder(char *buffer, const char *placeholder, char **remainder) {
    char *placeholder_pos = strstr(buffer, placeholder);
    if (placeholder_pos) {
        *placeholder_pos = '\0';
        *remainder = placeholder_pos + strlen(placeholder);
    } else {
        *remainder = buffer + strlen(buffer);
    }
}


char *read_template(const char *filename, const char *placeholder, char **remainder) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening template file");
        return NULL;
    }

    char *buffer = malloc(MAX_TEMPLATE_SIZE);
    size_t bytesRead = fread(buffer, 1, MAX_TEMPLATE_SIZE - 1, file);
    buffer[bytesRead] = '\0';

    fclose(file);

    skip_placeholder(buffer, placeholder, remainder);

    return buffer;
}


bool parse_url_data(const char *body, const char **expected_keys, int n_expected_keys, bool *found_keys) {
    if (!body) return false;

    char *body_copy = strdup(body);
    char *token, *rest = body_copy;
    while ((token = strtok_r(rest, "&", &rest))) {
        char *key = strtok(token, "=");
        char *value = strtok(NULL, "=");

        if (key && value) {
            bool is_expected = false;
            for (int i = 0; i < n_expected_keys; ++i) {
                if (strcmp(key, expected_keys[i]) == 0) {
                    found_keys[i] = true;
                    is_expected = true;
                    break;
                }
            }
            if (!is_expected) {
                free(body_copy);
                return false;
            }
        }
    }
    free(body_copy);
    return true;
}


int is_path_safe(const char *path) {
    char resolved_path[MAX_PATH_LENGTH];
    char resolved_root[MAX_PATH_LENGTH];

    if (realpath(path, resolved_path) == NULL) {
        return 0;
    }
    if (realpath(DOCUMENT_ROOT, resolved_root) == NULL) {
        perror("Invalid DOCUMENT_ROOT");
        return 0;
    }

    return (strncmp(resolved_path, resolved_root, strlen(resolved_root)) == 0);
}


bool validate_url_id(const char *url_id, int *id) {
    size_t url_len = strlen(url_id);
    for (int i = 0; i < url_len; ++i) {
        if (!isdigit(*(url_id + i))) {
            return false;
        }
    }

    *id = atoi(url_id);
    if (*id < 0 || (*id == 0 && strcmp(url_id, "0") != 0)) {
        return false;
    }
    return true;
}


bool is_valid_email(const char *email) {
    int atSymbolIndex = -1;
    int dotSymbolIndex = -1;
    size_t length = strlen(email);
    if (length > DB_EMAIL_LEN) {
        return false;
    }

    for (int i = 0; i < length; i++) {
        if (email[i] == '@') {
            if (atSymbolIndex != -1) {
                return false;
            }
            atSymbolIndex = i;
        } else if (email[i] == '.' && atSymbolIndex != -1) {
            dotSymbolIndex = i;
        }
    }
    if (atSymbolIndex > 0 && dotSymbolIndex > atSymbolIndex + 1 && dotSymbolIndex < length - 1) {
        return true;
    }
    return false;
}


bool is_valid_password(const char *password, char *msg) {
    size_t length = strlen(password);
    if (length < 8) {
        sprintf(msg, "Password must be at least 8 characters long.");
        return false;
    } else if (length > DB_PASSWORD_LEN) {
        sprintf(msg, "Password cannot be longer than %d characters.", DB_PASSWORD_LEN);
        return false;
    }

    bool hasChar = false;
    bool hasDigit = false;
    bool hasSpecial = false;

    for (int i = 0; i < length; i++) {
        if (isalpha(password[i])) {
            hasChar = true;
        } else if (isdigit(password[i])) {
            hasDigit = true;
        } else if (ispunct(password[i])) {
            hasSpecial = true;
        }
    }
    if (!hasChar) {
        sprintf(msg, "Password must have at least 1 letter.");
        return false;
    } else if (!(hasDigit || hasSpecial)) {
        sprintf(msg, "Password must have at least 1 number/special character.");
        return false;
    }
    return true;
}