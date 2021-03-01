# RTMP-pro

```bash
#从文件提取pcm数据
ffmpeg -i buweishui.mp3 -ar 48000 -ac 2 -f s16le 48000_2_s16le.pcm
# 从文件提取yuv数据
ffmpeg -i 720x480_25fps.mp4 -an -c:v rawvideo -pix_fmt yuv420p 720x480_25fps_420p.yuv
```
