##########################################################################
#
# Dezi's linux mock ups.
#

nix:
	@echo "Please give target..."

bashvi:
	make -f Makefile.bashvi

osx:
	make -f Makefile.osx

centos:
	make -f Makefile.centos

raspberrypi:
	make -f Makefile.raspberrypi

local:
	make -f Makefile.local local-doit

ffmpeg: ffmpeg-cross ffmpeg-local

ffmpeg-cross: ffmpeg-win32-doit ffmpeg-win64-doit

ffmpeg-win32-doit:
	make -f Makefile.ffmpeg-cross ffmpeg-win32-doit 

ffmpeg-win64-doit:
	make -f Makefile.ffmpeg-cross ffmpeg-win64-doit

ffmpeg-local:
	make -f Makefile.ffmpeg-local ffmpeg-local-doit

dezi:
	make -f Makefile.dezi

spon:
	make -f Makefile.spon

collect:
	make -f Makefile.collect
