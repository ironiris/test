#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const int decode_table[256] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
    52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
    -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
    15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
    -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
    41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
};

// UNION: 3 바이트를 한 번에 다루기 위한 구조체
typedef union {
    unsigned char bytes[3];
    unsigned int value;
} triple_t;

// UNION: 4 개의 6 비트 인덱스를 저장하는 구조체
typedef union {
    unsigned char idx[4];  // 4 개의 6 비트 인덱스 (각각 0~63 값 저장)
    unsigned int packed;    // 24 비트 패킹된 값 (하위 24 비트만 사용)
} sextets_t;

int encode_file(const char *input_path, const char *output_path) {
    FILE *fin = fopen(input_path, "rb");
    if (!fin) { perror("입력 파일을 열 수 없습니다"); return 1; }

    FILE *fout = fopen(output_path, "wb");
    if (!fout) { fclose(fin); perror("출력 파일을 열 수 없습니다"); return 1; }

    triple_t triple;
    sextets_t sextets;
    int count;

    while ((count = fread(triple.bytes, 1, 3, fin)) > 0) {
        if (count < 3) {
            for (int i = count; i < 3; i++) triple.bytes[i] = 0;
        }

        // 3 바이트를 24 비트 정수로 (bytes[0]:최상위, bytes[2]:최하위)
        triple.value = ((unsigned int)triple.bytes[0] << 16) |
                       ((unsigned int)triple.bytes[1] << 8) |
                       ((unsigned int)triple.bytes[2]);

        // UNION 을 이용해 24 비트를 6 비트 4 개로 분할
        // idx[0]: 첫 번째 문자 (최상위 6 비트), idx[3]: 네 번째 문자 (최하위 6 비트)
        sextets.idx[0] = (triple.value >> 18) & 0x3F;
        sextets.idx[1] = (triple.value >> 12) & 0x3F;
        sextets.idx[2] = (triple.value >> 6) & 0x3F;
        sextets.idx[3] = triple.value & 0x3F;

        fputc(base64_chars[sextets.idx[0]], fout);
        fputc(base64_chars[sextets.idx[1]], fout);
        fputc(count >= 2 ? base64_chars[sextets.idx[2]] : '=', fout);
        fputc(count >= 3 ? base64_chars[sextets.idx[3]] : '=', fout);
    }

    fclose(fin);
    fclose(fout);
    return 0;
}

int decode_file(const char *input_path, const char *output_path) {
    FILE *fin = fopen(input_path, "r");
    if (!fin) { perror("입력 파일을 열 수 없습니다"); return 1; }

    FILE *fout = fopen(output_path, "wb");
    if (!fout) { fclose(fin); perror("출력 파일을 열 수 없습니다"); return 1; }

    sextets_t sextets;
    triple_t triple;
    int idx = 0;
    int padding = 0;
    
    memset(&sextets, 0, sizeof(sextets));

    int c;
    while ((c = fgetc(fin)) != EOF) {
        if (c == '\n' || c == '\r') continue;
        
        if (c == '=') {
            padding++;
            sextets.idx[idx++] = 0;
        } else {
            int val = decode_table[c];
            if (val == -1) continue;
            sextets.idx[idx++] = (unsigned char)val;
        }

        if (idx == 4) {
            // 4 개의 6 비트 값을 24 비트로 조합
            sextets.packed = ((unsigned int)sextets.idx[0] << 18) |
                             ((unsigned int)sextets.idx[1] << 12) |
                             ((unsigned int)sextets.idx[2] << 6) |
                             ((unsigned int)sextets.idx[3]);

            // UNION 을 통해 24 비트를 3 바이트로 추출
            fwrite(triple.bytes, 1, 3 - padding, fout);
            
            idx = 0;
            padding = 0;
            memset(&sextets, 0, sizeof(sextets));
        }
    }

    fclose(fin);
    fclose(fout);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "사용법: %s [-e/-d] 입력파일 출력파일\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-e") == 0) {
        return encode_file(argv[2], argv[3]);
    } else if (strcmp(argv[1], "-d") == 0) {
        return decode_file(argv[2], argv[3]);
    } else {
        fprintf(stderr, "오류: 모드는 -e(인코딩) 또는 -d(디코딩) 이어야 합니다.\n");
        return 1;
    }
}
