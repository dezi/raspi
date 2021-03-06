//
// Kappa FiFo Manager
//

#include <sys/types.h>
#include <sys/param.h>

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <pthread.h>
#include <jpeglib.h>
#include <zip.h>
#include <png.h>

#include "libavutil/common.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "libswscale/swscale.h"
#include "libavformat/avformat.h"

//
// Defines
//

typedef unsigned char byte;

#define MAXSLOTS 64
#define MAXONOFF 64

#define VBUFSIZE 96 * 1024 * 1024
#define RBUFSIZE 16 * 1024 * 1024
#define ABUFSIZE  8 * 1024 * 1024
#define RDWRSIZE	    64 * 1024

//
// Pipe info structure.
//

struct kafifo
{
	//
	// Video related.
	//

	int			isvideo;
	int			isyuv4mpeg;

	int			width;
	int			height;
	int			format;
	int			pixfmt;

	int			crop_top;
	int			crop_left;
	int			crop_width;
	int			crop_height;
	int			crop_center;

	int			aspect_num;
	int			aspect_den;

	int			fps_num;
	int			fps_den;

	char		mode;

	//
	// Audio related.
	//

	int			isaudio;

	int			channels;
	int			rate;

	//
	// Logo related.
	//
	
	char		logo[ MAXPATHLEN ];
	
	int			logo_top;
	int			logo_left;
	int			logo_width;
	int			logo_height;
	int			logo_pixfmt;
	
	uint8_t	   *logo_rgb_data[ 4 ];
	int			logo_rgb_size[ 4 ];

	uint8_t	   *logo_yuv_data[ 4 ];
	int			logo_yuv_size[ 4 ];
	
	uint8_t	   *alphatable;
	
	//
	// Pipe related.
	//
	
	int			haveheader;
	int			havestills;

	char		name[ MAXPATHLEN ];
	int			framesize;
	int			chunksize;
	int			iobytes;
	long long	total;
	int			group;
	int			fd;

	int			bufsiz;
	byte	   *buffer;
	pthread_t	thread;

	//
	// Scene related.
	//

	int			threshold;
	int			minframes;
	int			maxframes;
	int			lastscene;
	
	uint8_t	   *lastframe;
	uint8_t	   *thisframe;
	
	char		scenezipname[ MAXPATHLEN ];
	int			scenecount;
	int			scenemaximg;
	char	   *scenesizes;
	struct zip *scenezip;
	
	AVFrame	   *stillframe;
	uint8_t	   *stillbuffer;
	int			stillalloc;
	int			stillfinal;
	
	//
	// Frame related.
	//

	int			framecount;

	int			wantimage;
	int			wantstill;
	int			wantscene;
	int			wantscale;
	int			wantcrop;
	int			wantlogo;

	int			final_width;
	int			final_height;

	int			finalsize;
	int			scalesize;

	byte	   *finalpixels;
	byte	   *scalepixels;

	AVFrame	   *finalframe;
	AVFrame	   *scaleframe;
	AVFrame	   *outputframe;

	struct SwsContext *sws;
};

typedef struct kafifo kafifo_t;

//
// Globals
//

char	   *kappa_fifo_version 		= "1.0.16";

int			kappa_fifo_pass 		= 1;
int			kappa_fifo_maxframe 	= 0;
int			kappa_fifo_percentdone 	= 0;
int			kappa_fifo_aspect_num 	= 16;
int			kappa_fifo_aspect_den 	= 9;
char	   *kappa_fifo_pipedir		= ".";
char	   *kappa_fifo_fileprefix 	= "output";
char	   *kappa_fifo_stillsizes 	= NULL; // "0x720:0x576:0x480:0x360:0x315:0x135:106x60:80x60";
char	   *kappa_fifo_sceneparam 	= NULL; // "40:10:1000:0x360:0x120";
char	   *kappa_fifo_stillparam 	= NULL; // "20";

double		kappa_fifo_logotime[ MAXONOFF ];
int		    kappa_fifo_logomode[ MAXONOFF ];
double		kappa_fifo_logofade  = 1.0;
int		    kappa_fifo_logooocnt = 0;

int			kappa_fifo_groupscnt = 0;
int			kappa_fifo_groupmore = 0;
char		kappa_fifo_groupname[ MAXSLOTS ][ 64 ];
int			kappa_fifo_grouptodo[ MAXSLOTS ];

int			kappa_fifo_inputscnt = 0;
int			kappa_fifo_inputdone = 0;
kafifo_t	kappa_fifo_inputinfo[ MAXSLOTS ];

int			kappa_fifo_outputscnt = 0;
int			kappa_fifo_outputdone = 0;
kafifo_t	kappa_fifo_outputinfo[ MAXSLOTS ];

//
// Usage print.
//

void kappa_fifo_usage()
{
	fprintf(stderr,"Kappa Fifoman Version %s\n\n",kappa_fifo_version);
	fprintf(stderr,"--pass    1|2\n");
	fprintf(stderr,"--aspect  16:9\n");
	fprintf(stderr,"--pipedir .\n");
	fprintf(stderr,"--prefix  output\n");
	fprintf(stderr,"--frames  number\n");
	fprintf(stderr,"--images  0x720:0x576:0x480:0x360:0x315:0x135:106x60:80x60\n");
	fprintf(stderr,"--scene   40:10:1000:0x360:0x120\n");
	fprintf(stderr,"--stills  number:suffix\n");

	exit(1);
}

//
// Close info structure and deallocated everything.
//

void kappa_fifo_close_info(char *group,kafifo_t *info)
{
	if (info->scenezip)
	{
		zip_close(info->scenezip);
		info->scenezip = NULL;
	}
}

//
// Check and parse fifo name for infos and options.
//

void kappa_fifo_parse_fifoname(char *group,kafifo_t *info)
{
	char *poi;

	if (rindex(info->name,'.') && ! strcmp(rindex(info->name,'.'),".y4m"))
	{
		info->isvideo	 = true;
		info->isyuv4mpeg = true;
	}

	if (rindex(info->name,'.') && ! strcmp(rindex(info->name,'.'),".s16le"))
	{
		info->isaudio = true;
	}

	if ((poi = strstr(info->name,".size~")) != NULL)
	{
		sscanf(poi + 6,"%dx%d",&info->width,&info->height);
	}

	if ((poi = strstr(info->name,".ac~")) != NULL)
	{
		sscanf(poi + 4,"%d",&info->channels);
	}

	if ((poi = strstr(info->name,".ar~")) != NULL)
	{
		sscanf(poi + 4,"%d",&info->rate);
	}

	if ((poi = strstr(info->name,".crop~")) != NULL)
	{
		info->crop_center = 0;
		
		sscanf(poi + 6,"%dx%d~%dx%d",
			&info->crop_left,&info->crop_top,
			&info->crop_width,&info->crop_height
			);
	}

	if ((poi = strstr(info->name,".center~")) != NULL)
	{
		info->crop_center = 1;
		
		sscanf(poi + 8,"%dx%d",
			&info->crop_width,&info->crop_height
			);
	}

	if ((poi = strstr(info->name,".logo~")) != NULL)
	{
		strcpy(info->logo,poi + 6);
		*index(info->logo,'.') = '\0';
	}
}

//
// Check and parse YUV4MPEG header according to
// http://wiki.multimedia.cx/index.php?title=YUV4MPEG2
//

void kappa_fifo_parse_yuv4mpeg(char *group,kafifo_t *info)
{
	if (strncmp(info->buffer,"YUV4MPEG2",9))
	{
		fprintf(stderr,"Pipe name %s is not YUV4MPEG2, exitting now...\n",info->name);
		exit(1);
	}

	byte *headend = index(info->buffer,'\n');
	int	  headlen;

	*headend = '\0';

	headlen = strlen(info->buffer) + 1;
	fprintf(stderr,"Header	output %s %d %s\n",group,headlen,info->buffer);

	*headend = '\n';

	if (strstr(info->buffer," W"))
	{
		info->width = atoi(strstr(info->buffer," W") + 2);
	}

	if (strstr(info->buffer," H"))
	{
		info->height = atoi(strstr(info->buffer," H") + 2);
	}

	if (strstr(info->buffer," F"))
	{
		info->fps_num = atoi(strstr(info->buffer," F") + 2);
		info->fps_den = atoi(strstr(strstr(info->buffer," F") + 2,":") + 1);
	}

	if (strstr(info->buffer," A"))
	{
		info->aspect_num = atoi(strstr(info->buffer," A") + 2);
		info->aspect_den = atoi(strstr(strstr(info->buffer," A") + 2,":") + 1);
	}

	if (strstr(info->buffer," I"))
	{
		info->mode = *(strstr(info->buffer," I") + 2);
	}

	if (strstr(info->buffer," C"))
	{
		info->format = atoi(strstr(info->buffer," C") + 2);

		if (info->format == 420) info->pixfmt = AV_PIX_FMT_YUV420P;
		if (info->format == 422) info->pixfmt = AV_PIX_FMT_YUV422P;
		if (info->format == 444) info->pixfmt = AV_PIX_FMT_YUV444P;
	}

	info->framesize = avpicture_get_size(info->pixfmt,info->width,info->height);
	info->chunksize = info->framesize + strlen("FRAME\n");

	fprintf(stderr,"Header	output %s %d %d %d F%d:%d A%d:%d I%c %s framesize=%d\n",
			group,
			info->width,info->height,
			info->format,
			info->fps_num,info->fps_den,
			info->aspect_num,info->aspect_den,
			info->mode,av_get_pix_fmt_name(info->pixfmt),
			info->framesize);

	//
	// Remove header from buffer.
	//

	memcpy(info->buffer,info->buffer + headlen,info->bufsiz - headlen);
	info->iobytes -= headlen;

	//
	// Reallocate the current buffer to a multiple
	// of the frame chunk size including header.
	//

	int frames = (info->bufsiz / info->chunksize) + 1;

	byte *newbuf = (byte *) malloc(info->chunksize * frames);

	memcpy(newbuf,info->buffer,info->bufsiz);

	free(info->buffer);

	info->buffer = newbuf;
	info->bufsiz = info->chunksize * frames;
	
	info->wantimage = (kappa_fifo_stillsizes != NULL);
	info->wantscene = (kappa_fifo_sceneparam != NULL);
	info->wantstill = (kappa_fifo_stillparam != NULL);
	
	if (info->wantscene)
	{
		int error;
		
		sscanf(kappa_fifo_sceneparam,"%d:%d:%d",&info->threshold,&info->minframes,&info->maxframes);
		
		info->scenesizes = kappa_fifo_sceneparam;
		
		if (info->scenesizes) info->scenesizes = index(info->scenesizes + 1,':');
		if (info->scenesizes) info->scenesizes = index(info->scenesizes + 1,':');
		if (info->scenesizes) info->scenesizes = index(info->scenesizes + 1,':');
		
		if (info->scenesizes)
		{
			info->scenesizes++;
		}
		else
		{
			info->scenesizes = "0x360:0x120";
		}
		
		if (! info->threshold) info->threshold = 40;
		if (! info->minframes) info->minframes = 10;
		if (! info->maxframes) info->maxframes = 60 * 25;
		
		if (info->wantstill)
		{
			char *suffix = index(kappa_fifo_stillparam,':');
			
			if (suffix)
			{
				snprintf(info->scenezipname,sizeof(info->scenezipname),"%s_%s.zip",kappa_fifo_fileprefix,suffix + 1);
			}
			else
			{
				snprintf(info->scenezipname,sizeof(info->scenezipname),"%s_stills.zip",kappa_fifo_fileprefix);
			}
		}
		else
		{
			snprintf(info->scenezipname,sizeof(info->scenezipname),"%s_scene.zip",kappa_fifo_fileprefix);
		}
		
		info->scenezip = zip_open(info->scenezipname,ZIP_CREATE + ZIP_TRUNCATE,&error);
		
		if (error == 0)
		{
			fprintf(stderr,"Header	scene   %s %s => %d %s\n",group,info->scenezipname,error,"Success");
		}
		else
		{
			fprintf(stderr,"Header	scene   %s %s => %d %s\n",group,info->scenezipname,error,zip_strerror(info->scenezip));
		}
		
		if (! info->scenezip)
		{
			fprintf(stderr,"Could not create scene zip %s %d, exitting now...\n",info->scenezipname,error);
			exit(1);
		}
		
		fprintf(stderr,"Scenezip:%s\n",info->scenezipname);
		fflush(stderr);
	}

	if (info->wantstill)
	{
		int numimages = atoi(kappa_fifo_stillparam);
		int intervall = (kappa_fifo_maxframe <= 0) ? (25 * 60) : (kappa_fifo_maxframe / numimages);
		
		info->threshold   = 0;
		info->minframes   = intervall;
		info->maxframes   = intervall;
		info->scenemaximg = numimages;
		
		fprintf(stderr,"Header	still   %s %d %d %d\n",group,kappa_fifo_maxframe,numimages,intervall);
	}
		
	if (info->wantimage || info->wantscene)
	{
		info->stillframe = av_frame_alloc();

		info->stillframe->format = info->pixfmt;
		info->stillframe->width  = info->width;
		info->stillframe->height = info->height;
		
		info->stillalloc  = 256 * 1024;
		info->stillbuffer = (uint8_t *) malloc(info->stillalloc);
	}
	
	info->haveheader = true;
}

//
// Save frame as JPEG image.
//

void kappa_fifo_save_jpeg(char *group,kafifo_t *info,AVFrame *stillframe,char *filename,int isscene)
{
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	
	JSAMPROW row_pointer[ 1 ];
	FILE *outfile;
	
	unsigned char *mem = NULL;
	unsigned long memsize = 0;
	
	cinfo.err = jpeg_std_error(&jerr);	
	jpeg_create_compress(&cinfo);
	
	jpeg_mem_dest(&cinfo,&mem,&memsize);

	cinfo.image_width      = stillframe->width;
	cinfo.image_height     = stillframe->height;
	cinfo.input_components = 3;     
	cinfo.in_color_space   = JCS_RGB;

	jpeg_set_defaults(&cinfo);
	
	jpeg_start_compress(&cinfo,TRUE);

	row_pointer[ 0 ] = stillframe->data[ 0 ];
	
    while (cinfo.next_scanline < cinfo.image_height) 
	{
		jpeg_write_scanlines(&cinfo,row_pointer,1);
		
		row_pointer[ 0 ] += stillframe->linesize[ 0 ];
	}
	
	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);
	
	if (isscene)
	{
		struct zip_source *source = zip_source_buffer(info->scenezip,mem,memsize,1);
		
		char *zipname = strncmp(filename,"../",3) ? filename : (filename + 3);
		
		if (zip_file_add(info->scenezip,zipname,source,ZIP_FL_ENC_UTF_8) < 0)
		{
			fprintf(stderr,"Could not add still to zip %s => %s, exitting now...\n",
					group,info->scenezipname,
					zip_strerror(info->scenezip)
					);
					
			exit(1);
		}
	}
	else
	{
		if ((outfile = fopen(filename,"wb")) == NULL) 
		{
			fprintf(stderr,"Could not create still %s, exitting now...\n",filename);
			exit(1);
		}

		fwrite(mem,memsize,1,outfile);
		fclose(outfile);
		
		free(mem);
		
		fprintf(stderr,"Stillimage:%s\n",filename);
		fflush(stderr);
	}
}

//
// Make single still image.
//

void kappa_fifo_make_still(char *group,kafifo_t *info,int width,int height,int framecount,int onlyoneres)
{
	char jpegfile[ MAXPATHLEN ];
	
	if (framecount >= 0)
	{
		info->scenecount += 1;
		
		if (info->wantstill)
		{
			if (info->scenecount > info->scenemaximg) return;

			long millisecs = (long) (((long long) framecount) * 1000 * info->fps_den / info->fps_num);
			
			if (onlyoneres)
			{
				snprintf(jpegfile,sizeof(jpegfile),"%08d_%08ld.jpg",info->scenecount,millisecs);
			}
			else
			{
				snprintf(jpegfile,sizeof(jpegfile),"%08d_%08ld_%dx%d.jpg",info->scenecount,millisecs,width,height);
			}
		}
		else
		{
			if (onlyoneres)
			{
				snprintf(jpegfile,sizeof(jpegfile),"%s_%08d.jpg",kappa_fifo_fileprefix,framecount);
			}
			else
			{
				snprintf(jpegfile,sizeof(jpegfile),"%s_%08d_%dx%d.jpg",kappa_fifo_fileprefix,framecount,width,height);
			}
		}		
	}
	else
	{
		snprintf(jpegfile,sizeof(jpegfile),"%s_%dx%d.jpg",kappa_fifo_fileprefix,width,height);
	}
	
	fprintf(stderr,"Header	still   %s %s\n",group,jpegfile);

	//
	// Create destination frame
	//
	
	int 	 stillsize   = avpicture_get_size(AV_PIX_FMT_RGB24,width,height);
	uint8_t *stillpixels = malloc(stillsize);
	AVFrame *stillframe  = av_frame_alloc();

	stillframe->format = AV_PIX_FMT_RGB24;
	stillframe->width  = width;
	stillframe->height = height;

	avpicture_fill((AVPicture *) stillframe,
		stillpixels,
		AV_PIX_FMT_RGB24,
		width,
		height
		);
	
	//
	// Create the scaler context and scale.
	//

	struct SwsContext *ctx = sws_getContext(
		info->width,
		info->height,
		info->pixfmt,
		width,
		height,
		AV_PIX_FMT_RGB24,
		SWS_BICUBIC,
		NULL, NULL, NULL
		);
		
	sws_scale(
		ctx,
		(const uint8_t * const *) info->stillframe->data,
		info->stillframe->linesize,
		0,info->height,
		stillframe->data,
		stillframe->linesize
		);
	
	sws_freeContext(ctx);
	
	//
	// Save image as JPEG.
	//
	
	kappa_fifo_save_jpeg(group,info,stillframe,jpegfile,(framecount >= 0));
	
	av_frame_free(&stillframe);
}

//
// Make all still images.
//

void kappa_fifo_make_stills(char *group,kafifo_t *info)
{
	if (! kappa_fifo_stillsizes)
	{
		info->havestills++;
		return;
	}
	
	if ((info->framecount < 5) || ! kappa_fifo_stillsizes) return;
	
	//
	// Prepare current frame buffer.
	//
	
	uint8_t *framepixels = info->buffer + info->iobytes - info->framesize;

	avpicture_fill((AVPicture *) info->stillframe,
		framepixels,
		info->pixfmt,
		info->width,
		info->height
	   );

	//
	// Loop through desired resolutions.
	//
	
	char *stillargs = kappa_fifo_stillsizes;
	
	while (*stillargs)
	{
		int width;
		int height;
		
		sscanf(stillargs,"%dx%d",&width,&height);
		
		if (width == 0)
		{
			width = (height * kappa_fifo_aspect_num) / kappa_fifo_aspect_den;
			
			if (width % 2) width++;
		}
		
		if (height == 0)
		{
			height = (width * kappa_fifo_aspect_den) / kappa_fifo_aspect_num;
			
			if (height % 2) width++;
		}
		
		kappa_fifo_make_still(group,info,width,height,-1,false);
		
		if (! index(stillargs,':')) break;
		
		stillargs = index(stillargs,':') + 1;
	}
	
	info->havestills++;
}

//
// Make scene detect.
//

int kappa_fifo_make_scene(char *group,kafifo_t *info)
{	
	//
	// Prepare current frame buffer.
	//
	
	info->lastframe = info->thisframe;
	info->thisframe = info->buffer + info->iobytes - info->framesize;
	
	int numsamples = 131;
	int stepsize   = info->framesize / numsamples;
	int	values     = 0;
	int	saminx     = 0;
	int doit = true;
	
	if (info->lastframe)
	{
		//
		// Derive sample pixel values all over the frame.
		//
		
		for (saminx = 0; saminx < info->framesize; saminx += stepsize)
		{
			if (info->thisframe[ saminx ] > info->lastframe[ saminx ])
			{
				values += info->thisframe[ saminx ] - info->lastframe[ saminx ];
			}
			else
			{
				values += info->lastframe[ saminx ] - info->thisframe[ saminx ];
			}
		}
		
		values /= numsamples;
		
		doit = ((values >= info->threshold) && ((info->framecount - info->lastscene) > info->minframes))
			|| ((info->framecount - info->lastscene) > info->maxframes);
	}
	
	if (! doit) return false;
	
	fprintf(stderr,"Header	scene   %s %d %d\n",group,info->framecount,values);

	info->lastscene = info->framecount - 1;
	
	//
	// Prepare current frame buffer.
	//
	
	avpicture_fill((AVPicture *) info->stillframe,
		info->thisframe,
		info->pixfmt,
		info->width,
		info->height
	   );

	//
	// Loop through desired resolutions.
	//
	
	char *stillargs  = info->scenesizes;
	int   onlyoneres = (index(stillargs,':') == NULL);
	
	while (*stillargs)
	{
		int width;
		int height;
		
		sscanf(stillargs,"%dx%d",&width,&height);
		
		if ((width == 0) && (height == 0))
		{
			width  = info->width;
			height = info->height;
		}
		
		if (width == 0)
		{
			width = (height * kappa_fifo_aspect_num) / kappa_fifo_aspect_den;
			
			if (width % 2) width++;
		}
		
		if (height == 0)
		{
			height = (width * kappa_fifo_aspect_den) / kappa_fifo_aspect_num;
			
			if (height % 2) width++;
		}
		
		kappa_fifo_make_still(group,info,width,height,info->lastscene,onlyoneres);
		
		if (! index(stillargs,':')) break;
		
		stillargs = index(stillargs,':') + 1;
	}
	
	return true;
}

//
// Read PNG image.
//

int read_png_file(uint8_t *data[ 4 ], int linesize[ 4 ],
                  int *width, int *height, enum AVPixelFormat *pix_fmt,
                  const char *filename, void *log_ctx)
{
	png_structp png_ptr;
	png_infop info_ptr;
	int number_of_passes;
	png_bytep *row_pointers;	
	png_bytep image_data;	
	png_byte color_type;
	png_byte bit_depth;
	char header[ 8 ];
	int rowbytes;
	int res = 0;
	
	FILE *fp = fopen(filename,"rb");
	if (! fp) return -1;
 	
	while (true)
	{
		fread(header,1,8,fp);
		
		if (png_sig_cmp(header,0,8)) 
		{ 
			res = -1; 
			break;
		}
		
		png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,NULL);

		if (! png_ptr)
		{
			res = -1; 
			break;
		}

		info_ptr = png_create_info_struct(png_ptr);
		
		if (! info_ptr)
		{
			res = -1; 
			break;
		}

		if (setjmp(png_jmpbuf(png_ptr)))
		{
			res = -1; 
			break;
		}

		png_init_io(png_ptr,fp);
		png_set_sig_bytes(png_ptr,8);

		png_read_info(png_ptr, info_ptr);

		*width   = png_get_image_width (png_ptr,info_ptr);
		*height  = png_get_image_height(png_ptr,info_ptr);
		*pix_fmt = AV_PIX_FMT_RGBA;
		
		color_type = png_get_color_type(png_ptr,info_ptr);
		bit_depth  = png_get_bit_depth (png_ptr,info_ptr);

		number_of_passes = png_set_interlace_handling(png_ptr);
		png_read_update_info(png_ptr,info_ptr);

		if (setjmp(png_jmpbuf(png_ptr)))
		{
			res = -1; 
			break;
		}

		rowbytes = png_get_rowbytes(png_ptr,info_ptr);
		image_data = (png_bytep) malloc(rowbytes * *height);
		row_pointers = (png_bytep *) malloc(sizeof(png_bytep) * *height);
		
		int y;
		
		for (y = 0; y < *height; y++)
		{
			row_pointers[ y ] = image_data + (y * rowbytes);
		}

		png_read_image(png_ptr,row_pointers);
	
		data[ 0 ] = image_data;
		data[ 1 ] = 0;
		data[ 2 ] = 0;
		data[ 3 ] = 0;
		
		linesize[ 0 ] = rowbytes;
		linesize[ 1 ] = 0;
		linesize[ 2 ] = 0;
		linesize[ 3 ] = 0;
		
		break;
	}
	
	fclose(fp);
	
	return res;
}

//
// Create logo.
//

void kappa_fifo_create_logo(char *group,kafifo_t *output,kafifo_t *input)
{
	if (! strlen(input->logo)) return;
	
	fprintf(stderr,"Header	logo   %s %s\n",group,input->logo);
	
	char fullpath[ 256 ];
	char target	 [ 256 ];
	
	strcpy(target,input->logo);
	strcat(target,".png");
	
	DIR *dir = opendir(strcpy(fullpath,"./fliegen"));
	if (! dir) dir = opendir(strcpy(fullpath,kappa_fifo_pipedir));
	
	struct dirent *entry;
	int match = false;
	int lwidth;

	while (true)
	{
		entry = readdir(dir);
		if (! entry) break;
		
		if (! strstr(entry->d_name,target)) continue;
		
		lwidth = atoi(entry->d_name);
		if (lwidth != input->final_width) continue;
		
		strcat(fullpath,"/");
		strcat(fullpath,entry->d_name);
		
		fprintf(stderr,"Header	logo   %s %s\n",group,fullpath);
		
		match = true;
	}
	
	closedir(dir);
	
	if (! match) return;
	
	//
	// int res = ff_load_image(
	// int res = read_png_file(
	//
	
	int res = read_png_file(
		input->logo_rgb_data,input->logo_rgb_size,
		&input->logo_width,&input->logo_height,
		&input->logo_pixfmt,
		fullpath, 
		NULL
		);
	
	if (res < 0)
	{
		fprintf(stderr,"Could not open logo %s, exitting now...\n",fullpath);
		exit(1);
	}
	
	fprintf(stderr,"Header	logo   %s %s %dx%d format=%s\n",
		group,fullpath,
		input->logo_width,input->logo_height,
		av_get_pix_fmt_name(input->logo_pixfmt)
		);
	
	//
	// Convert logo pixels to YUV444P video format.
	//
	
	struct SwsContext *ctx = sws_getContext(
		input->logo_width,input->logo_height,input->logo_pixfmt, 
		input->logo_width,input->logo_height,AV_PIX_FMT_YUV444P, 
		0, 
		NULL, NULL, NULL
		);
	
	const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(input->pixfmt);

	int y_shift = desc->log2_chroma_h;
	int x_shift = desc->log2_chroma_w;

	input->logo_yuv_size[ 0 ] = input->logo_width;
	input->logo_yuv_size[ 1 ] = input->logo_width;
	input->logo_yuv_size[ 2 ] = input->logo_width;

	input->logo_yuv_data[ 0 ] = (uint8_t *) malloc(input->logo_yuv_size[ 0 ] * input->logo_height);
	input->logo_yuv_data[ 1 ] = (uint8_t *) malloc(input->logo_yuv_size[ 1 ] * input->logo_height);
	input->logo_yuv_data[ 2 ] = (uint8_t *) malloc(input->logo_yuv_size[ 2 ] * input->logo_height);
		
	sws_scale(
		ctx,
		(const uint8_t * const *) input->logo_rgb_data,
		input->logo_rgb_size,
		0,input->logo_height,
		input->logo_yuv_data,
		input->logo_yuv_size
		);
		
	sws_freeContext(ctx);
	
	//
	// Build alpha lookup table.
	//
	
	input->alphatable = (uint8_t *) malloc(256 * 256);
	
	int alpha;
	int pixel;
	
	for (alpha = 0; alpha < 256; alpha++)
	{
		for (pixel = 0; pixel < 256; pixel++)
		{
			input->alphatable[ (alpha << 8) + pixel ] = (uint8_t) ((pixel * alpha) / 255);
		}
	}
	
	//
	// Optimize logo rectangle.
	//

	int logowid = input->logo_width;
	int logohei = input->logo_height;
	int logosiz = input->logo_rgb_size[ 0 ];
	
	int 	 wid;
	int 	 hei;
	int		 zero;
	uint8_t *rgba;
	
	//
	// Optimize top.
	//
	
	rgba = input->logo_rgb_data[ 0 ] + 3;
	
	for (hei = 0; (hei < logohei) && (input->logo_height > 0); hei++)
	{	
		for (zero = true, wid = 0; zero && (wid < logowid); wid++)
		{
			if (*rgba > 0) zero = false;
			
			rgba += 4;
		}
		
		if (! zero) break;
		
		input->logo_top++;
		input->logo_height--;
	}
	
	//
	// Optimize height.
	//
	
	rgba = input->logo_rgb_data[ 0 ] - 1 + (logohei * logosiz);
	
	for (hei = 0; (hei < logohei) && (input->logo_height > 0); hei++)
	{	
		for (zero = true, wid = 0; zero && (wid < logowid); wid++)
		{
			if (*rgba > 0) zero = false;
			
			rgba -= 4;
		}
		
		if (! zero) break;
		
		input->logo_height--;
	}
	
	//
	// Optimize left.
	//
	
	for (wid = 0; (wid < logowid) && (input->logo_width > 0); wid++)
	{	
		rgba = input->logo_rgb_data[ 0 ] + 3 + (wid * 4);
	
		for (zero = true, hei = 0; zero && (hei < logohei); hei++)
		{
			if (*rgba > 0) zero = false;
			
			rgba += logosiz;
		}
		
		if (! zero) break;
		
		input->logo_left++;
		input->logo_width--;
	}
	
	//
	// Optimize width.
	//
	
	for (wid = 0; (wid < logowid) && (input->logo_width > 1); wid++)
	{	
		rgba = input->logo_rgb_data[ 0 ] - 1 + (logohei * logosiz) - (wid * 4);
		
		for (zero = true, hei = 0; zero && (hei < logohei); hei++)
		{
			if (*rgba > 0) zero = false;
			
			rgba -= logosiz;
		}
		
		if (! zero) break;
		
		input->logo_width--;
	}
	
	fprintf(stderr,"Header	logo   %s %s %d:%d => %dx%d\n",
		group,fullpath,
		input->logo_left,input->logo_top,
		input->logo_width,input->logo_height
		);
	
	input->wantlogo = true;
}

//
// Create scale and crop.
//

void kappa_fifo_create_scale_crop(char *group,kafifo_t *output,kafifo_t *input)
{
	//
	// Create the scaler context.
	//

	input->sws = sws_getContext(
		output->width,
		output->height,
		output->pixfmt,
		input->width,
		input->height,
		input->pixfmt,
		SWS_BICUBIC,
		NULL, NULL, NULL
		);

	//
	// Decide what to do.
	//

	input->wantscale = (input->width  != output->width)	 ||
					   (input->height != output->height) ||
					   (input->pixfmt != output->pixfmt);

	input->wantcrop	 = (input->final_width	!= input->width) ||
					   (input->final_height != input->height);

	//
	// Lay out allocated frame buffer on frame struct.
	//

	if (input->wantscale && input->wantcrop)
	{
		input->scalepixels = malloc(input->scalesize);

		input->scaleframe = av_frame_alloc();

		input->scaleframe->format = input->pixfmt;
		input->scaleframe->width  = input->width;
		input->scaleframe->height = input->height;

		avpicture_fill((AVPicture *) input->scaleframe,
			input->scalepixels,
			input->pixfmt,
			input->width,
			input->height
		   );
	}

	if (input->wantscale || input->wantcrop || input->wantlogo)
	{
		input->finalpixels = malloc(input->finalsize);

		input->finalframe = av_frame_alloc();

		input->finalframe->format = input->pixfmt;
		input->finalframe->width  = input->final_width;
		input->finalframe->height = input->final_height;

		avpicture_fill((AVPicture *) input->finalframe,
			input->finalpixels,
			input->pixfmt,
			input->final_width,
			input->final_height
		   );

		input->outputframe = av_frame_alloc();
	}
}

//
// Create input YUV4MPEG header and write.
//

void kappa_fifo_write_yuv4mpeg(char *group,kafifo_t *output,kafifo_t *input)
{
	if (! input->width)			input->width		= output->width;
	if (! input->height)		input->height		= output->height;
	if (! input->format)		input->format		= output->format;
	if (! input->pixfmt)		input->pixfmt		= output->pixfmt;
	if (! input->aspect_num)	input->aspect_num	= output->aspect_num;
	if (! input->aspect_den)	input->aspect_den	= output->aspect_den;
	if (! input->fps_num)		input->fps_num		= output->fps_num;
	if (! input->fps_den)		input->fps_den		= output->fps_den;
	if (! input->mode)			input->mode			= output->mode;

	input->final_width	= input->crop_width	 ? input->crop_width  : input->width;
	input->final_height = input->crop_height ? input->crop_height : input->height;

	if (input->crop_center)
	{
		input->crop_left = (input->width  - input->crop_width ) >> 1;
		input->crop_top  = (input->height - input->crop_height) >> 1;
	}
	
	input->scalesize = avpicture_get_size(input->pixfmt,input->width,input->height);
	input->finalsize = avpicture_get_size(input->pixfmt,input->final_width,input->final_height);
	input->chunksize = input->finalsize + strlen("FRAME\n");

	char yheader[ 256 ];

	snprintf(yheader,sizeof(yheader),
			 "YUV4MPEG2 W%d H%d F%d:%d I%c A%d:%d C%03dmpeg2 XYSCSS=%03dMPEG2\n",
			 input->final_width,input->final_height,
			 input->fps_num,input->fps_den,
			 input->mode,
			 input->aspect_num,input->aspect_den,
			 input->format,input->format
			);

	int xfer = write(input->fd,yheader,strlen(yheader));

	fprintf(stderr,"Header	input  %s %d %s",group,xfer,yheader);
	
	kappa_fifo_create_logo(group,output,input);
	
	kappa_fifo_create_scale_crop(group,output,input);

	input->haveheader = true;
}

//
// Compute current logo visibility. 
// 0 is transparent, 255 is visible.
//

int kappa_fifo_logovisible(kafifo_t *info)
{
	if (! kappa_fifo_logooocnt)
	{
		//
		// No options specified, 
		// logo is always visible.
		//
		
		return 255;
	}

	double etime = (kappa_fifo_maxframe <= 0) ? -1 : (kappa_fifo_maxframe * info->fps_den / (double) info->fps_num);
	double ctime = info->framecount * info->fps_den / (double) info->fps_num;

	int visibility = 255;
	int inx;

	for (inx = kappa_fifo_logooocnt - 1; inx >= 0; inx--)
	{
		double dtime = kappa_fifo_logotime[ inx ];
		
		if (dtime < 0.0)
		{
			if (etime < 0.0) 
			{	
				//
				// Time is negative and no number of frames
				// specified, so ignore this entry.
				//
				
				continue;
			}
			
			dtime = etime + dtime;
		}
		
		if (ctime < (dtime - kappa_fifo_logofade)) continue;
		
		//
		// Compute time distance until logo either fully visible
		// or fully invisible.
		//
		
		double dist = (dtime - ctime);
		
		if (dist < 0.0) dist = 0.0;

		//
		// Distance is 0.0 just before event and 
		// 1.0 when fading is about to start.
		//
		
		if (kappa_fifo_logofade > 0.0) dist = (dist / kappa_fifo_logofade);
		
		double fade = (kappa_fifo_logomode[ inx ] == 0) ? 0.0 + dist : 1.0 - dist;
		
		visibility = (int) (255 * fade);
		
		break;
	}
	
	return visibility;
}

//
// Add logo to frame.
//

void kappa_fifo_logo(AVFrame *dst,kafifo_t *info)
{
	const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(dst->format);

	int y_shift = desc->log2_chroma_h;
	int x_shift = desc->log2_chroma_w;

	int top    = info->logo_top;
	int left   = info->logo_left;
	int width  = info->logo_width;
	int height = info->logo_height;
	
	byte *dptr[ 3 ];

	dptr[ 0 ] = dst->data[ 0 ] + ( top			   * dst->linesize[ 0 ]) +	left;
	dptr[ 1 ] = dst->data[ 1 ] + ((top >> y_shift) * dst->linesize[ 1 ]) + (left >> x_shift);
	dptr[ 2 ] = dst->data[ 2 ] + ((top >> y_shift) * dst->linesize[ 2 ]) + (left >> x_shift);
	
	byte *sptr[ 4 ];

	sptr[ 0 ] = info->logo_yuv_data[ 0 ] + (top * info->logo_yuv_size[ 0 ]) + left;
	sptr[ 1 ] = info->logo_yuv_data[ 1 ] + (top * info->logo_yuv_size[ 1 ]) + left;
	sptr[ 2 ] = info->logo_yuv_data[ 2 ] + (top * info->logo_yuv_size[ 2 ]) + left;
	sptr[ 3 ] = info->logo_rgb_data[ 0 ] + (top * info->logo_rgb_size[ 0 ]) + (left * 4) + 3;
	
	int line;
	int pixi;
	int maxh;
	int maxw;
	
	uint8_t *dptr0;
	uint8_t *dptr1;
	uint8_t *dptr2;
	
	uint8_t *sptr0;
	uint8_t *sptr1;
	uint8_t *sptr2;
	uint8_t *sptr3;
	uint8_t  sval3;
	
	uint8_t *alphatable = info->alphatable;
	
	int logovis = kappa_fifo_logovisible(info) << 8;
	
	//
	// Grey plane not subsampled.
	//
	
	maxh = height;
	maxw = width;
	
	sptr[ 3 ] = info->logo_rgb_data[ 0 ] + (top * info->logo_rgb_size[ 0 ]) + (left * 4) + 3;

	for (line = 0; line < maxh; line++)
	{
		dptr0 = dptr[ 0 ];
		
		sptr0 = sptr[ 0 ];
		sptr3 = sptr[ 3 ];
		
		for (pixi = 0; pixi < maxw; pixi++)
		{
			sval3 = alphatable[ logovis + *sptr3 ];
			
			*dptr0 = alphatable[ ((255 - sval3) << 8) + *dptr0 ] + alphatable[ (sval3 << 8) + *sptr0 ];
			
			dptr0++;
			
			sptr0 += 1;
			sptr3 += 4;
		}
		
		dptr[ 0 ] += dst->linesize[ 0 ];
		
		sptr[ 0 ] += info->logo_yuv_size[ 0 ];
		sptr[ 3 ] += info->logo_rgb_size[ 0 ];
	}
	
	//
	// Color planes possibly subsampled.
	//

	maxh = height >> y_shift;
	maxw = width  >> x_shift;
	
	sptr[ 3 ] = info->logo_rgb_data[ 0 ] + (top * info->logo_rgb_size[ 0 ]) + (left * 4) + 3;

	for (line = 0; line < maxh; line++)
	{
		dptr1 = dptr[ 1 ];
		dptr2 = dptr[ 2 ];
		
		sptr1 = sptr[ 1 ];
		sptr2 = sptr[ 2 ];
		sptr3 = sptr[ 3 ];
		
		for (pixi = 0; pixi < maxw; pixi++)
		{
			sval3 = alphatable[ logovis + *sptr3 ];
			
			*dptr1 = alphatable[ ((255 - sval3) << 8) + *dptr1 ] + alphatable[ (sval3 << 8) + *sptr1 ];
			*dptr2 = alphatable[ ((255 - sval3) << 8) + *dptr2 ] + alphatable[ (sval3 << 8) + *sptr2 ];
			
			dptr1++;
			dptr2++;
			
			sptr1 += 1 << x_shift;
			sptr2 += 1 << x_shift;
			sptr3 += 4 << x_shift;
		}
				
		dptr[ 1 ] += dst->linesize[ 1 ];
		dptr[ 2 ] += dst->linesize[ 2 ];
		
		sptr[ 1 ] += info->logo_yuv_size[ 1 ] << y_shift;
		sptr[ 2 ] += info->logo_yuv_size[ 2 ] << y_shift;
		sptr[ 3 ] += info->logo_rgb_size[ 0 ] << y_shift;
	}
}

//
// Crop frame.
//

void kappa_fifo_crop(AVFrame *dst,AVFrame *src,int top,int left)
{
	const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(dst->format);

	int y_shift = desc->log2_chroma_h;
	int x_shift = desc->log2_chroma_w;

	byte *sptr[ 3 ];

	sptr[ 0 ] = src->data[ 0 ] + ( top			   * src->linesize[ 0 ]) +	left;
	sptr[ 1 ] = src->data[ 1 ] + ((top >> y_shift) * src->linesize[ 1 ]) + (left >> x_shift);
	sptr[ 2 ] = src->data[ 2 ] + ((top >> y_shift) * src->linesize[ 2 ]) + (left >> x_shift);

	byte *dptr[ 3 ];

	dptr[ 0 ] = dst->data[ 0 ];
	dptr[ 1 ] = dst->data[ 1 ];
	dptr[ 2 ] = dst->data[ 2 ];

	int line;
	int maxh;

	maxh = dst->height;

	for (line = 0; line < maxh; line++)
	{
		memcpy(dptr[ 0 ],sptr[ 0 ],dst->linesize[ 0 ]);

		dptr[ 0 ] += dst->linesize[ 0 ];
		sptr[ 0 ] += src->linesize[ 0 ];
	}

	maxh = dst->height >> y_shift;

	for (line = 0; line < maxh; line++)
	{
		memcpy(dptr[ 1 ],sptr[ 1 ],dst->linesize[ 1 ]);
		memcpy(dptr[ 2 ],sptr[ 2 ],dst->linesize[ 2 ]);

		dptr[ 1 ] += dst->linesize[ 1 ];
		dptr[ 2 ] += dst->linesize[ 2 ];

		sptr[ 1 ] += src->linesize[ 1 ];
		sptr[ 2 ] += src->linesize[ 2 ];
	}
}

//
// Thread writer main loop.
//

void *kappa_fifo_thread_writer(void *kafifoptr)
{
	kafifo_t *input = (kafifo_t *) kafifoptr;
	char	 *name	= kappa_fifo_groupname[ input->group ];

	fprintf(stderr,"Started thread %s writer\n",name);

	//
	// Identify output pipe.
	//

	kafifo_t *output = NULL;

	int cnt;

	while (true)
	{
		for (cnt = 0; cnt < kappa_fifo_outputscnt; cnt++)
		{
			if (input->group == kappa_fifo_outputinfo[ cnt ].group)
			{
				output = &kappa_fifo_outputinfo[ cnt ];

				break;
			}
		}

		if (output) break;

		usleep(10000);
	}

	//
	// Derive basic info from pipe name.
	//

	kappa_fifo_parse_fifoname(name,input);

	//
	// We switch to blocking mode.
	//

	fcntl(input->fd,F_SETFL,fcntl(input->fd,F_GETFL,0) & ~O_NONBLOCK);

	fprintf(stderr,"Running thread %s writer\n",name);

	int	  offs;
	int	  xfer;
	int	  yfer;
	byte *bufp;

	while (true)
	{
		if (output->isyuv4mpeg && ! input->haveheader)
		{
			//
			// We did not yet prepare the header.
			//

			if (! output->haveheader)
			{
				//
				// Nor did the output reader.
				//

				usleep(10000);

				continue;
			}

			kappa_fifo_write_yuv4mpeg(name,output,input);
		}

		if (input->iobytes == output->iobytes)
		{
			//
			// Nothing to do.
			//

			if (output->fd < 0)
			{
				//
				// Output pipe is closed, input pipe has all written,
				// no more work to do, close this now.
				//

				fprintf(stderr,"Closing write pipe %s\n",name);

				close(input->fd);
				input->fd = -1;

				break;
			}

			usleep(10000);
			continue;
		}

		bufp = output->buffer;
		offs = input->iobytes;

		if (offs <= output->iobytes)
		{
			xfer = output->iobytes - offs;
		}
		else
		{
			xfer = output->bufsiz - offs;
		}

		if (xfer == 0)
		{
			//
			// Nothing todo.
			//
			
			usleep(10000);
			continue;
		}

		if (input->isyuv4mpeg)
		{
			//
			// We want full frames in each write.
			//

			if (xfer < output->chunksize)
			{
				usleep(10000);
				continue;
			}

			if (xfer > output->chunksize) xfer = output->chunksize;

			if (strncmp(output->buffer + offs,"FRAME\n",6))
			{
				fprintf(stderr,"Frame magic wrong %s, exitting now...\n",name);
				exit(1);
			}

			input->framecount++;

			//fprintf(stderr,"Frame writer %s %7d\n",name,input->framecount);

			//
			// Do frame processing now.
			//

			if (! (input->wantscale || input->wantcrop || input->wantlogo))
			{
				//
				// No scale, no crop, we just copy the pointer.
				//

				bufp = output->buffer + offs + 6;
			}
			else
			{
				//
				// We want to scale or crop or both.
				//
				
				avpicture_fill((AVPicture *) input->outputframe,
					output->buffer + offs + 6,
					output->pixfmt,
					output->width,
					output->height
				   );

				if (input->wantscale)
				{
					//
					// We want to scale.
					//

					if (input->wantcrop)
					{
						sws_scale(
							input->sws,
							(const uint8_t * const *) input->outputframe->data,
							input->outputframe->linesize,
							0,output->height,
							input->scaleframe->data,
							input->scaleframe->linesize
							);

						kappa_fifo_crop(
							input->finalframe,
							input->scaleframe,
							input->crop_top,
							input->crop_left
							);
					}
					else
					{
						sws_scale(
							input->sws,
							(const uint8_t * const *) input->outputframe->data,
							input->outputframe->linesize,
							0,output->height,
							input->finalframe->data,
							input->finalframe->linesize
							);
					}
				}
				else
				{
					if (input->wantcrop)
					{
						kappa_fifo_crop(
							input->finalframe,
							input->outputframe,
							input->crop_top,
							input->crop_left
							);
					}
					else
					{
						//
						// Only logo desired, must copy pixels.
						//
						
						memcpy(input->finalpixels,output->buffer + offs + 6,input->finalsize);
					}
				}

				if (input->wantlogo)
				{
					kappa_fifo_logo(input->finalframe,input);
				}
				
				bufp = input->finalpixels;
			}

			write(input->fd,"FRAME\n",6);
			write(input->fd,bufp,input->finalsize);

			yfer = xfer;
		}
		else
		{
			if (xfer > (128 * 1024)) xfer = 128 * 1024;

			//if (input->isaudio) fprintf(stderr,"Want write audio pipe %s %d\n",name,xfer);

			yfer = write(input->fd,bufp + offs,xfer);

			//if (input->isaudio) fprintf(stderr,"Done write audio pipe %s %d\n",name,yfer);
			
			//fprintf(stderr,"Thread %s writer wrote (%d/%d) %s\n",name,xfer,yfer,input->name);
		}

		if (yfer <= 0)
		{
			usleep(10000);
			continue;
		}

		if (yfer > 0)
		{
			input->iobytes += yfer;
			input->total   += yfer;
		}
	}

	fprintf(stderr,"Closing thread %s writer with %d frames %s\n",name,input->framecount,input->name);

	kappa_fifo_close_info(name,input);
	
	kappa_fifo_inputdone++;

	return NULL;
}

//
// Thread reader main loop.
//

void *kappa_fifo_thread_reader(void *kafifoptr)
{
	kafifo_t *output = (kafifo_t *) kafifoptr;
	char	 *name	 = kappa_fifo_groupname[ output->group ];

	fprintf(stderr,"Started thread %s reader\n",name);

	//
	// Derive basic info from pipe names.
	//

	kappa_fifo_parse_fifoname(name,output);

	//
	// We switch to blocking mode.
	//

	fcntl(output->fd,F_SETFL,fcntl(output->fd,F_GETFL,0) & ~O_NONBLOCK);

	fprintf(stderr,"Running thread %s reader\n",name);

	kafifo_t *input;

	int ends;
	int offs;
	int xfer;
	int yfer;
	int wrap;
	int done;
	int rest;
	int cnt;

	while (true)
	{
		//
		// Read as much as possible.
		//

		ends = output->bufsiz;

		if (kappa_fifo_grouptodo[ output->group ] != 0)
		{
			//
			// Some inputs are not yet ready. Read
			// until buffer filled completely and wait
			// for inputs to come alive.
			//
		}
		else
		{
			if (output->iobytes == output->bufsiz)
			{
				//
				// We like to wrap. Only possible if
				// all input pointers are not at start
				// of buffer.
				//

				wrap = true;

				for (cnt = 0; cnt < kappa_fifo_inputscnt; cnt++)
				{
					input = &kappa_fifo_inputinfo[ cnt ];

					if (input->group != output->group) continue;

					if (input->iobytes == 0) wrap = false;
				}

				//
				// Wrap buffer now.
				//

				if (wrap) output->iobytes = 0;
			}

			done = true;

			for (cnt = 0; cnt < kappa_fifo_inputscnt; cnt++)
			{
				input = &kappa_fifo_inputinfo[ cnt ];

				if (input->group != output->group) continue;

				if (input->fd < 0) continue;

				if (output->iobytes < input->iobytes)
				{
					//
					// Input pointer is behind us in buffer,
					// means, we have wrapped and input not yet.
					// Stay before as close as we can.
					//

					if (ends > (input->iobytes - 1))
					{
						//
						// Limit our buffer end to this point.
						//

						ends = input->iobytes - 1;
					}
				}

				if ((input->iobytes  == output->bufsiz) &&
					(output->iobytes != output->bufsiz))
				{
					//
					// We can now safely wrap this input.
					//

					input->iobytes = 0;
				}

				done = false;
			}

			if (done && (output->fd < 0))
			{
				//
				// Leave thread loop.
				//

				break;
			}
		}

		if ((output->iobytes == ends) || (output->fd < 0))
		{
			//
			// Nothing todo.
			//

			usleep(10000);
			continue;
		}

		offs = output->iobytes;
		xfer = ends - offs;

		if (output->isyuv4mpeg && output->haveheader)
		{
			//
			// We prefer to read full frames for processing.
			//

			rest = output->chunksize - (output->iobytes % output->chunksize);

			//
			// Try to read the rest of frame or another.
			//

			if (xfer > rest) xfer = rest;
		}
		else
		{
			if (xfer > RDWRSIZE) xfer = RDWRSIZE;
		}

		//if (output->isaudio) fprintf(stderr,"Want read  audio pipe %s %d\n",name,xfer);

		yfer = read(output->fd,output->buffer + offs,xfer);

		//if (output->isaudio) fprintf(stderr,"Done read  audio pipe %s %d\n",name,yfer);

		if (yfer == 0)
		{
			if (output->total > 0)
			{
				//
				// We had already input, means pipe
				// is at end, so close it and leave loop.
				//

				fprintf(stderr,"Closing reader pipe %s\n",name);

				close(output->fd);
				output->fd = -1;
			}
		}

		if (yfer > 0)
		{
			output->iobytes += yfer;
			output->total	+= yfer;

			if (output->isyuv4mpeg)
			{
				if (! output->haveheader)
				{
					kappa_fifo_parse_yuv4mpeg(name,output);

					continue;
				}

				if ((output->iobytes > 0) && ((output->iobytes % output->chunksize) == 0))
				{
					if (strncmp(output->buffer + output->iobytes - output->chunksize,"FRAME\n",6))
					{
						fprintf(stderr,"Frame magic wrong %s, exitting now...\n",name);
						exit(1);
					}

					output->framecount++;
						
					if (kappa_fifo_maxframe > 0)
					{
						int percent = 100 * output->framecount / kappa_fifo_maxframe;
						
						if (percent != kappa_fifo_percentdone)
						{
							fprintf(stderr,"Progress:%d\n",percent);
							fflush(stderr);
							
							kappa_fifo_percentdone = percent;
						}
					}
					else
					{
						if ((output->framecount % 10) == 0)
						{
							fprintf(stderr,"Actframe:%d\n",output->framecount);
						}
					}
					
					if (output->wantscene) 
					{
						int wasscene = kappa_fifo_make_scene(name,output);
						
						if (output->wantimage && wasscene && (output->havestills  == 1))
						{
							kappa_fifo_make_stills(name,output);
						}		
					}

					if (output->havestills == 0)
					{
						kappa_fifo_make_stills(name,output);
					}
				}
			}
		}
	}

	fprintf(stderr,"Closing thread %s reader with %d frames\n",name,output->framecount);
	
	kappa_fifo_close_info(name,output);
	
	kappa_fifo_outputdone++;

	return NULL;
}

//
// Get group index.
//

int kappa_fifo_groupindex(char *pipe)
{
	int inx;
	int dot = 0;

	for (inx = 0; inx < strlen(pipe); inx++)
	{
		if (pipe[ inx ] == '.') dot++;
	}

	if (dot < 5)
	{
		fprintf(stderr,"Pipe name %s malformatted, exitting now...\n",pipe);
		exit(1);
	}

	char  work[ MAXPATHLEN ];
	char *indx = work;

	strcpy(work,pipe);

	indx = index(indx,'.') + 1;
	indx = index(indx,'.') + 1;
	indx = index(indx,'.') + 1;
	indx = index(indx,'.') + 1;

	*index(indx,'.') = '\0';

	for (inx = 0; inx < kappa_fifo_groupscnt; inx++)
	{
		if (! strcmp(indx,kappa_fifo_groupname[ inx ])) return inx;
	}

	strcpy(kappa_fifo_groupname[ inx ],indx);

	kappa_fifo_grouptodo[ inx ] = -1;

	fprintf(stderr,"Created group  %s\n",indx);

	kappa_fifo_groupscnt++;

	return inx;
}

//
// Look for input pipes and open them.
//

void kappa_fifo_open_all(int pass)
{
	int inx;
	int cnt;
	int dup;
	int tfd;
	int grp;

	char pattern_inp[ 64 ];
	char pattern_out[ 64 ];

	snprintf(pattern_inp,sizeof(pattern_inp),"Kappa.inp.%d",pass);
	snprintf(pattern_out,sizeof(pattern_out),"Kappa.out.%d",pass);

	//
	// Setup private todo counters.
	//

	int temptodo[ MAXSLOTS ];
	int tempmore = 0;

	for (grp = 0; grp < MAXSLOTS; grp++)
	{
		temptodo[ grp ] = 0;
	}

	//
	// Scan working directory for pipes.
	//

	char pipepath[ MAXPATHLEN ];
	DIR *dir = opendir(kappa_fifo_pipedir);
	struct dirent *entry;
	 
	while (true)
	{
		entry = readdir(dir);
		if (! entry) break;
		
		snprintf(pipepath,sizeof(pipepath),"%s/%s",kappa_fifo_pipedir,entry->d_name);

		if (strncmp(entry->d_name,pattern_out,strlen(pattern_out)) == 0)
		{
			for (dup = false, cnt = 0; cnt < kappa_fifo_outputscnt; cnt++)
			{
				if (! strcmp(kappa_fifo_outputinfo[ cnt ].name,pipepath))
				{
					dup = true;
					break;
				}
			}

			if (dup) continue;
			
			grp = kappa_fifo_groupindex(entry->d_name);

			tfd = open(pipepath,O_RDONLY | O_NONBLOCK | O_SYNC);

			int bufsize = RBUFSIZE;
				
			if (strcmp(pipepath + strlen(pipepath) - 4,".y4m"  ) == 0) bufsize = VBUFSIZE;
			if (strcmp(pipepath + strlen(pipepath) - 6,".s16le") == 0) bufsize = ABUFSIZE;

			fprintf(stderr,"Opening output %s = %2d => %s (%d)\n",
					kappa_fifo_groupname[ grp ],tfd,pipepath,bufsize);

			if (tfd < 0)
			{
				temptodo[ grp ]++;
				tempmore++;
			}
			else
			{
				memset(&kappa_fifo_outputinfo[ kappa_fifo_outputscnt ],0,sizeof(kafifo_t));
				strcpy( kappa_fifo_outputinfo[ kappa_fifo_outputscnt ].name,pipepath);
				
				kappa_fifo_outputinfo[ kappa_fifo_outputscnt ].fd	  = tfd;
				kappa_fifo_outputinfo[ kappa_fifo_outputscnt ].group  = grp;
				kappa_fifo_outputinfo[ kappa_fifo_outputscnt ].bufsiz = bufsize;
				kappa_fifo_outputinfo[ kappa_fifo_outputscnt ].buffer = (byte *) malloc(bufsize);

				pthread_create(
					&kappa_fifo_outputinfo[ kappa_fifo_outputscnt ].thread,
					NULL,
					kappa_fifo_thread_reader,
					(void *) &kappa_fifo_outputinfo[ kappa_fifo_outputscnt ]
					);

				kappa_fifo_outputscnt++;

				continue;
			}
		}

		if (strncmp(entry->d_name,pattern_inp,strlen(pattern_inp)) == 0)
		{
			for (dup = false, cnt = 0; cnt < kappa_fifo_inputscnt; cnt++)
			{
				if (! strcmp(kappa_fifo_inputinfo[ cnt ].name,pipepath))
				{
					dup = true;
					break;
				}
			}

			if (dup) continue;

			grp = kappa_fifo_groupindex(entry->d_name);

			tfd = open(pipepath,O_WRONLY | O_NONBLOCK | O_SYNC);

			fprintf(stderr,"Opening input  %s = %2d => %s\n",
					kappa_fifo_groupname[ grp ],tfd,pipepath);

			if (tfd < 0)
			{
				temptodo[ grp ]++;
				tempmore++;
			}
			else
			{
				memset(&kappa_fifo_inputinfo[ kappa_fifo_inputscnt ],0,sizeof(kafifo_t));
				strcpy( kappa_fifo_inputinfo[ kappa_fifo_inputscnt ].name,pipepath);

				kappa_fifo_inputinfo[ kappa_fifo_inputscnt ].fd	   = tfd;
				kappa_fifo_inputinfo[ kappa_fifo_inputscnt ].group = grp;

				pthread_create(
					&kappa_fifo_inputinfo[ kappa_fifo_inputscnt ].thread,
					NULL,
					kappa_fifo_thread_writer,
					(void *) &kappa_fifo_inputinfo[ kappa_fifo_inputscnt ]
					);

				kappa_fifo_inputscnt++;

				continue;
			}
		}
	}

	closedir(dir);

	//
	// Copy results for waiting threads.
	//

	for (grp = 0; grp < kappa_fifo_groupscnt; grp++)
	{
		kappa_fifo_grouptodo[ grp ] = temptodo[ grp ];
	}

	kappa_fifo_groupmore = tempmore;
}

//
// Close all pipes.
//

void kappa_fifo_close_all()
{
	int inx;

	for (inx = 0; inx < kappa_fifo_inputscnt; inx++)
	{
		kafifo_t *input = &kappa_fifo_inputinfo[ inx ];

		if (input->fd >= 0)
		{
			close(input->fd);
			input->fd = -1;
		}
	}

	for (inx = 0; inx < kappa_fifo_outputscnt; inx++)
	{
		kafifo_t *output = &kappa_fifo_outputinfo[ inx ];

		if (output->fd >= 0)
		{
			close(output->fd);
			output->fd = -1;
		}
	}
}

//
// Execute a single pass until input closed.
//

int kappa_fifo_execute_pass(int pass)
{
	//
	// Initial open loop.
	//

	fprintf(stderr,"Looking for Kappa pipes on pass %d.\n",pass);

	while (! kappa_fifo_outputscnt)
	{
		kappa_fifo_open_all(pass);
	}

	//
	// Working and open loop.
	//

	while ((kappa_fifo_outputdone < kappa_fifo_outputscnt) ||
		   (kappa_fifo_inputdone  < kappa_fifo_inputscnt ))
	{
		if (kappa_fifo_groupmore)
		{
			kappa_fifo_open_all(pass);
		}

		fflush(stderr);

		sleep(1);
	}

	//
	// Close remaining pipes.
	//

	kappa_fifo_close_all();

	return 0;
}

//
// Main entry.
//

int main(int argc, char **argv)
{
	fprintf(stderr,"Kappa Fifo Manager %s.\n",kappa_fifo_version);
	fflush(stderr);

	if (argc == 0) kappa_fifo_usage();

	int inx;

	for (inx = 1; inx + 1 < argc; inx++)
	{
		if (! strcmp(argv[ inx ],"--pass"))
		{
			kappa_fifo_pass = atoi(argv[ inx + 1 ]);
		}
		
		if (! strcmp(argv[ inx ],"--frames"))
		{
			kappa_fifo_maxframe = atoi(argv[ inx + 1 ]);
		}
		
		if (! strcmp(argv[ inx ],"--logofade"))
		{
			kappa_fifo_logofade = strtod(argv[ inx + 1 ],NULL);
		}
		
		if ((! strcmp(argv[ inx ],"--logoon")) && (kappa_fifo_logooocnt < MAXONOFF))
		{
			kappa_fifo_logotime[ kappa_fifo_logooocnt ] = strtod(argv[ inx + 1 ],NULL);
			kappa_fifo_logomode[ kappa_fifo_logooocnt ] = 1;
			
			if ((kappa_fifo_logooocnt == 0) ||
				(kappa_fifo_logomode[ kappa_fifo_logooocnt - 1 ] !=
				 kappa_fifo_logomode[ kappa_fifo_logooocnt - 0 ]))
			{
				//
				// Avoid duplicate instructions.
				//
				
				kappa_fifo_logooocnt++;
			}
		}
		
		if ((! strcmp(argv[ inx ],"--logooff")) && (kappa_fifo_logooocnt < MAXONOFF))
		{
			kappa_fifo_logotime[ kappa_fifo_logooocnt ] = strtod(argv[ inx + 1 ],NULL);
			kappa_fifo_logomode[ kappa_fifo_logooocnt ] = 0;
			
			if ((kappa_fifo_logooocnt == 0) ||
				(kappa_fifo_logomode[ kappa_fifo_logooocnt - 1 ] !=
				 kappa_fifo_logomode[ kappa_fifo_logooocnt - 0 ]))
			{
				//
				// Avoid duplicate instructions.
				//
				
				kappa_fifo_logooocnt++;
			}
		}
		
		if (! strcmp(argv[ inx ],"--aspect"))
		{
			if (! strstr(argv[ inx + 1 ],":"))
			{
				fprintf(stderr,"Aspect parameter malformatted %s, exitting now...\n",argv[ inx + 1 ]);
				exit(1);
			}
			
			kappa_fifo_aspect_num = atoi(argv[ inx + 1 ]);
			kappa_fifo_aspect_den = atoi(argv[ inx + 1 ]);
		}
				
		if (! strcmp(argv[ inx ],"--pipedir"))
		{
			kappa_fifo_pipedir = argv[ inx + 1 ];
		}

		if (! strcmp(argv[ inx ],"--prefix"))
		{
			kappa_fifo_fileprefix = argv[ inx + 1 ];
		}
		
		if (! strcmp(argv[ inx ],"--images"))
		{
			kappa_fifo_stillsizes = argv[ inx + 1 ];
		}
		
		if (! strcmp(argv[ inx ],"--scene"))
		{
			kappa_fifo_sceneparam = argv[ inx + 1 ];
		}
		
		if (! strcmp(argv[ inx ],"--stills"))
		{
			kappa_fifo_stillparam = argv[ inx + 1 ];
		}
	}

	//
	// Execute desired pass.
	//
	
	int res = kappa_fifo_execute_pass(kappa_fifo_pass);

	return res;
}
