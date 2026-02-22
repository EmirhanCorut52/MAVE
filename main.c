#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <x264.h>

#define WIDTH 1080
#define HEIGHT 1920
#define THRESHOLD 5

static int write_nals(FILE *file_out, x264_nal_t *nals, int i_nals) {
    for (int i = 0; i < i_nals; i++) {
        if (fwrite(nals[i].p_payload, 1, nals[i].i_payload, file_out) != (size_t)nals[i].i_payload) {
            return -1;
        }
    }
    return 0;
}

long calculate_frame_difference(uint8_t *frame1, uint8_t *frame2, int size) {
    long total_diff = 0;

    if (size == 0) return 0;

    for (int i = 0; i < size; i++) {
        int diff = abs((int)frame1[i] - (int)frame2[i]);
        total_diff += diff;
    }

    return total_diff / size;
}

int main(int argc, char *argv[]) {

    if (argc < 2) {
        printf("Dosya ismi girilmedi");
        return -1;
    }

    const char *base_name = argv[1];
    char yuv_path[256], h264_path[256], tc_path[256], aac_path[256], mkv_path[256], mov_path[256];
    char cmd_aac[2048], cmd_mkv[2048], cmd_yuv[2048];

    snprintf(yuv_path, sizeof(yuv_path), "yuv/%s.yuv", base_name);
    snprintf(h264_path, sizeof(h264_path), "h264/%s.h264", base_name);
    snprintf(tc_path, sizeof(tc_path), "time/%s.txt", base_name);
    snprintf(aac_path, sizeof(aac_path), "aac/%s.aac", base_name);
    snprintf(mkv_path, sizeof(mkv_path), "mkv/%s.mkv", base_name);
    snprintf(mov_path, sizeof(mov_path), "mov/%s.mov", base_name);
    snprintf(cmd_yuv, sizeof(cmd_yuv), "ffmpeg -y -i '%s' -c:v rawvideo -pix_fmt yuv420p -s %dx%d '%s' -loglevel warning", mov_path, WIDTH, HEIGHT, yuv_path);
    system(cmd_yuv);

    FILE *file_in = fopen(yuv_path, "rb");
    FILE *file_out = fopen(h264_path, "wb");
    FILE *file_tc = fopen(tc_path, "w");

    if (!file_in || !file_out || !file_tc) {
        printf("Dosyalar Acilamadi!");
        if (file_in) fclose(file_in);
        if (file_out) fclose(file_out);
        if (file_tc) fclose(file_tc);
        return -1;
    }

    fprintf(file_tc, "# timecode format v2\n");

    x264_param_t param;
    x264_param_default_preset(&param, "medium", "zerolatency");

    param.i_width = WIDTH;
    param.i_height = HEIGHT;
    param.i_fps_num = 30;
    param.i_fps_den = 1;
    param.i_timebase_num = 1;
    param.i_timebase_den = 30;
    param.b_vfr_input = 1;
    param.i_bframe = 0;

    x264_t *encoder = x264_encoder_open(&param);

    if (!encoder) {
        printf("x264 Baslamadi!");
        return -1;
    }

    x264_picture_t pic_in, pic_out;
    if (x264_picture_alloc(&pic_in, X264_CSP_I420, WIDTH, HEIGHT) != 0) {
        printf("x264_picture_alloc basarisiz!\n");
        x264_encoder_close(encoder);
        fclose(file_in);
        fclose(file_out);
        fclose(file_tc);
        return -1;
    }

    int y_size = WIDTH * HEIGHT;

    uint8_t *prev_frame_y = (uint8_t*)calloc(y_size, 1);
    if (!prev_frame_y) {
        printf("Bellek ayrilamadi!\n");
        x264_picture_clean(&pic_in);
        x264_encoder_close(encoder);
        fclose(file_in);
        fclose(file_out);
        fclose(file_tc);
        return -1;
    }

    int frame_count = 0;
    int encoded_count = 0;

    while (fread(pic_in.img.plane[0], 1, y_size, file_in) == y_size) {
        if (fread(pic_in.img.plane[1], 1, y_size / 4, file_in) != (size_t)(y_size / 4)) break;
        if (fread(pic_in.img.plane[2], 1, y_size / 4, file_in) != (size_t)(y_size / 4)) break;

        frame_count ++;

        long diff = calculate_frame_difference(prev_frame_y, pic_in.img.plane[0], y_size);

        if (diff < THRESHOLD && frame_count > 1) {
            printf("Kare: %d\t Fark: %ld ATILDI\n", frame_count, diff);
        }

        else {
            pic_in.i_pts = frame_count - 1;

            printf("Kare: %d\t Fark: %ld\t PTS: %lld ALINDI\n", frame_count, diff, (long long int)pic_in.i_pts);

            x264_nal_t *nals;
            int i_nals;

            int frame_bytes = x264_encoder_encode(encoder, &nals, &i_nals, &pic_in, &pic_out);

            if (frame_bytes > 0) {
                if (write_nals(file_out, nals, i_nals) != 0) {
                    printf("H264 yazma hatasi!\n");
                    break;
                }
                encoded_count++;

                long long ms = (long long)(pic_out.i_pts * 1000.0 / 30.0);
                fprintf(file_tc, "%lld\n", ms);
            }
            else if (frame_bytes < 0) {
                printf("x264 encode hatasi!\n");
                break;
            }
        }

        memcpy(prev_frame_y, pic_in.img.plane[0], y_size);
    }

    while (1) {
        x264_nal_t *nals;
        int i_nals;
        int frame_bytes = x264_encoder_encode(encoder, &nals, &i_nals, NULL, &pic_out);
        
        if (frame_bytes <= 0)
            break;

        if (write_nals(file_out, nals, i_nals) != 0) {
            printf("H264 yazma hatasi!\n");
            break;
        }
        encoded_count++;

        long long ms = (long long)(pic_out.i_pts * 1000.0 / 30.0);
        fprintf(file_tc, "%lld\n", ms);
    }

    printf("Toplam Kare: %d\n", frame_count);
    printf("Encode olan kare: %d\n", encoded_count);
    if (frame_count > 0) {
        float saved_percent = (1.0f - ((float)encoded_count / (float)frame_count)) * 100.0f;
        printf("Tasarruf Orani: %% %.2f\n", saved_percent);
    } else {
        printf("Tasarruf Orani: %% 0.00\n");
    }

    x264_encoder_close(encoder);
    x264_picture_clean(&pic_in);
    free(prev_frame_y);
    fclose(file_in);
    fclose(file_out);
    fclose(file_tc);

    snprintf(cmd_aac, sizeof(cmd_aac), "ffmpeg -y -i '%s' -vn -c:a aac '%s' -loglevel warning", mov_path, aac_path);
    system(cmd_aac);
    snprintf(cmd_mkv, sizeof(cmd_mkv), "mkvmerge -o '%s' --timestamps 0:'%s' '%s' '%s'", mkv_path, tc_path, h264_path, aac_path);
    system(cmd_mkv);

    return 0;
}