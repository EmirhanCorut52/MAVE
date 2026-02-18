# .YUV OUTPUT

### ffmpeg -i mov/.mov -c:v rawvideo -s 1080x1920 yuv/.yuv
### ffmpeg -i mov/.mov -c:v rawvideo -pix_fmt yuv420p -s 1080x1920 yuv/.yuv

# .MKV OUTPUT

### mkvmerge -o mkv/.mkv --timestamps 0:times/.txt h264/.h264 aac/.aac

# .AAC OUTPUT

### ffmpeg -i mov/.mov -vn -c:a aac aac/.aac