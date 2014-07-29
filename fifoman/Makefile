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
		-l:libjpeg.a -l:libpng12.a -l:libzip.a -l:libz.a -lm -lpthread \
		-o fifoman

centos:
	gcc fifoman.c -O3 \
		-I /usr/local/include \
		-I /usr/local/lib/libzip/include \
		-I /usr/lib/arm-linux-gnueabihf/libzip/include \
		-I /usr/local/include \
		-L /usr/local/lib \
		-lavfilter_pure -lavformat_pure \
		-lavcodec_pure -lswresample_pure \
		-lswscale_pure -lavutil_pure \
		-l:libjpeg.a -l:libpng12.a -l:libzip.a -l:libz.a -lrt -lm -lpthread \
		-o fifoman

osx:
	gcc fifoman.c -g \
		-I /usr/local/include \
		-I /usr/local/lib/libzip/include \
		-I /usr/lib/arm-linux-gnueabihf/libzip/include \
		-I /usr/local/include \
		-L /usr/local/lib \
		-lavfilter_pure -lavformat_pure \
		-lavcodec_pure -lswresample_pure \
		-lswscale_pure -lavutil_pure \
		/usr/local/lib/libiconv.a \
		/usr/local/lib/libjpeg.a \
		/usr/local/lib/libpng12.a \
		/usr/local/lib/libzip.a \
		/usr/local/lib/libbz2.a \
		/usr/local/lib/libz.a \
		-lm -lpthread \
		-o fifoman

