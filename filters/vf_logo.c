/*
 * libavfilter/vf_logo.c
 *
 * Filter to overlay logo on top of video
 *
 * Copyright (c) 2013 Dennis Zierahn   (update to current ffmpeg)
 * Copyright (c) 2012 Dennis Zierahn   (update to current ffmpeg)
 * Copyright (c) 2010 Dennis Zierahn   (major revision)
 * Copyright (c) 2010 Stephan Heffner  (minor changes make it compile, cross-compile environment)
 * Copyright (c) 2009 Juergen Meissner (using parts of previous code)
 * Copyright (c) 2008 Victor Paesa     (libavfilter/vsrc_movie.c)
 * Copyright (c) 2007 Bobby Bingham    (libavfilter/vf_overlay.c)
 * Copyright (c) 2007 Juergen Meissner (vhook/overlay.c)
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 *
 * example of using libavfilter/vf_logo:
 *
 * ffmpeg -i inputvideofile -vf logo=10:20:logofile.png -y outputvideofile
 *
 * image of logofile.png is overlayed onto every frame of inputvideofile 
 * at offset x=10 y=20 giving outputvideofile
 *
 * x <INT>
 *
 *   Defines a logo (left border) offset from the left side of the video.
 *   A negative value offsets (logo right border) from
 *   the right side of the video.
 *
 * y <INT>
 *
 *   Defines a logo (top border) offset from the top of the video.
 *   A negative value offsets (logo bottom border) from
 *   the bottom of the video.
 *
 * if logofile has no alpha-path You can prefix another 3 fields R,G,B
 * to select a RGB-color to be the transparent one, or You can code
 * 999:999:999 to force overlaying all pixels (even if no alpha-path)
 *
 * ffmpeg -i inputvideofile -vf logo=0:0:0:10:20:logofile.png -y outputvideofile
 *   black is the color to be understood as transparent
 *
 * ffmpeg -i inputvideofile -vf logo=999:999:999:10:20:logofile.png -y outputvideofile
 *   overlay with all the pixels, alpha will be adjusted to 255
 *
 */
 
#define VF_LOGO_VERSION "1.0.0.2013.01.22"

#include "libavutil/colorspace.h"
#include "libavutil/common.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "libavutil/parseutils.h"
#include "libswscale/swscale.h"
#include "libavformat/avformat.h"
#include "avfilter.h"
#include "formats.h"
#include "internal.h"
#include "video.h"

enum { Y, U, V, A };

typedef struct
{
  uint8_t R; ///< Red.
  uint8_t G; ///< Green.
  uint8_t B; ///< Blue.
  uint8_t A; ///< Alpha.
} RGBA;

typedef struct {
    
	const AVClass 	 *class;

    int               bpp;         //< bytes per pixel
    int               hsub, vsub;  //< chroma subsampling
    int               video_w, video_h, video_format;

    // logo parameters
    int               x, y;         //< requested offsets of logo on video
    int               w, h, format; //< width, height, pix-format
    int               app_x, app_y; //< final logo offsets
	int				  app_w, app_h; //< final logo sizes
    char             *file_name;
    int               alpha_R,alpha_G,alpha_B;
    
    AVFormatContext   *pFormatCtx;
    AVCodecContext    *pCodecCtx;
    AVCodec           *pCodec;
	
	int               videoStream;
	AVPacket          packet;

	struct SwsContext *sws;
	
    AVFrame           *plogo_frame;						//< Original image frame
    uint8_t           *buffer_logo_frame;				//< Original image pixel buffer
	
    AVFrame           *plogo_frame_rgba32;				//< RGBA32 variant image frame 
    uint8_t           *buffer_logo_frame_rgba32;		//< RGBA32 variant pixel buffer
    
	AVFrame           *plogo_frame_video_format;		//< Target format image frame
    uint8_t           *buffer_logo_frame_video_format;  //< Target format pixel buffer
    
	uint8_t           *pRuler_0;			//< Alpha values for planes w/o subsampling
    uint8_t           *pRuler_1_2;			//< Alpha values for planes with subsampling
	
} LogoContext;

#define OFFSET(x) offsetof(LogoContext, x)
#define FLAGS AV_OPT_FLAG_VIDEO_PARAM|AV_OPT_FLAG_FILTERING_PARAM

static const AVOption logo_options[] = {
    { "x",           "set the box top-left corner x position", OFFSET(x), AV_OPT_TYPE_INT, {.i64=0}, INT_MIN, INT_MAX, FLAGS },
    { "y",           "set the box top-left corner y position", OFFSET(y), AV_OPT_TYPE_INT, {.i64=0}, INT_MIN, INT_MAX, FLAGS },
    { "img",         "set the image path", OFFSET(file_name), AV_OPT_TYPE_STRING, {.str=""}, CHAR_MIN, CHAR_MAX, FLAGS },
    { "i",           "set the image path", OFFSET(file_name), AV_OPT_TYPE_STRING, {.str=""}, CHAR_MIN, CHAR_MAX, FLAGS },
    {NULL},
};

AVFILTER_DEFINE_CLASS(logo);

//
// Function declarations.
//

static int load_logo_create_frames(AVFilterContext *ctx);

static av_cold int init(AVFilterContext *ctx, const char *args)
{
    LogoContext *logo = ctx->priv;
    static const char *shorthand[] = { "x", "y", "i", NULL };
    int ret;
	
    av_log(NULL, AV_LOG_INFO, "vf_logo: init\n");
    av_log(NULL, AV_LOG_INFO, "vf_logo: version %s\n", VF_LOGO_VERSION);

    logo->class = &logo_class;
    av_opt_set_defaults(logo);

    if ((ret = av_opt_set_from_string(logo, args, shorthand, "=", ":")) < 0)
        return ret;
		
    logo->alpha_R = -1;
    logo->alpha_G = -1;
    logo->alpha_B = -1;
		
	//
	// Replace exclamation mark into drive separator character.
	//
		
	for (int inx = 0; logo->file_name[ inx ]; inx++)
	{
		if (logo->file_name[ inx ] == '!') logo->file_name[ inx ] = ':';
	}
	
    av_log(NULL, AV_LOG_INFO, "vf_logo: x:%d y:%d image:%s\n", logo->x, logo->y, logo->file_name);

	return load_logo_create_frames(ctx);
}

static int load_logo_create_frames(AVFilterContext *ctx)
{
    LogoContext	*logo = ctx->priv;
    
	int err;
	int numBytes;
	int frameFinished;

    av_log(NULL, AV_LOG_INFO, "vf_logo: load_logo_create_frames\n");

	//
	// The last three parameters specify the file format, buffer size and format
	// parameters; by simply specifying NULL or 0 we ask libavformat to auto-detect
	// the format and use a default buffer size.
	//
	
	if ((err = avformat_open_input(&logo->pFormatCtx, logo->file_name, NULL, NULL)) != 0) 
	{
		av_log(NULL, AV_LOG_ERROR, " vf_logo: cannot open logo file %s error is %d\n", logo->file_name, err);
		return -1;
	}

	//
	// This fills the streams field of the AVFormatContext with valid information.
	//
	
	if (avformat_find_stream_info(logo->pFormatCtx, NULL) < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "vf_logo: failed to find stream info in logo file\n");
		return -1;
	}

	//
	// We simply use the first video stream.
	//
	
	logo->videoStream = -1;
	
	for (int i = 0; i < logo->pFormatCtx->nb_streams; i++)
	{
		if (logo->pFormatCtx->streams[ i ]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			logo->videoStream = i;
			break;
		}
	}
	
	if (logo->videoStream == -1)
	{
		av_log(NULL, AV_LOG_ERROR, "vf_logo: failed to find any video stream in logo file\n");
		return -1;
	}

	//
	// Get a pointer to the codec context for the video stream.
	//
	
	logo->pCodecCtx = logo->pFormatCtx->streams[ logo->videoStream ]->codec;
	
	//
	// OK, so now we've got a pointer to the so-called codec context for our video
	// stream, but we still have to find the actual codec and open it.
	//
	
	//
	// Find the decoder for the video stream.
	//
	
	logo->pCodec = avcodec_find_decoder(logo->pCodecCtx->codec_id);
	
	if (logo->pCodec == NULL)
	{
		av_log(NULL, AV_LOG_ERROR, "vf_logo: failed to find any codec for decoding logo frame.\n");
		return -1;
	}

	//
	// Inform the codec that we can handle truncated bitstreams, 
	// i.e. bitstreams where frame boundaries can fall in 
	// the middle of packets.
	//
	
	if (logo->pCodec->capabilities & CODEC_CAP_TRUNCATED)
	{
		logo->pCodecCtx->flags |= CODEC_FLAG_TRUNCATED;
	}
	
	//
	// Open codec.
	//
	
	av_log(NULL, AV_LOG_INFO, "vf_logo: avcodec_open\n");
	
	if (avcodec_open2(logo->pCodecCtx, logo->pCodec, NULL) < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "vf_logo: failed to open codec\n");
		return -1;
	}

	//
	// Allocate an AVFrame structure. 
	//
	
	logo->plogo_frame = avcodec_alloc_frame();
	
	if (logo->plogo_frame == NULL)
	{
		av_log(NULL, AV_LOG_ERROR, "vf_logo: failed to alloc plogo_frame\n");
		return -1;
	}
  
	//
	// Determine required buffer size and allocate buffer 
	// for uncompressed image data.
	//
	
	numBytes = avpicture_get_size(logo->pCodecCtx->pix_fmt, 
								  logo->pCodecCtx->width, 
								  logo->pCodecCtx->height
								  );
	
	logo->buffer_logo_frame = av_malloc(numBytes);

	//
	// Assign appropriate parts of buffer to image planes in plogo_frame.
	//
	
	avpicture_fill((AVPicture *) logo->plogo_frame, 
		logo->buffer_logo_frame, 
		logo->pCodecCtx->pix_fmt, 
		logo->pCodecCtx->width, 
		logo->pCodecCtx->height
		);

	logo->w      = logo->pCodecCtx->width;
	logo->h      = logo->pCodecCtx->height;
	logo->format = logo->pCodecCtx->pix_fmt;

	av_log(NULL, AV_LOG_INFO, "vf_logo: logo size is %dx%d pix-fmt:%s\n", logo->w, logo->h, av_get_pix_fmt_name(logo->format));

	av_log(NULL, AV_LOG_INFO, "vf_logo: linesizes [0]=%d [1]=%d [2]=%d [3]=%d\n",
		((AVPicture *) logo->plogo_frame)->linesize[ 0 ], 
		((AVPicture *) logo->plogo_frame)->linesize[ 1 ],
		((AVPicture *) logo->plogo_frame)->linesize[ 2 ], 
		((AVPicture *) logo->plogo_frame)->linesize[ 3 ]
		);
  
	//
	// Now read the original image frame.
	//
	
	av_log(NULL, AV_LOG_INFO, "vf_logo: av_read_frame\n");
	
	frameFinished = 0;

	while (av_read_frame(logo->pFormatCtx, &logo->packet) >= 0)
	{		
		av_log(NULL, AV_LOG_INFO, "vf_logo: got a frame\n");
		
		//
		// Is this a packet from our video stream?
		//
		
		if (logo->packet.stream_index == logo->videoStream)
		{
			//
			// Decode video frame.
			//
			
			av_log(NULL, AV_LOG_INFO, "vf_logo: avcodec_decode_video\n");
			
			avcodec_decode_video2(logo->pCodecCtx, 
								  logo->plogo_frame, 
								  &frameFinished,
								  &logo->packet
								  );
		}
		
		//
		// Free the packet that was allocated by av_read_frame.
		//
		
		av_free_packet(&logo->packet);
			
		//
		// Did we get a video frame?
		//
		
		if (frameFinished)
		{
			av_log(NULL, AV_LOG_INFO, "vf_logo: got a finished frame, fine!!\n");
			
			av_log(NULL, AV_LOG_INFO, "vf_logo: avcodec decoded logo image to pix_fmt=%s\n", av_get_pix_fmt_name(logo->pCodecCtx->pix_fmt));
			
			break;
		}
	}
  
	if (! frameFinished)
	{
		av_log(NULL, AV_LOG_ERROR, "vf_logo: could not read valid data from image.\n");
		return -1;
	}
	
	//
	// Now we have read the image in whatever format.
	// We need the overlay image to be in RGBA, so
	// we will convert it with the swscaler functions.
	//
	  
	if (logo->pCodecCtx->pix_fmt == PIX_FMT_RGBA)
	{
		//
		// Source logo image is in correct format, 
		// simply copy the pointers.
		//
		
		logo->plogo_frame_rgba32       = logo->plogo_frame;
		logo->buffer_logo_frame_rgba32 = logo->buffer_logo_frame;
	}
	else
	{
		//
		// Transform it with swscaler to PIX_FMT_RGBA.
		//
		
		av_log(NULL, AV_LOG_INFO, "vf_logo: transform logo image from pix_fmt=%s to RGBA\n", 
			av_get_pix_fmt_name(logo->pCodecCtx->pix_fmt));
    
		if (logo->pCodecCtx->pix_fmt == PIX_FMT_RGB24)
		{    
			av_log(NULL, AV_LOG_INFO, " vf_logo: image of pix_fmt=%s has no alpha path!\n", 
				av_get_pix_fmt_name(logo->pCodecCtx->pix_fmt));
		}
		
		//
		// Allocate an AVFrame structure.
		//
		
		logo->plogo_frame_rgba32 = avcodec_alloc_frame();
		
		if (logo->plogo_frame_rgba32 == NULL)
		{
			av_log(NULL, AV_LOG_ERROR, "vf_logo: failed to alloc plogo_frame_rgba32\n");
			return -1;
		}
		
		logo->sws = sws_getCachedContext(NULL, 
					logo->w, logo->h, 
					logo->pCodecCtx->pix_fmt,
                    logo->w, logo->h, 
					PIX_FMT_RGBA,
                    SWS_BICUBIC,
					NULL, NULL, NULL
					);
					
		if (logo->sws == NULL)
		{
			av_log(NULL, AV_LOG_ERROR, "vf_logo: cannot initialize the to-RGBA conversion context\n");
			return -1;
		}
		
		//
		// Determine required buffer size and allocate buffer.
		//
		
		numBytes = avpicture_get_size(PIX_FMT_RGBA, logo->w, logo->h);
		logo->buffer_logo_frame_rgba32 = av_malloc(numBytes);
		
		//
		// Assign appropriate parts of buffer to image planes in plogo_frame_rgba32.
		//
		
		avpicture_fill((AVPicture *) logo->plogo_frame_rgba32, 
					   logo->buffer_logo_frame_rgba32, 
					   PIX_FMT_RGBA, 
					   logo->w, 
					   logo->h
					   );

		//
		// Transform to RGBA pixel format.
		//
		
		sws_scale(logo->sws, 
				  (const uint8_t * const*) logo->plogo_frame->data, 
				  logo->plogo_frame->linesize, 
				  0, logo->h,
				  logo->plogo_frame_rgba32->data, 
				  logo->plogo_frame_rgba32->linesize
				  );
				  
		//
		// Deallocate context and nuke.
		//
				  
		sws_freeContext(logo->sws);
		logo->sws = NULL;
		  
		av_log(NULL, AV_LOG_INFO, "vf_logo: RGBA linesizes [0]=%d [1]=%d [2]=%d [3]=%d\n",
				((AVPicture *) logo->plogo_frame_rgba32)->linesize[ 0 ], 
				((AVPicture *) logo->plogo_frame_rgba32)->linesize[ 1 ],
				((AVPicture *) logo->plogo_frame_rgba32)->linesize[ 2 ], 
				((AVPicture *) logo->plogo_frame_rgba32)->linesize[ 3 ]);				
	}

	/*
	av_log(NULL, AV_LOG_INFO, "vf_logo: alpha_transparency of frame is:%d\n",
         img_get_alpha_info((AVPicture *)logo->plogo_frame_rgba32,
							PIX_FMT_RGBA, 
							logo->w, 
							logo->h)
							);
	*/
	
	//
	// Large debug dump of overlay image if required.
	//
	
	if (av_log_get_level() >= 2)
	{  
		uint8_t *scanline = logo->plogo_frame_rgba32->data[ 0 ];
		
		for (int i = 0; i < logo->h; i++)
		{
			RGBA *pix = (RGBA *) scanline;
			
			for (int j = 0; j < logo->w; j++)
			{
				//av_log(NULL, AV_LOG_INFO, "vf_logo: image (%3d,%3d) R=%3d G=%3d B=%3d A=%3d\n",i,j,pix->R,pix->G,pix->B,pix->A);
				
				pix++;
			}
			
			scanline += logo->plogo_frame_rgba32->linesize[ 0 ];
		}
	}
     
	//
	// Check if fixed alpha color was desired in options
	// and adjust pixels data if so.
	//
	
	if (logo->alpha_B > -1)
	{  
		uint8_t *scanline = logo->plogo_frame_rgba32->data[ 0 ];
		
		for (int i = 0; i < logo->h; i++)
		{
			RGBA *pix = (RGBA *) scanline;
			
			for (int j = 0; j < logo->w; j++)
			{
				//
				// Preset non transparent.
				//
				
				pix->A = 255;
				
				if ((logo->alpha_R == pix->R) && 
					(logo->alpha_G == pix->G) &&
					(logo->alpha_B == pix->B))
				{
					//
					// Pixel color matches transparent pixel,
					// so set to transparent.
					//
					
					pix->A = 0;
				}
			
				pix++;
			}
			
			scanline += logo->plogo_frame_rgba32->linesize[ 0 ];
		}
	}
	
    return 0;
}

static av_cold void uninit(AVFilterContext *ctx)
{
    LogoContext *logo = ctx->priv;
    av_opt_free(logo);
}

static int query_formats(AVFilterContext *ctx)
{
    static const enum AVPixelFormat pix_fmts[] = {
        AV_PIX_FMT_YUV444P,  AV_PIX_FMT_YUV422P,  AV_PIX_FMT_YUV420P,
        AV_PIX_FMT_YUV411P,  AV_PIX_FMT_YUV410P,
        AV_PIX_FMT_YUVJ444P, AV_PIX_FMT_YUVJ422P, AV_PIX_FMT_YUVJ420P,
        AV_PIX_FMT_YUV440P,  AV_PIX_FMT_YUVJ440P,
        AV_PIX_FMT_NONE
    };
	
    av_log(NULL, AV_LOG_INFO, "vf_logo: query_formats\n");

    ff_set_common_formats(ctx, ff_make_format_list(pix_fmts));
	
    return 0;
}

static int config_input(AVFilterLink *inlink)
{
    LogoContext *logo = inlink->dst->priv;
 
	 int	r_0_numBytes;
	 int	r_1_2_numBytes;
	 
	 RGBA   	*pRGBA;
	 uint8_t	*pRGBA_sol;
	 uint8_t	*pRuler;

    av_log(NULL, AV_LOG_DEBUG, "vf_logo: config_input_main\n");
    
	av_log(inlink->dst, AV_LOG_INFO, "x:%d y:%d w:%d h:%d\n",logo->x, logo->y, logo->w, logo->h);
	av_log(inlink->dst, AV_LOG_INFO, "input video format:%s\n", av_get_pix_fmt_name(inlink->format));
 
	switch (inlink->format) 
	{
		case PIX_FMT_ARGB:
		case PIX_FMT_ABGR:
		case PIX_FMT_RGBA:
		case PIX_FMT_BGRA:
			logo->bpp = 4;
			break;
			
		case PIX_FMT_RGB24:
		case PIX_FMT_BGR24:
			logo->bpp = 3;
			break;
			
		case PIX_FMT_RGB565LE:
		case PIX_FMT_RGB555LE:
		case PIX_FMT_BGR565LE:
		case PIX_FMT_BGR555LE:
		case PIX_FMT_RGB565BE:
		case PIX_FMT_RGB555BE:
		case PIX_FMT_BGR565BE:
		case PIX_FMT_BGR555BE:
		case PIX_FMT_GRAY16BE:
		case PIX_FMT_GRAY16LE:
			logo->bpp = 2;
			break;
			
		default:
			logo->bpp = 1;
	}
	
	//
	// Figure out subsampling.
	//
	// avcodec_get_chroma_sub_sample(inlink->format,&logo->hsub, &logo->vsub);
	//
	
    logo->hsub = av_pix_fmt_descriptors[inlink->format].log2_chroma_w;
    logo->vsub = av_pix_fmt_descriptors[inlink->format].log2_chroma_h;
	
	av_log(inlink->dst, AV_LOG_DEBUG, "subsampling h:%d v:%d\n", logo->hsub, logo->vsub);

	logo->video_w      = inlink->w;
	logo->video_h      = inlink->h;
	logo->video_format = inlink->format;

	av_log(inlink->dst, AV_LOG_DEBUG, "vf_logo: video size is %dx%d pix-fmt:%s bpp:%d\n", 
		   logo->video_w, 
		   logo->video_h, 
		   av_get_pix_fmt_name(logo->video_format), 
		   logo->bpp
		   );
		   
	//
	// Choose the best already present format from 
	// overlay image or create matching pixel formats
	// if allocated formats do not match target video
	// format.
	//
	
	if (inlink->format == logo->format)
	{
		logo->plogo_frame_video_format       = logo->plogo_frame;
		logo->buffer_logo_frame_video_format = logo->buffer_logo_frame;
	}
	else 
	if (inlink->format == PIX_FMT_RGBA)
	{
		logo->plogo_frame_video_format       = logo->plogo_frame_rgba32;
		logo->buffer_logo_frame_video_format = logo->buffer_logo_frame_rgba32;
	}
	else
	{
		//
		// Transform it with swscaler from PIX_FMT_RGBA to inlink->format.
		//
		
		int numBytes;
		
		av_log(NULL, AV_LOG_DEBUG, "vf_logo: transform logo image from RGBA to pix_fmt=%s\n", av_get_pix_fmt_name(inlink->format));

		//
		// Allocate an AVFrame structure
		//
		
		logo->plogo_frame_video_format = avcodec_alloc_frame();
		
		if (logo->plogo_frame_video_format == NULL)
		{
			av_log(NULL, AV_LOG_ERROR, "vf_logo: failed to alloc plogo_frame_video_format\n");
			return -1;
		}
		
		logo->sws = sws_getCachedContext(NULL, 
						logo->w, logo->h, 
						PIX_FMT_RGBA,
                        logo->w, logo->h, 
						inlink->format,
                        SWS_BICUBIC, 
						NULL, NULL, NULL
						);
						
		if (logo->sws == NULL)
		{
			av_log(NULL, AV_LOG_ERROR, " vf_logo: cannot initialize the to-video_format conversion context\n");
			return -1;
		}

		//
		// Determine required buffer size and allocate buffer.
		//
		
		numBytes = avpicture_get_size(inlink->format, logo->w, logo->h);
		logo->buffer_logo_frame_video_format = av_malloc(numBytes);
		
		//
		// Assign appropriate parts of buffer to image planes in plogo_frame.
		//
		
		avpicture_fill((AVPicture *) logo->plogo_frame_video_format, 
				       logo->buffer_logo_frame_video_format, 
					   inlink->format, 
					   logo->w, logo->h
					   );

		//
		// Transform to video pixel format.
		
		sws_scale(logo->sws, 
				  (const uint8_t * const*) logo->plogo_frame_rgba32->data, 
				  logo->plogo_frame_rgba32->linesize, 
				  0, logo->h,
				  logo->plogo_frame_video_format->data, 
				  logo->plogo_frame_video_format->linesize
				  );
		
		//
		// Deallocate context and nuke.
		//
		
		sws_freeContext(logo->sws);
		logo->sws = NULL;
		
		av_log(NULL, AV_LOG_DEBUG, "vf_logo: logo linesizes [0]=%d [1]=%d [2]=%d [3]=%d in videos pix-fmt\n",
			  ((AVPicture *) logo->plogo_frame_video_format)->linesize[ 0 ], 
			  ((AVPicture *) logo->plogo_frame_video_format)->linesize[ 1 ],
			  ((AVPicture *) logo->plogo_frame_video_format)->linesize[ 2 ], 
			  ((AVPicture *) logo->plogo_frame_video_format)->linesize[ 3 ]);
	}
	
	//
	// Figure out the final position of logo
	// in video based on input video size and
	// relative positioning.
	//
	
	logo->app_x = logo->x;
	logo->app_y = logo->y;
	logo->app_w = logo->w;
	logo->app_h = logo->h;

	//
	// If negative, it is based on right/bottom border of logo and video.
	//
	
	if (logo->x < 0) logo->app_x = logo->video_w - logo->w + logo->x;
	if (logo->y < 0) logo->app_y = logo->video_h - logo->h + logo->y;

	//
	// Never cross left border or top of video, just shift it.
	//
	
	if (logo->app_x < 0) logo->app_x = 0;
	if (logo->app_y < 0) logo->app_y = 0;

	//
	// Fit overlay into video if overlapping borders.
	//
	
	if (logo->app_x > logo->video_w - 1) logo->app_x = logo->video_w - 1;
	if (logo->app_y > logo->video_h - 1) logo->app_y = logo->video_h - 1;
	
	if (logo->app_w > logo->video_w - logo->app_x) logo->app_w = logo->video_w - logo->app_x;
	if (logo->app_h > logo->video_h - logo->app_y) logo->app_h = logo->video_h - logo->app_y;

	av_log(NULL, AV_LOG_INFO, "vf_logo: overlay logo (%d,%d) at (%d,%d) w=%d h=%d onto video (%d,%d) pix-fmt=%s\n",
				logo->w,       logo->h,
				logo->app_x,   logo->app_y,
				logo->app_w,   logo->app_h,
				logo->video_w, logo->video_h,
				av_get_pix_fmt_name(logo->video_format));

	//
	// Most target video formats do not know transparency.
	// We need to create vectors with video-format conform
	// subsampling which hold the alpha value for each pixel
	// to be used in overlay transformation.
	//
	
	r_0_numBytes   = logo->app_w * logo->app_h;
	logo->pRuler_0 = av_mallocz(r_0_numBytes);

	if (logo->hsub == 0 && logo->vsub == 0)
	{
		//
		// Video format does not use subsampling,
		// simply duplicate ruler.
		//
		
		r_1_2_numBytes   = r_0_numBytes;
		logo->pRuler_1_2 = logo->pRuler_0;
	}
	else 
	{
		r_1_2_numBytes   = ((logo->app_w + (1 << logo->hsub) - 1) >> logo->hsub) * 
						   ((logo->app_h + (1 << logo->vsub) - 1) >> logo->vsub);
		logo->pRuler_1_2 = av_mallocz(r_1_2_numBytes);
	}

	if ((logo->pRuler_0 == NULL) || (logo->pRuler_1_2 == NULL))
	{
		av_log(NULL, AV_LOG_ERROR, "vf_logo: av_mallocz for rulers failed\n");
		return -1;
	}
	
	av_log(NULL, AV_LOG_INFO, "vf_logo: ruler sizes R_0:%d R_1_2:%d\n", r_0_numBytes, r_1_2_numBytes);

	//
	// Fill the rulers from RGBA alpha information.
	//
	
	pRuler    = logo->pRuler_0;
	pRGBA_sol = logo->plogo_frame_rgba32->data[ 0 ];
	
	for (int i = 0; i < logo->app_h; i++) 
	{
		pRGBA = (RGBA *) pRGBA_sol;
		
		for (int j = 0; j < logo->app_w; j++) 
		{
			*pRuler = pRGBA->A;
			
			pRuler++;
			pRGBA++;
		}
		
		pRGBA_sol += logo->plogo_frame_rgba32->linesize[ 0 ]; 
	}

	if (logo->pRuler_0 != logo->pRuler_1_2) 
	{
		int inc_i = 1 << logo->vsub;
		int inc_j = 1 << logo->hsub;

		pRuler    = logo->pRuler_1_2;
		pRGBA_sol = logo->plogo_frame_rgba32->data[0];
		
		for (int i = 0; i < logo->app_h; i += inc_i) 
		{
			pRGBA = (RGBA *) pRGBA_sol;
		  
			for (int j = 0; j < logo->app_w; j += inc_j) 
			{
				*pRuler = pRGBA->A;

				pRuler++;
				pRGBA += inc_j;
			}
		  
			pRGBA_sol += inc_i * logo->plogo_frame_rgba32->linesize[ 0 ];
		}
	}
	
    return 0;
}

#ifdef WANT_draw_slice_box

static int filter_frame_box(AVFilterLink *inlink, AVFilterBufferRef *frame)
{	
    LogoContext 	  *logo   = inlink->dst->priv;
	AVFilterBufferRef *picref = frame;

	int xb = logo->app_x;
	int yb = logo->app_y;
	
	unsigned char *vidrow[4];
	unsigned char yuv_color[4];
	double alpha;

	yuv_color[Y] = RGB_TO_Y_CCIR(255, 0, 0);
	yuv_color[U] = RGB_TO_U_CCIR(255, 0, 0, 0);
	yuv_color[V] = RGB_TO_V_CCIR(255, 0, 0, 0);
	yuv_color[A] = 128;
	
	alpha = (double) yuv_color[A] / 255;
    
	for (int y = FFMAX(yb, 0); y < frame->video->h && y < (yb + logo->app_h); y++) 
	{
		vidrow[ 0 ] = picref->data[ 0 ] + y * picref->linesize[ 0 ];

		for (int plane = 1; plane < 3; plane++)
		{
			vidrow[ plane ] = picref->data[ plane ] + picref->linesize[ plane ] * (y >> logo->vsub);
		}
		
		for (int x = FFMAX(xb, 0); x < (xb + logo->app_w) && x < picref->video->w; x++) 
		{
			int xh = x >> logo->hsub;
			
			if ((y - yb < 3) || (yb + logo->app_h - y < 4) ||
				(x - xb < 3) || (xb + logo->app_w - x < 4)) 
			{
				vidrow[0][ x  ] = (1 - alpha) * vidrow[0][ x  ] + alpha * yuv_color[Y];
				vidrow[1][ xh ] = (1 - alpha) * vidrow[1][ xh ] + alpha * yuv_color[U];
				vidrow[2][ xh ] = (1 - alpha) * vidrow[2][ xh ] + alpha * yuv_color[V];
			}
		}
	}
	
    return ff_filter_frame(inlink->dst->outputs[0], frame);
}

#endif

static int filter_frame(AVFilterLink *inlink, AVFilterBufferRef *frame)
{
    LogoContext 	  *logo   = inlink->dst->priv;
	AVFilterBufferRef *picref = frame;
	AVPicture		  *imgref = (AVPicture *) logo->plogo_frame_video_format;	
	AVPicture		  *rgbref = (AVPicture *) logo->plogo_frame_rgba32;	
	
	int xb = logo->app_x;
	int yb = logo->app_y;
	
	unsigned char *vidrow[ 4 ];
	unsigned char *imgrow[ 4 ];
	unsigned char *rulrow;

	for (int y = FFMAX(yb, 0); y < frame->video->h && y < (yb + logo->app_h); y++) 
	{
		vidrow[ 0 ] = picref->data[ 0 ] + y        * picref->linesize[ 0 ];
		imgrow[ 0 ] = imgref->data[ 0 ] + (y - yb) * imgref->linesize[ 0 ];
		rulrow	    = rgbref->data[ 0 ] + (y - yb) * rgbref->linesize[ 0 ];
		
		for (int plane = 1; plane < 3; plane++)
		{
			vidrow[ plane ] = picref->data[ plane ] + picref->linesize[ plane ] * ( y       >> logo->vsub);
			imgrow[ plane ] = imgref->data[ plane ] + imgref->linesize[ plane ] * ((y - yb) >> logo->vsub);
		}
		
		for (int x = FFMAX(xb, 0); x < (xb + logo->app_w) && x < picref->video->w; x++) 
		{
			int xh  =  x       >> logo->hsub;	// Sub sampled video image index
			int xbh = (x - xb) >> logo->hsub;	// Sub sampled logo image index
			int xba = (x - xb) << 2;			// Non sub sampled RGBA logo image index
			
			double alpha = rulrow[ xba + 3 ] / (double) 255;
			
			vidrow[0][ x  ] = (1 - alpha) * vidrow[0][ x  ] + alpha * imgrow[0][ x - xb ];
			vidrow[1][ xh ] = (1 - alpha) * vidrow[1][ xh ] + alpha * imgrow[1][ xbh    ];
			vidrow[2][ xh ] = (1 - alpha) * vidrow[2][ xh ] + alpha * imgrow[2][ xbh    ];
		}
	}
	
    return ff_filter_frame(inlink->dst->outputs[0], frame);
}

static const AVFilterPad avfilter_vf_logo_inputs[] = {
    {
        .name             = "default",
        .type             = AVMEDIA_TYPE_VIDEO,
        .config_props     = config_input,
        .get_video_buffer = ff_null_get_video_buffer,
        .filter_frame     = filter_frame,
        .min_perms        = AV_PERM_WRITE | AV_PERM_READ,
    },
    { NULL }
};

static const AVFilterPad avfilter_vf_logo_outputs[] = {
    {
        .name = "default",
        .type = AVMEDIA_TYPE_VIDEO,
    },
    { NULL }
};

AVFilter avfilter_vf_logo = {
    .name      = "logo",
    .description = NULL_IF_CONFIG_SMALL("Logo overlay filter."),
    .priv_size = sizeof(LogoContext),
    .init      = init,
    .uninit    = uninit,

    .query_formats   = query_formats,
    .inputs    = avfilter_vf_logo_inputs,
    .outputs   = avfilter_vf_logo_outputs,
    .priv_class = &logo_class,
};
