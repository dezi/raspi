/**
 * libavfilter/vf_scene.c
 * filter for scene detection on video
 * Copyright (c) 2012 Dennis Zierahn   (update to current ffmpeg)
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
 * example of using libavfilter/vf_scene:
 *
 * ffmpeg -i inputvideofile -vf scene=<threshold>:<stopframes>:<minframes>:scenedir -y outputvideofile
 *
 * images are send to directory scenedir.
 *
 * threshold <INT>
 *
 *   Threshold between 0 and 255. Lower threshold yields more images.
 *
 * stopframes <INT>
 *
 *   Number of frames where no scene is detected after cut.
 *
 * minframes <INT>
 *
 *   Number of frames to force image cut.
 *
 */

#define VF_SCENE_VERSION "1.0.0.2012.04.09"
 
#include "libavutil/parseutils.h"
#include "libavutil/colorspace.h"
#include "libavutil/avstring.h"
#include "libavutil/pixdesc.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "avfilter.h"
#include <jpeglib.h>
#include <sys/stat.h>

enum { Y, U, V, A };

typedef struct {

    int               bpp;         //< bytes per pixel
    int               hsub, vsub;  //< chroma subsampling
    int               video_w, video_h, video_format;
	
	int				  thisframe;
	int				  lastscene;
	int				  samplecount;
	int				  samplediff;

    // scene parameters
	
 	int				  threshold;
	int				  stopframes;
	int				  minframes;
	char              file_name[ 2048 ];
	
	uint8_t			  *cmpbuf[ 3 ];
    int64_t			  lastpts;
	
} SceneContext;

typedef struct
{
  uint8_t R; ///< Red.
  uint8_t G; ///< Green.
  uint8_t B; ///< Blue.
  uint8_t A; ///< Alpha.
} RGBA;

//
// Function declarations.
//

static av_cold int init(AVFilterContext *ctx, const char *args, void *opaque)
{
    SceneContext *scene = ctx->priv;
	int num_fields;

    av_log(NULL, AV_LOG_DEBUG, "vf_scene: init\n");
    av_log(NULL, AV_LOG_INFO, "vf_scene: version %s\n", VF_SCENE_VERSION);

    if(!args || strlen(args) > 1024) 
	{
        av_log(NULL, AV_LOG_ERROR, "vf_scene: Invalid arguments!\n");
		
        return -1;
    }

	scene->threshold   = 100;
	scene->stopframes  =   8;
	scene->minframes   = 250;
	scene->thisframe   =  -1;
	scene->lastscene   =  -1;
	scene->samplecount =   0;
	scene->samplediff  =  -1;

	//
	// Retry 3 argument option version.
	//

	num_fields = sscanf(args, "%d:%d:%d:%512[^:]",
						&scene->threshold, 
						&scene->stopframes, 
						&scene->minframes, 
						scene->file_name
						);
						
	if (num_fields == 4) 
	{
		//
		// Replace exclamation mark into drive separator character.
		//
		
		for (int inx = 0; scene->file_name[ inx ]; inx++)
		{
			if (scene->file_name[ inx ] == '!') scene->file_name[ inx ] = ':';
		}

		av_log(NULL, AV_LOG_INFO, "vf_scene: threshold=%d stopframes=%d minframes=%d directory=%s\n",
			scene->threshold, 
			scene->stopframes, 
			scene->minframes, 
			scene->file_name
			);
	}
	else 
	{
		av_log(NULL, AV_LOG_ERROR, "vf_scene: expected 3 arguments\n\t\t\tscene=x:y:scenedir\n\t\t\tbut wrong args are given: '%s'\n", args);
	  
		return -1;
	}

	return 0;
}

static av_cold void uninit(AVFilterContext *ctx)
{
    av_log(NULL, AV_LOG_DEBUG, "vf_scene: uninit\n");
}

static int query_formats(AVFilterContext *ctx)
{
    enum PixelFormat pix_fmts[] = {
        PIX_FMT_YUV444P,  PIX_FMT_YUV422P,  PIX_FMT_YUV420P,
        PIX_FMT_YUV411P,  PIX_FMT_YUV410P,
        PIX_FMT_YUVJ444P, PIX_FMT_YUVJ422P, PIX_FMT_YUVJ420P,
        PIX_FMT_YUV440P,  PIX_FMT_YUVJ440P,
        PIX_FMT_NONE
    };
	
    av_log(NULL, AV_LOG_DEBUG, "vf_scene: query_formats\n");

    avfilter_set_common_pixel_formats(ctx, avfilter_make_format_list(pix_fmts));
    
	return 0;
}

static int config_input_main(AVFilterLink *inlink)
{
    SceneContext *scene = inlink->dst->priv;
 
    av_log(inlink->dst, AV_LOG_DEBUG, "config_input_main\n");
    
	av_log(inlink->dst, AV_LOG_INFO, "input video format:%s\n", av_get_pix_fmt_name(inlink->format));
 
	switch (inlink->format) 
	{
		case PIX_FMT_ARGB:
		case PIX_FMT_ABGR:
		case PIX_FMT_RGBA:
		case PIX_FMT_BGRA:
			scene->bpp = 4;
			break;
			
		case PIX_FMT_RGB24:
		case PIX_FMT_BGR24:
			scene->bpp = 3;
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
			scene->bpp = 2;
			break;
			
		default:
			scene->bpp = 1;
	}
	
	//
	// Figure out subsampling.
	//
	// avcodec_get_chroma_sub_sample(inlink->format,&scene->hsub, &scene->vsub);
	//
	
    scene->hsub = av_pix_fmt_descriptors[inlink->format].log2_chroma_w;
    scene->vsub = av_pix_fmt_descriptors[inlink->format].log2_chroma_h;
	
	av_log(inlink->dst, AV_LOG_DEBUG, "subsampling h:%d v:%d\n", scene->hsub, scene->vsub);

	scene->video_w      = inlink->w;
	scene->video_h      = inlink->h;
	scene->video_format = inlink->format;

	av_log(inlink->dst, AV_LOG_DEBUG, "vf_scene: video size is %dx%d pix-fmt:%s bpp:%d\n", 
		   scene->video_w, 
		   scene->video_h, 
		   av_get_pix_fmt_name(scene->video_format), 
		   scene->bpp
		   );
			
	//
	// Allocate compare buffers w/o subsampling.
	//
	
	scene->cmpbuf[ 0 ] = av_malloc(scene->video_h * scene->video_w);
	scene->cmpbuf[ 1 ] = av_malloc(scene->video_h * scene->video_w);
	scene->cmpbuf[ 2 ] = av_malloc(scene->video_h * scene->video_w);
	
    av_log(inlink->dst, AV_LOG_DEBUG, "config_input_main done\n");
	
    return 0;
}

static int do_mkdir(const char *path, mode_t mode)
{
    struct stat     st;
    int             status = 0;

    if (stat(path, &st) != 0)
    {
        /* Directory does not exist */
        if (mkdir(path, mode) != 0)
            status = -1;
    }
    else if (!S_ISDIR(st.st_mode))
    {
        errno = ENOTDIR;
        status = -1;
    }

    return(status);
}

static int mkpath(const char *path, mode_t mode)
{
    char           *pp;
    char           *sp;
    int             status;
    char           *copypath;

	copypath = av_malloc(strlen(path) + 1);
	strcpy(copypath,path);
	
    status = 0;
    pp = copypath;
    while (status == 0 && (sp = strchr(pp, '/')) != 0)
    {
        if (sp != pp)
        {
            /* Neither root nor double slash in path */
            *sp = '\0';
            status = do_mkdir(copypath, mode);
            *sp = '/';
        }
        pp = sp + 1;
    }
	
    if (status == 0) status = do_mkdir(path, mode);
	
    av_free(copypath);
	
    return (status);
}

static void draw_slice_scene(AVFilterLink *inlink, int y0, int h, int slice_dir)
{
    SceneContext 	  *scene   = inlink->dst->priv;
	AVFilterBufferRef *picref = inlink->cur_buf;

	unsigned char *vidrow[ 3 ];
	unsigned char *cmprow[ 3 ];
	
    int yc, uc, vc;
    int r, g, b;

	for (int y = y0; y < (y0 + h); y++) 
	{
		vidrow[ 0 ] = picref->data[ 0 ] + picref->linesize[ 0 ] * y;
		cmprow[ 0 ] = scene->cmpbuf[ 0 ] + scene->video_w * y;
		
		vidrow[ 1 ] = picref->data[ 1 ] + picref->linesize[ 1 ] * (y >> scene->vsub);
		cmprow[ 1 ] = scene->cmpbuf[ 1 ] + scene->video_w * y;
		
		vidrow[ 2 ] = picref->data[ 2 ] + picref->linesize[ 2 ] * (y >> scene->vsub);
		cmprow[ 2 ] = scene->cmpbuf[ 2 ] + scene->video_w * y;
		
		for (int x = 0; x < picref->video->w; x++) 
		{
			int xh  = x >> scene->hsub;
			
			yc = vidrow[0][ x  ];
			uc = vidrow[1][ xh ];
			vc = vidrow[2][ xh ];

			r = yc + 1.402   * (vc - 128);
			g = yc - 0.34414 * (uc - 128) - 0.71414 * (vc - 128);
			b = yc + 1.772   * (uc - 128);
			
			if (b < 0) b = 0;
			if (g < 0) g = 0;
			if (r < 0) r = 0;
			
			if (b > 255) b = 255;
			if (g > 255) g = 255;
			if (r > 255) r = 255;
								
			if (scene->thisframe > 0)
			{
				scene->samplediff += (cmprow[0][ x ] > b) ? (cmprow[0][ x ] - b) : (b - cmprow[0][ x ]);
				scene->samplediff += (cmprow[1][ x ] > g) ? (cmprow[1][ x ] - g) : (g - cmprow[1][ x ]);
				scene->samplediff += (cmprow[2][ x ] > r) ? (cmprow[2][ x ] - r) : (r - cmprow[2][ x ]);
					
				scene->samplecount += 3;
			}
			
			cmprow[0][ x ] = b;
			cmprow[1][ x ] = g;
			cmprow[2][ x ] = r;
		}
	}
}

static void dump_scene_image(AVFilterLink *inlink,int divisor,int fcount,int fps)
{
    SceneContext *scene = inlink->dst->priv;
    
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	
	FILE *outfile;
	char *filename;
	char filenumber[ 32 ];
	char filemode[ 32 ];
	
	JSAMPROW row_pointer[ 1 ];
    int row_stride;
	unsigned char *rp,*gp,*bp;
	unsigned char *lp;
	int sampleadd = divisor - 1;
	
	int hour,min,sec,frame;
	
	frame = fcount % fps;
	fcount = fcount / fps;
	hour = fcount / 3600;
	fcount -= 3600 * hour;
	min = fcount / 60;
	fcount -= 60 * min;
	sec = fcount / 1;
	
	filename = av_malloc(2050);
	strcpy(filename,scene->file_name);
	mkpath(filename,0777);

	strcpy(filemode,"4");
	if (divisor > 1) strcpy(filemode,"0");
	
	snprintf(filenumber,32,"/%02d-%02d-%02d-%02d_%s.jpg",hour,min,sec,frame,filemode);
	av_strlcat(filename,filenumber,2048);
	
	if ((outfile = fopen(filename, "wb")) == NULL) 
	{
		return;
	}

	cinfo.err = jpeg_std_error(&jerr);	
	jpeg_create_compress(&cinfo);
	
	jpeg_stdio_dest(&cinfo,outfile);
	
	cinfo.image_width      = scene->video_w / divisor;
	cinfo.image_height     = scene->video_h / divisor;
	cinfo.input_components = 3;     
	cinfo.in_color_space   = JCS_RGB;

	jpeg_set_defaults(&cinfo);
	
	jpeg_start_compress(&cinfo, TRUE);
	
	bp = scene->cmpbuf[ 0 ];
	gp = scene->cmpbuf[ 1 ];
	rp = scene->cmpbuf[ 2 ];

    row_stride = cinfo.image_width * 3;
	row_pointer[ 0 ] = av_malloc(row_stride);
	
    while (cinfo.next_scanline < cinfo.image_height) 
	{
		lp = row_pointer[ 0 ];
		
		for (int x = 0; x < scene->video_w; x += divisor)
		{
			*lp++ = *rp++;
			*lp++ = *gp++;
			*lp++ = *bp++;
			
			if (divisor > 1)
			{
				rp += sampleadd;
				gp += sampleadd;
				bp += sampleadd;
			}
		}
		
		if (divisor > 1)
		{
			rp += sampleadd * scene->video_w;
			gp += sampleadd * scene->video_w;
			bp += sampleadd * scene->video_w;
		}

		jpeg_write_scanlines(&cinfo,row_pointer,1);
	}
	
	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);
	
	av_free(row_pointer[ 0 ]);
	
	fclose(outfile);
	
	av_free(filename);
}

static void draw_slice(AVFilterLink *inlink, int y0, int h, int slice_dir)
{
    SceneContext *scene = inlink->dst->priv;
	
	if (y0 == 0) 
	{
		if (scene->thisframe >= 0)
		{
			int dumpdat = 1;
			
			if (scene->samplecount > 0)
			{
				scene->samplediff /= scene->samplecount;
				
				if (scene->samplediff < scene->threshold) dumpdat = 0;
			}
			
			if (dumpdat)
			{
				//
				// Check stop frames.
				//
				
				if ((scene->lastscene >= 0) && 
				    ((scene->thisframe - scene->lastscene) < scene->stopframes))
				{
					dumpdat = 0;	
				}
			}
			else
			{
				//
				// Check min frames.
				//
				
				if ((scene->lastscene < 0) || 
				    ((scene->thisframe - scene->lastscene) >= scene->minframes))
				{
					dumpdat = 1;	
				}
			}
			
			if (dumpdat)
			{
				//
				// Compute frame rate from sample times.
				//
				
				int fps;
				
				int64_t ptsdiff = inlink->cur_buf->pts - scene->lastpts;
				ptsdiff = ptsdiff / (inlink->time_base.den / 1000);
				ptsdiff = ptsdiff * inlink->time_base.num;
				
				fps = 1000 / ptsdiff;
				
				av_log(inlink->dst, AV_LOG_DEBUG, "draw_slice frame=%d diff=%d fps=%d\n",
					scene->thisframe,
					scene->samplediff,
					fps
					);
					
				dump_scene_image(inlink,1,scene->thisframe,fps);
				dump_scene_image(inlink,4,scene->thisframe,fps);
				
				scene->lastscene = scene->thisframe;
			}
		}
		
		scene->thisframe  += 1;
		scene->samplecount = 0;
		scene->samplediff  = 0;
		
		scene->lastpts = inlink->cur_buf->pts;
	}
	
	/*
	av_log(inlink->dst, AV_LOG_DEBUG, "draw_slice frame=%d y0=%d h=%d slice_dir=%d\n", 
		scene->thisframe, y0, h, slice_dir
		);
	*/
	/*		   
	av_log(inlink->dst, AV_LOG_DEBUG, "vf_scene: fps:%d/%d %ld\n", 
			inlink->time_base.num,
			inlink->time_base.den,
			inlink->cur_buf->pts
			);
	*/
	
	draw_slice_scene(inlink, y0, h, slice_dir);
	
    avfilter_draw_slice(inlink->dst->outputs[0], y0, h, slice_dir);
}

AVFilter avfilter_vf_scene = {
    .name        = "scene",
    .description = NULL_IF_CONFIG_SMALL("Scene detect filter."),
    .priv_size   = sizeof(SceneContext),
	
    .init      = init,
    .uninit    = uninit,

    .query_formats   = query_formats,
    .inputs    = (AVFilterPad[]) {{ .name             = "default",
                                    .type             = AVMEDIA_TYPE_VIDEO,
                                    .config_props     = config_input_main,
                                    .draw_slice       = draw_slice,
                                    .min_perms        = AV_PERM_WRITE | AV_PERM_READ,
                                    .rej_perms        = AV_PERM_REUSE },
                                  { .name = NULL}},
    .outputs   = (AVFilterPad[]) {{ .name             = "default",
                                    .type             = AVMEDIA_TYPE_VIDEO, },
                                  { .name = NULL}},
};
