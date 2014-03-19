all:
	gcc fifoman.c -g \
		-I /usr/local/include \
		-I /usr/lib/arm-linux-gnueabihf/libzip/include \
		-lavfilter_pure -lavformat_pure \
		-lavcodec_pure -lswresample_pure \
		-lswscale_pure -lavutil_pure \
		-ljpeg -lzip -lbz2 -lz -lm -lpthread \
		-o fifoman

