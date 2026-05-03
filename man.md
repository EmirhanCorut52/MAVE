# .YUV OUTPUT

## NORMAL

ffmpeg -i input/ -c:v rawvideo -s 1920x1080 yuv/.yuv

## FORCE

ffmpeg -i input/ -c:v rawvideo -pix_fmt yuv420p -s 1920x1080 yuv/.yuv

# .MKV OUTPUT

mkvmerge -o mkv/.mkv --timestamps 0:times/.txt h264/.h264 aac/.aac

# COMPILE CODE 

gcc main.c -o vfr -lx264 -Wall

# EXECUTE CODE 

./vfr yuv/.yuv v-pixel h-pixel fps