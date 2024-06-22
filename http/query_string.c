#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "query_string.h"

bool is_print(char c)
{
    return c >= 32 && c < 127;
}

bool hex_to_num(char x, int *n)
{
    // TODO: This is inefficient
    if      (x >= 'A' && x <= 'F') *n = x - 'A';
    else if (x >= 'a' && x <= 'f') *n = x - 'a';
    else if (x >= '0' && x <= '9') *n = x - '0';
    else return false;
    return true;
}

size_t skip_any_valid_chars_not_percent_encoded(char *src, size_t len, size_t cur)
{
    while (cur < len
        && src[cur] != '=' // Start of the value
        && src[cur] != '&' // End of the parameter (no value)
        && src[cur] != '%' // The name is percent encoded
        && src[cur] != '+' // Also percent encoded
        && is_print(src[cur])) // Query stirng contains invalid chracters
        cur++;
    return cur;
}

// This function is like the "parse" version but doesn't
// return the decoded string.
bool skip_percent_encoded_substr(char *src, size_t len, size_t *cur_)
{
    size_t cur = *cur_;
    cur = skip_any_valid_chars_not_percent_encoded(src, len, cur);
    while (cur < len && (src[cur] == '+' || src[cur] == '%')) {
        if (src[cur] == '+')
            cur++;
        else {
            int u, v;
            if (cur+2 >= len
                || !hex_to_num(src[cur+1], &u)
                || !hex_to_num(src[cur+2], &v))
                return false; // Invalid percent encoding
            cur += 3;
        }
        cur = skip_any_valid_chars_not_percent_encoded(src, len, cur);
    }
    *cur_ = cur;
    return true;
}

// Percent decodes the source string until a query string
// separator ('=', '&') or an invalid character. The resulting
// slice (out) may point into the source string or the
// destination buffer (dst). It only returns false if copying
// was required and the buffer was too small.
bool parse_percent_encoded_substr(char *src, size_t len,
                                  char *dst, size_t max,
                                  size_t *cur_,
                                  struct slice *out)
{
    size_t cur = *cur_;
    size_t start = cur;

    cur = skip_any_valid_chars_not_percent_encoded(src, len, cur);

    // Use secondary pointer so the key to avoid pointing
    // into the source buffer. If we found something to
    // decode (eg a %xx token) we'll need to copy the key
    // into the destination buffer and make this pointer
    // refer to that.
    out->data = src + start;
    out->size = cur - start;

    // If the name is percent encoded, copying into
    // a seprate buffer to decode it is necessary
    if (cur < len && (src[cur] == '%' || src[cur] == '+')) {

        // Copy what was already scanned into the
        // output buffer.
        if (out->size > max)
            return -1; // Output buffer is too small
        memcpy(dst, out->data, out->size);

        size_t copied = out->size;
        do {

            assert(cur < len);

            if (src[cur] == '+') {

                if (copied == max) return false; // Output buffer is too small
                dst[copied++] = ' ';
                cur++;

            } else {

                // We know there is a '%' at this point
                int u, v;
                if (cur+2 >= len
                    || !hex_to_num(src[cur+1], &u)
                    || !hex_to_num(src[cur+2], &v))
                    return false; // Invalid percent encoding
                cur += 3;

                assert(u >= 0 && u < 16);
                assert(v >= 0 && v < 16);
                uint8_t byte = (u << 4) | v;

                if (copied == max) return false; // Output buffer is too small
                dst[copied++] = (char) byte;
            }

            // Skip all plain characters and memcopy them at once

            start = cur;
            cur = skip_any_valid_chars_not_percent_encoded(src, len, cur);

            if (copied + cur - start > max) return false; // Buffer is too small
            memcpy(dst + copied,
                   src + start,
                   cur - start);
            copied += cur - start;

        } while (cur < len && (src[cur] == '%' || src[cur] == '+'));

        out->data = dst;
        out->size = copied;
    }

    *cur_ = cur;
    return true;
}

bool get_query_string_param(char *src, size_t src_len, char *key, char *dst, size_t max, struct slice *out)
{
    assert(key);

    if (src == NULL)
        src = "";

    size_t key_len = strlen(key);

    size_t cur = 0;
    if (cur < src_len && src[cur] == '?')
        cur++;

    while (cur < src_len) {
        
        struct slice key2;
        if (!parse_percent_encoded_substr(src, src_len, dst, max, &cur, &key2))
            return false; // Buffer too small
        
        if (cur < src_len && !is_print(src[cur]))
            return false; // Invalid query string

        if (key2.size == key_len && !memcmp(key2.data, key, key2.size)) {
            if (cur < src_len && src[cur] == '=') {
                cur++; // =
                if (!parse_percent_encoded_substr(src, src_len, dst, max, &cur, out))
                    return false;
            } else {
                out->data = "";
                out->size = 0;
            }
            return true;
        } else {
            if (cur < src_len && src[cur] == '=') {
                cur++; // =
                if (!skip_percent_encoded_substr(src, src_len, &cur))
                    return false;
            }
        }

        if (cur < src_len && src[cur] != '&')
            return false;
        cur++; // &
    }
    return false;
}