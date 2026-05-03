#define _FILE_OFFSET_BITS 64
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <x264.h>

#define BLOCK_SIZE 16

#define PIXEL_THRESHOLD 35
#define BLOCK_COUNT_THRESHOLD 100

#define IGNORE_TOP_BLOCKS 6
#define IGNORE_BOTTOM_BLOCKS 6
#define IGNORE_LEFT_BLOCKS 4
#define IGNORE_RIGHT_BLOCKS 4

static int write_nals(FILE *file_out, x264_nal_t *nals, int i_nals) {
    for (int i = 0; i < i_nals; i++) {
        if (fwrite(nals[i].p_payload, 1, nals[i].i_payload, file_out) != (size_t)nals[i].i_payload) {
            return -1;
        }
    }
    return 0;
}

int calculate_frame_difference(uint8_t *frame1, uint8_t *frame2, int width, int height) {
    int active_blocks = 0;
    int start_y = IGNORE_TOP_BLOCKS * BLOCK_SIZE;
    int end_y = height - (IGNORE_BOTTOM_BLOCKS * BLOCK_SIZE);
    int start_x = IGNORE_LEFT_BLOCKS * BLOCK_SIZE;
    int end_x = width - (IGNORE_RIGHT_BLOCKS * BLOCK_SIZE);

    for (int by = start_y; by < end_y; by += BLOCK_SIZE) {
        for (int bx = start_x; bx < end_x; bx += BLOCK_SIZE) {
            
            uint32_t block_diff = 0;
            int pixel_count = 0;
            int current_block_height = (by + BLOCK_SIZE > end_y) ? (end_y - by) : BLOCK_SIZE;
            int current_block_width = (bx + BLOCK_SIZE > end_x) ? (end_x - bx) : BLOCK_SIZE;

            for (int y = 0; y < current_block_height; y++) {
                int offset_y = (by + y) * width;
                for (int x = 0; x < current_block_width; x++) {
                    int offset = offset_y + (bx + x);
                    block_diff += abs((int)frame1[offset] - (int)frame2[offset]);
                    pixel_count++;
                }
            }

            int avg_block_diff = block_diff / pixel_count;
            
            if (avg_block_diff > PIXEL_THRESHOLD) {
                active_blocks++;
            }
        }
    }

    return active_blocks;
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Hatali arguman sayisi\n");
        printf("Kullanim: <input.yuv> <yatay piksel> <dikey piksel> <fps>\n");
        return -1;
    }

    char *endptr;

    errno = 0;
    long width_long = strtol(argv[2], &endptr, 10);
    if (errno == ERANGE || *endptr != '\0' || width_long <= 0 || width_long > 10000) {
        printf("Gecersiz genislik degeri\n");
        return -1;
    }

    errno = 0;
    long height_long = strtol(argv[3], &endptr, 10);
    if (errno == ERANGE || *endptr != '\0' || height_long <= 0 || height_long > 10000) {
        printf("Gecersiz yukseklik degeri\n");
        return -1;
    }

    errno = 0;
    long fps_long = strtol(argv[4], &endptr, 10);
    if (errno == ERANGE || *endptr != '\0' || fps_long <= 0 || fps_long > 1000) {
        printf("Gecersiz fps degeri\n");
        return -1;
    }

    int WIDTH = (int)width_long;
    int HEIGHT = (int)height_long;
    int FPS = (int)fps_long;

    if (WIDTH % 2 != 0 || HEIGHT % 2 != 0) {
        printf("Genislik ve yukseklik cift sayi olmalidir\n");
        return -1;
    }

    char h264_path[256], tc_path[256], mkv_path[256];
    char base_name[242], tc_input_arg[260];

    const char *yuv_path = argv[1];
    const char *last_slash = strrchr(yuv_path, '/');
    const char *filename = last_slash ? last_slash + 1 : yuv_path;
    const char *dot = strrchr(filename, '.');
    size_t name_len = dot ? (size_t)(dot - filename) : strlen(filename);

    if (name_len == 0 || name_len > sizeof(base_name) - 1) {
        printf("Dosya ismi gecersiz veya cok uzun\n");
        return -1;
    }

    strncpy(base_name, filename, name_len);
    base_name[name_len] = '\0';

    snprintf(h264_path, sizeof(h264_path), "h264/%s.h264", base_name);
    snprintf(tc_path, sizeof(tc_path), "timecode/%s.txt", base_name);
    snprintf(mkv_path, sizeof(mkv_path), "mkv/%s.mkv", base_name);

    struct stat st = {0};
    if (stat("h264", &st) == -1) mkdir("h264", 0755);
    if (stat("timecode", &st) == -1) mkdir("timecode", 0755);
    if (stat("mkv", &st) == -1) mkdir("mkv", 0755);

    FILE *file_in = fopen(yuv_path, "rb");
    FILE *file_out = fopen(h264_path, "wb");
    FILE *file_tc = fopen(tc_path, "w");

    if (!file_in || !file_out || !file_tc) {
        printf("Dosyalar acilamadi\n");
        if (file_in) fclose(file_in);
        if (file_out) fclose(file_out);
        if (file_tc) fclose(file_tc);
        return -1;
    }

    if (fprintf(file_tc, "# timecode format v4\n") < 0) {
        printf("Timecode yazma hatasi\n");
        fclose(file_in);
        fclose(file_out);
        fclose(file_tc);
        return -1;
    }

    x264_param_t param;
    x264_param_default_preset(&param, "medium", NULL);
    param.i_width = WIDTH;
    param.i_height = HEIGHT;
    param.i_fps_num = FPS;
    param.i_fps_den = 1;
    param.i_csp = X264_CSP_I420;
    param.i_timebase_num = 1;
    param.i_timebase_den = FPS;
    param.b_vfr_input = 1;
    param.i_bframe = 3;
    param.b_repeat_headers = 1;
    param.b_annexb = 1;

    x264_t *encoder = x264_encoder_open(&param);

    if (!encoder) {
        printf("x264 baslamadi\n");
        fclose(file_in);
        fclose(file_out);
        fclose(file_tc);
        return -1;
    }

    x264_picture_t pic_in, pic_out;

    if (x264_picture_alloc(&pic_in, param.i_csp, param.i_width, param.i_height) != 0) {
        printf("x264_picture_alloc basarisiz\n");
        x264_encoder_close(encoder);
        fclose(file_in);
        fclose(file_out);
        fclose(file_tc);
        return -1;
    }

    int y_size = WIDTH * HEIGHT;
    uint8_t *prev_frame_y = (uint8_t*)calloc(y_size, 1);

    if (!prev_frame_y) {
        printf("Bellek ayrilamadi\n");
        x264_picture_clean(&pic_in);
        x264_encoder_close(encoder);
        fclose(file_in);
        fclose(file_out);
        fclose(file_tc);
        return -1;
    }

    int64_t frame_count = 0;
    int64_t encoded_count = 0;
    int encode_error = 0;
    int skipped_frames = 0;

    while (fread(pic_in.img.plane[0], 1, y_size, file_in) == (size_t)y_size) {

        if (fread(pic_in.img.plane[1], 1, y_size / 4, file_in) != (size_t)(y_size / 4)) {
            printf("U duzlemi okunamadi\n");
            encode_error = 1;
            break;
        }

        if (fread(pic_in.img.plane[2], 1, y_size / 4, file_in) != (size_t)(y_size / 4)) {
            printf("V duzlemi okunamadi\n");
            encode_error = 1;
            break;
        }

        frame_count ++;
        int diff = calculate_frame_difference(prev_frame_y, pic_in.img.plane[0], WIDTH, HEIGHT);

        if (diff < BLOCK_COUNT_THRESHOLD && frame_count > 1 && skipped_frames < FPS) {
            skipped_frames++;
            printf("Kare: %lld\t Fark: %d ATILDI\n", (long long)frame_count, diff);
        }

        else {
            pic_in.i_pts = frame_count - 1;

            printf("Kare: %lld\t Fark: %d\t PTS: %lld ALINDI\n",(long long)frame_count, diff, (long long)pic_in.i_pts);

            double ms = (double)(pic_in.i_pts * 1000.0 / FPS);
            
            if (fprintf(file_tc, "%.3f\n", ms) < 0) {
                printf("Timecode yazma hatasi\n");
                encode_error = 1;
                break;
            }

            x264_nal_t *nals;
            int i_nals;
            int frame_bytes = x264_encoder_encode(encoder, &nals, &i_nals, &pic_in, &pic_out);

            if (frame_bytes < 0) {
                printf("x264 encode hatasi\n");
                encode_error = 1;
                break;
            }

            else if (frame_bytes > 0) {
                if (write_nals(file_out, nals, i_nals) != 0) {
                    printf("H264 yazma hatasi\n");
                    encode_error = 1;
                    break;
                }

                encoded_count++;
                skipped_frames = 0;
            }
            memcpy(prev_frame_y, pic_in.img.plane[0], y_size);
        }
    }

    while (!encode_error && x264_encoder_delayed_frames(encoder) > 0) {
        x264_nal_t *nals;
        int i_nals;
        int frame_bytes = x264_encoder_encode(encoder, &nals, &i_nals, NULL, &pic_out);

        if (frame_bytes < 0) {
            printf("x264 flush hatasi\n");
            encode_error = 1;
            break;
        }

        if (frame_bytes == 0) {
            continue;
        }

        if (write_nals(file_out, nals, i_nals) != 0) {
            printf("H264 yazma hatasi\n");
            encode_error = 1;
            break;
        }

        encoded_count++;
    }

    printf("Toplam kare: %lld\n", (long long)frame_count);
    printf("Encode olan kare: %lld\n", (long long)encoded_count);

    if (frame_count > 0) {
        float saved_percent = (1.0f - ((float)encoded_count / (float)frame_count)) * 100.0f;
        printf("Kare tasarruf orani: %% %.2f\n", saved_percent);
    }

    else {
        printf("Kare sayisi 0, tasarruf orani hesaplanamadi\n");
    }

    free(prev_frame_y);
    x264_encoder_close(encoder);
    x264_picture_clean(&pic_in);
    fclose(file_in);
    fclose(file_out);
    fclose(file_tc);

    if (encode_error) {
        printf("Encode islemi hatayla sonlandi\n");
        return -1;
    }

    snprintf(tc_input_arg, sizeof(tc_input_arg), "0:%s", tc_path);
    char *mkvmerge_args[] = {
        "mkvmerge", "-o", mkv_path, "--timestamps", tc_input_arg, h264_path, NULL
    };

    pid_t pid = fork();
    if (pid < 0) {
        printf("mkvmerge baslatilamadi\n");
        return -1;
    }

    if (pid == 0) {
        execvp(mkvmerge_args[0], mkvmerge_args);
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        printf("mkvmerge komutu bulunamadi\n");
        return -1;
    }

    return 0;
}