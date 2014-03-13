#!/bin/sh

rm -f Kappa.out.* Kappa.in.*

mkfifo Kappa.out.1.1.vr.size~720x576.y4m
mkfifo Kappa.out.1.1.ar.ac~2.ar~48000.s16le
mkfifo Kappa.in.1.2.vr.size~720x576.y4m
mkfifo Kappa.in.1.2.ar.ac~2.ar~48000.s16le

ffmpeg-static -y -i http://os-smart-tv.net/videos/Test.mkv \
	-f s16le        -ac 2 -ar 48000                    Kappa.out.1.1.ar.ac~2.ar~48000.s16le \
	-f yuv4mpegpipe -r 25 -vf yadif=0 -pix_fmt yuv420p Kappa.out.1.1.vr.size~720x576.y4m \
	> Kappa.1.log 2>&1 &

ffmpeg-static -y \
	-f s16le -ac 2 -ar 48000 -probesize 32 -i Kappa.in.1.2.ar.ac~2.ar~48000.s16le \
	-f yuv4mpegpipe                        -i Kappa.in.1.2.vr.size~720x576.y4m \
	-vcodec libx264 -profile:v baseline -preset:v veryfast \
	-vf "drawtext=fontfile=./dejavusans.ttf: \
				  x=(w-text_w)/2:y=(h-text_h-line_h): \
				  fontsize=30:fontcolor=white@0.5:boxcolor=black@0.5:box=1: \
				  rate=25:timecode='10\\:00\\:00\\:00'" \
	-f mp4 xxx.mp4

