#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Base64 character set
static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Union to handle byte manipulation for base64 encoding/decoding
typedef union {
    uint8_t bytes[3];
    struct {
        uint32_t b1 : 8;
        uint32_t b2 : 8;
        uint32_t b3 : 8;
    } parts;
    uint32_t value;
} triple_union;

// Union for handling 4 base64 characters during decoding
typedef union {
    uint8_t bytes[4];
    struct {
        uint32_t c1 : 8;
        uint32_t c2 : 8;
        uint32_t c3 : 8;
        uint32_t c4 : 8;
    } chars;
    uint32_t value;
} quad_union;

// Function to get base64 index from character
static int base64_index(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1; // Invalid character or padding
}

// Base64 encode function
int base64_encode(const char *input_file, const char *output_file) {
    FILE *fin = fopen(input_file, "rb");
    if (!fin) {
        fprintf(stderr, "Error: Cannot open input file '%s'\n", input_file);
        return -1;
    }

    FILE *fout = fopen(output_file, "wb");
    if (!fout) {
        fprintf(stderr, "Error: Cannot open output file '%s'\n", output_file);
        fclose(fin);
        return -1;
    }

    triple_union tu;
    unsigned char buffer[3];
    int bytes_read;

    while ((bytes_read = fread(buffer, 1, 3, fin)) > 0) {
        // Clear the union
        tu.value = 0;
        
        // Set bytes in union
        for (int i = 0; i < bytes_read; i++) {
            tu.bytes[i] = buffer[i];
        }

        // Encode based on number of bytes read
        if (bytes_read == 3) {
            fputc(base64_chars[(tu.bytes[0] >> 2) & 0x3F], fout);
            fputc(base64_chars[((tu.bytes[0] & 0x03) << 4) | ((tu.bytes[1] >> 4) & 0x0F)], fout);
            fputc(base64_chars[((tu.bytes[1] & 0x0F) << 2) | ((tu.bytes[2] >> 6) & 0x03)], fout);
            fputc(base64_chars[tu.bytes[2] & 0x3F], fout);
        } else if (bytes_read == 2) {
            fputc(base64_chars[(tu.bytes[0] >> 2) & 0x3F], fout);
            fputc(base64_chars[((tu.bytes[0] & 0x03) << 4) | ((tu.bytes[1] >> 4) & 0x0F)], fout);
            fputc(base64_chars[(tu.bytes[1] & 0x0F) << 2], fout);
            fputc('=', fout);
        } else if (bytes_read == 1) {
            fputc(base64_chars[(tu.bytes[0] >> 2) & 0x3F], fout);
            fputc(base64_chars[(tu.bytes[0] & 0x03) << 4], fout);
            fputc('=', fout);
            fputc('=', fout);
        }
    }

    fclose(fin);
    fclose(fout);
    return 0;
}

// Base64 decode function
int base64_decode(const char *input_file, const char *output_file) {
    FILE *fin = fopen(input_file, "r");
    if (!fin) {
        fprintf(stderr, "Error: Cannot open input file '%s'\n", input_file);
        return -1;
    }

    FILE *fout = fopen(output_file, "wb");
    if (!fout) {
        fprintf(stderr, "Error: Cannot open output file '%s'\n", output_file);
        fclose(fin);
        return -1;
    }

    quad_union qu;
    int indices[4];
    int c;
    int pos = 0;

    while (1) {
        // Read 4 base64 characters, skipping whitespace
        pos = 0;
        while (pos < 4) {
            c = fgetc(fin);
            if (c == EOF) {
                if (pos > 0) {
                    fprintf(stderr, "Error: Invalid base64 encoding (incomplete group)\n");
                    fclose(fin);
                    fclose(fout);
                    return -1;
                }
                goto done;
            }
            // Skip whitespace
            if (c == '\n' || c == '\r' || c == ' ' || c == '\t') {
                continue;
            }
            
            if (c == '=') {
                indices[pos++] = -1; // Padding
            } else {
                int idx = base64_index(c);
                if (idx < 0) {
                    fprintf(stderr, "Error: Invalid base64 character '%c'\n", c);
                    fclose(fin);
                    fclose(fout);
                    return -1;
                }
                indices[pos++] = idx;
            }
        }

        // Decode the 4 characters
        // Clear union
        qu.value = 0;
        
        // Reconstruct the 24-bit value
        uint32_t combined = 0;
        int padding = 0;
        
        if (indices[0] >= 0) combined |= (indices[0] << 18);
        if (indices[1] >= 0) combined |= (indices[1] << 12);
        if (indices[2] >= 0) combined |= (indices[2] << 6);
        if (indices[3] >= 0) combined |= indices[3];
        
        // Count padding
        if (indices[3] < 0) padding++;
        if (indices[2] < 0) padding++;
        
        // Write decoded bytes
        qu.bytes[0] = (combined >> 16) & 0xFF;
        qu.bytes[1] = (combined >> 8) & 0xFF;
        qu.bytes[2] = combined & 0xFF;
        
        fputc(qu.bytes[0], fout);
        if (padding < 2) {
            fputc(qu.bytes[1], fout);
        }
        if (padding < 1) {
            fputc(qu.bytes[2], fout);
        }
    }

done:
    fclose(fin);
    fclose(fout);
    return 0;
}

void print_usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s [-e/-d] <input_file> <output_file>\n", prog_name);
    fprintf(stderr, "  -e: Encode file to base64\n");
    fprintf(stderr, "  -d: Decode base64 file\n");
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        print_usage(argv[0]);
        return 1;
    }

    char *mode = argv[1];
    char *input_file = argv[2];
    char *output_file = argv[3];

    if (strcmp(mode, "-e") == 0) {
        if (base64_encode(input_file, output_file) == 0) {
            printf("Successfully encoded '%s' to '%s'\n", input_file, output_file);
            return 0;
        }
    } else if (strcmp(mode, "-d") == 0) {
        if (base64_decode(input_file, output_file) == 0) {
            printf("Successfully decoded '%s' to '%s'\n", input_file, output_file);
            return 0;
        }
    } else {
        fprintf(stderr, "Error: Invalid mode '%s'. Use -e for encode or -d for decode.\n", mode);
        print_usage(argv[0]);
        return 1;
    }

    return 1;
}
