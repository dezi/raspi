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

ffmpeg: ffmpeg-win32 ffmpeg-local

ffmpeg-local:
	make -f Makefile.ffmpeg-local

ffmpeg-win32:
	make -f Makefile.ffmpeg-win32

dezi:
	make -f Makefile.dezi

spon:
	make -f Makefile.spon
