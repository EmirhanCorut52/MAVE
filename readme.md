# .YUV OUTPUT

ffmpeg -i input/. -c:v rawvideo -s 1920x1080 yuv/.yuv
ffmpeg -i input/. -c:v rawvideo -pix_fmt yuv420p -s 1920x1080 yuv/.yuv

# .MKV OUTPUT

mkvmerge -o mkv/.mkv --timestamps 0:times/.txt h264/.h264 aac/.aac

# .AAC OUTPUT

ffmpeg -i input/. -vn -c:a aac aac/.aac

# COMPILE CODE 

gcc main.c -o vfr -lx264 -Wall

# EXECUTE CODE 

./vfr yuv/.yuv