#!/bin/sh

rm -f Kappa.out.* Kappa.inp.* Kappa.xxx.*

mkfifo Kappa.out.1.1.yuv.y4m
mkfifo Kappa.out.1.1.pcm.s16le

mkfifo Kappa.inp.1.2.yuv.size~1024x576.logo~Spiegel_Online.y4m
mkfifo Kappa.out.1.2.264.mkv

mkfifo Kappa.inp.1.3.264.mkv
mkfifo Kappa.inp.1.3.pcm.s16le

ffmpeg-static -y -vcodec h264 -i ./Test-SD.mp4 \
	-f yuv4mpegpipe -r 25 -vf yadif=0 -pix_fmt yuv420p Kappa.out.1.1.yuv.y4m \
	-f s16le        -ac 2 -ar 48000                    Kappa.out.1.1.pcm.s16le \
	> Kappa.1.log 2>&1 &

x264 --profile baseline --preset veryfast --bitrate 4000 --demuxer y4m --muxer mkv --sar 1:1 \
	-o Kappa.out.1.2.264.mkv \
	   Kappa.inp.1.2.yuv.size~1024x576.logo~Spiegel_Online.y4m \
	> Kappa.2.log 2>&1 &

ffmpeg -y \
	-f s16le -ac 2 -ar 48000 -probesize 32 -i Kappa.inp.1.3.pcm.s16le \
	-f matroska                            -i Kappa.inp.1.3.264.mkv \
	-strict -2 -vcodec copy -f mp4 output.mp4

