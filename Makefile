##########################################################################
#
# Dezi's linux mock ups.
#

nix:
	@echo "Please give target..."

bashvi:
	make -f Makefile.bashvi

raspberrypi:
	make -f Makefile.raspberrypi

centos:
	make -f Makefile.centos

local:
	make -f Makefile.local

ffmpeg: ffmpeg-cross ffmpeg-local

ffmpeg-cross: ffmpeg-win32-doit ffmpeg-win64-doit

ffmpeg-win32-doit:
	make -f Makefile.ffmpeg-cross ffmpeg-win32-doit 

ffmpeg-win64-doit:
	make -f Makefile.ffmpeg-cross ffmpeg-win64-doit

ffmpeg-local:
	make -f Makefile.ffmpeg-local

dezi:
	make -f Makefile.dezi

spon:
	make -f Makefile.spon
