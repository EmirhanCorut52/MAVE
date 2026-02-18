#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <x264.h>

#define WIDTH 1080
#define HEIGHT 1920
#define THRESHOLD 5

long calculate_frame_difference (uint8_t *frame1, uint8_t *frame2, int size) {
    long total_diff = 0;

    if (size == 0) return 0;

    for (int i = 0; i < size; i++) {
        int diff = abs((int)frame1[i] - (int)frame2[i]);
        total_diff += diff;
    }

    return total_diff / size;
}

int main() {
    FILE *file_in = fopen("yuv/orta.yuv", "rb");
    FILE *file_out = fopen("h264/orta.h264", "wb");
    FILE *file_tc = fopen("times/orta.txt", "w");

    fprintf(file_tc, "# timecode format v2\n");

    if (!file_in || !file_out) {
        printf("Dosyalar Acilamadi!");
        return -1;
    }

    x264_param_t param;
    x264_param_default_preset(&param, "medium", "zerolatency");

    param.i_width = WIDTH;
    param.i_height = HEIGHT;
    param.i_fps_num = 30;
    param.i_fps_den = 1;
    param.i_timebase_num = 1;
    param.i_timebase_den = 30;
    param.b_vfr_input = 1;

    x264_t *encoder = x264_encoder_open(&param);

    if (!encoder) {
        printf("x264 Baslamadi!");
        return -1;
    }

    x264_picture_t pic_in, pic_out;
    x264_picture_alloc(&pic_in, X264_CSP_I420, WIDTH, HEIGHT);

    int y_size = WIDTH * HEIGHT;
    int frame_size = y_size * 3 / 2;

    uint8_t *prev_frame_y = (uint8_t*)calloc(frame_size, 1);

    int frame_count = 0;
    int encoded_count = 0;

    while (fread(pic_in.img.plane[0], 1, y_size, file_in) == y_size) {
        fread(pic_in.img.plane[1], 1, y_size / 4, file_in);
        fread(pic_in.img.plane[2], 1, y_size / 4, file_in);

        frame_count ++;

        long diff = calculate_frame_difference(prev_frame_y, pic_in.img.plane[0], y_size);

        if (diff < THRESHOLD && frame_count > 1) {
            printf("Kare: %d\t Fark: %ld ATLANDI\n", frame_count, diff);
        }

        else {
            pic_in.i_pts = frame_count - 1;

            printf("Kare: %d\t Fark: %ld\t PTS: %lld ALINDI\n", frame_count, diff, (long long int)pic_in.i_pts);

            x264_nal_t *nals;
            int i_nals;

            int frame_bytes = x264_encoder_encode(encoder, &nals, &i_nals, &pic_in, &pic_out);

            if (frame_bytes > 0) {
                fwrite(nals[0].p_payload, 1, frame_bytes, file_out);
                encoded_count++;

                long long ms = (long long)(pic_out.i_pts * 1000.0 / 30.0);
                fprintf(file_tc, "%lld\n", ms);
            }
            
            memcpy (prev_frame_y, pic_in.img.plane[0], y_size);
        }
    }

    while (1) {
        x264_nal_t *nals;
        int i_nals;
        int frame_bytes = x264_encoder_encode(encoder, &nals, &i_nals, NULL, &pic_out);
        
        if (frame_bytes <= 0)
            break;
            
        fwrite(nals[0].p_payload, 1, frame_bytes, file_out);
        encoded_count++;

        long long ms = (long long)(pic_out.i_pts * 1000.0 / 30.0);
        fprintf(file_tc, "%lld\n", ms);
    }

    printf("Toplam Kare: %d\n", frame_count);
    printf("Encode olan kare: %d\n", encoded_count);
    printf("Tasarruf Orani: %% %.2f\n", (float)encoded_count / frame_count * 100.0);

    x264_encoder_close(encoder);
    x264_picture_clean(&pic_in);
    free(prev_frame_y);
    fclose(file_in);
    fclose(file_out);
    fclose(file_tc);

    return 0;
}