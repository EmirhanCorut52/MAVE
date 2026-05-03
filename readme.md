# MAVE (Motion Aware VFR Encoder)

MAVE is a custom Variable Frame Rate (VFR) H.264 encoder written in C, designed to significantly reduce video file sizes. 

It works by analyzing the luminance (Luma/Y channel) differences between consecutive frames in raw YUV video. If no significant motion is detected (e.g., in static scenes, presentations, or talking-head videos), MAVE drops the redundant frames. To maintain perfect audio and video synchronization, it generates a Timecode v4 file and automatically muxes the final output into an MKV container.

## 🚀 Features

*   **Smart Frame Dropping:** Distinguishes between natural camera noise and actual motion using customizable pixel and block thresholds.
*   **Margin Ignore:** Excludes the edges of the video (where logos, subtitles, or edge-flickering might occur) from motion calculation to prevent false positives.
*   **Timecode V4 Generation:** Creates precise, millisecond-accurate timestamps for every encoded frame to prevent audio/video desync.
*   **Automated Muxing:** Uses `mkvmerge` to bundle the `.h264` video stream and timecodes into a final `.mkv` file.

## 📋 Prerequisites

Before using MAVE, ensure you have the following installed on your system:
*   **GCC:** For compiling the C code.
*   **libx264:** The H.264 encoder library (`libx264-dev`).
*   **FFmpeg:** For extracting raw YUV data from your source videos.
*   **MKVToolNix (`mkvmerge`):** For muxing the final MKV file.

## 🛠️ Compilation

Compile the source code using `gcc` with the x264 library linked:
```bash
gcc main.c -o vfr -lx264 -Wall
```

## 📖 Usage Pipeline

The standard workflow consists of three main steps: extracting the raw video, encoding it with MAVE, and (optionally) manually muxing it with audio.

*Note: Ensure the required directories (`yuv/`, `h264/`, `timecode/`, `mkv/`, `aac/`) exist in your working environment before running the commands.*

### Step 1: Extract YUV Output
First, convert your source video into raw YUV format using FFmpeg.

**Normal Extraction:**
```bash
ffmpeg -i input/<filename>.mp4 -c:v rawvideo -s 1920x1080 yuv/<filename>.yuv
```

**Force Pixel Format (Recommended):**
To ensure compatibility with MAVE (which expects I420 color space), it is highly recommended to force the pixel format:
```bash
ffmpeg -i input/<filename>.mp4 -c:v rawvideo -pix_fmt yuv420p -s 1920x1080 yuv/<filename>.yuv
```

### Step 2: Execute MAVE (Encode)
Run the compiled `vfr` executable by providing the input file, horizontal pixels (width), vertical pixels (height), and the original framerate.
```bash
./vfr yuv/<filename>.yuv <width> <height> <fps>
```
*Example for a 1080p 60FPS video:*
```bash
./vfr yuv/video.yuv 1920 1080 60
```

### Step 3: Muxing (MKV Output)
While the C code automatically attempts to trigger `mkvmerge` for the video and timecodes, you can manually mux the files if you want to include an audio track (like AAC) that you extracted separately:
```bash
mkvmerge -o mkv/<filename>.mkv --timestamps 0:timecode/<filename>.txt h264/<filename>.h264 aac/<filename>.aac
```

## ⚙️ Developer Settings (Tuning)

You can adjust the sensitivity of the motion detection by modifying the macros inside `main.c` before compiling:

*   `PIXEL_THRESHOLD` (e.g., 35): The average difference in brightness required for a block of pixels to be considered "changed". Higher values make it less sensitive to noise but might miss subtle movements.
*   `BLOCK_COUNT_THRESHOLD` (e.g., 100): The minimum number of changed blocks required in a single frame to trigger an encode. If the changed blocks are below this number, the frame is dropped.
```