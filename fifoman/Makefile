all:
	gcc fifoman.c -g \
		-I /usr/local/include \
		-I /usr/local/lib/libzip/include \
		-I /usr/lib/arm-linux-gnueabihf/libzip/include \
		-I /usr/local/include \
		-L /usr/local/lib \
		-lavfilter_pure -lavformat_pure \
		-lavcodec_pure -lswresample_pure \
		-lswscale_pure -lavutil_pure \
		-liconv -ljpeg -lzip -lbz2 -lz -lm -lpthread \
		-o fifoman

