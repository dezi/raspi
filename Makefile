##########################################################################
#
# Dezi's linux mock ups.
#

nix:
	@echo "Please give target..."

bashvi:
	make -f Makefile.bashvi all

upgrade:
	make -f Makefile.raspberrypi upgrade

osx:
	make -f Makefile.osx

centos:
	make -f Makefile.centos

raspberrypi:
	make -f Makefile.raspberrypi

dvbstick:
	make -f Makefile.dvbstick dvbstick-doit

local:
	make -f Makefile.local local-doit

xberry:
	make -f Makefile.xberry xberry-doit

xbmc:
	make -f Makefile.xbmc xbmc-doit

xberry:
	make -f Makefile.xodroid xberry-doit

xodroid:
	make -f Makefile.xodroid xodroid-doit

updatepacks:
	make -f Makefile.devel updatepacks

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
