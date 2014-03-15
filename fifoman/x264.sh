#!/bin/sh

rm -f Kappa.out.* Kappa.inp.* Kappa.xxx.*

mkfifo Kappa.out.1.1.yuv.size~720x576.y4m
mkfifo Kappa.out.1.1.pcm.ac~2.ar~48000.s16le

mkfifo Kappa.inp.1.2.yuv.size~720x576.y4m
mkfifo Kappa.out.1.2.264.size~720x576.mkv

mkfifo Kappa.inp.1.3.264.size~720x576.mkv
mkfifo Kappa.inp.1.3.pcm.ac~2.ar~48000.s16le

ffmpeg -y -i ./Test.mkv \
	-f s16le        -ac 2 -ar 48000                    Kappa.out.1.1.pcm.ac~2.ar~48000.s16le \
	-f yuv4mpegpipe -r 25 -vf yadif=0 -pix_fmt yuv420p Kappa.out.1.1.yuv.size~720x576.y4m \
	> Kappa.1.log 2>&1 &

x264 --profile baseline --preset veryfast --bitrate 4000 --demuxer y4m --muxer mkv \
	-o Kappa.out.1.2.264.size~720x576.mkv \
	   Kappa.inp.1.2.yuv.size~720x576.y4m \
	> Kappa.2.log 2>&1 &

ffmpeg -y \
	-f s16le -ac 2 -ar 48000 -probesize 32 -i Kappa.inp.1.3.pcm.ac~2.ar~48000.s16le \
	-f matroska                            -i Kappa.inp.1.3.264.size~720x576.mkv \
	-vcodec copy -f mp4 Test.mp4

