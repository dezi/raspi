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

#include "libavutil/common.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "libswscale/swscale.h"
#include "libavformat/avformat.h"

//
// Defines
//

typedef unsigned char byte;

#define MAXSLOTS 32

#define BUFSIZE  4 * 1024 * 1024
#define RDWRSIZE       64 * 1024

//
// Pipe info structure.
//

struct kafifo
{
	//
	// Video related.
	//
	
	int		isvideo;
	int		isyuv4mpeg;
	int		isheader;
	
	int		width;
	int		height;
	int		format;
	int		pixfmt;
	
	int		aspect_num;
	int 	aspect_den;
	
	int		fps_num;
	int 	fps_den;
	
	char	mode;
	
	//
	// Audio related.
	//
	
	int		isaudio;
	
	int		channels;
	int		rate;

	//
	// Pipe related.
	//
	
	char    name[ MAXPATHLEN ];
	int		framesize;
	int		chunksize;
	int		iobytes;
	long	total;
	int 	group;
	int		fd;
	
	int			bufsiz;
	byte	   *buffer;
	pthread_t	thread;
};

typedef struct kafifo kafifo_t;

//
// Globals
//

char	   *kappa_fifo_version = "1.0.0";

int   		kappa_fifo_passes = 1;

int	  		kappa_fifo_groupscnt = 0;
int	  		kappa_fifo_groupmore = 0;
char  		kappa_fifo_groupname[ MAXSLOTS ][ 64 ];
int   		kappa_fifo_grouptodo[ MAXSLOTS ];

int	  		kappa_fifo_inputscnt = 0;
int	  		kappa_fifo_inputdone = 0;
kafifo_t 	kappa_fifo_inputinfo[ MAXSLOTS ];

int	  		kappa_fifo_outputscnt = 0;
int	  		kappa_fifo_outputdone = 0;
kafifo_t 	kappa_fifo_outputinfo[ MAXSLOTS ];

//
// Usage print.
//

void kappa_fifo_usage()
{
	fprintf(stdout,"Kappa TSM Version %s\n\n",kappa_fifo_version);
	fprintf(stdout,"--passes 1|2\n");
	
	exit(1);
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
	int   headlen;
	
	*headend = '\0';
	
	headlen = strlen(info->buffer) + 1;
	fprintf(stderr,"Header  output %s %d %s\n",group,headlen,info->buffer);
	
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
		
		if (info->format == 420) 
		{
			info->pixfmt    = AV_PIX_FMT_YUV420P;
			info->framesize = info->width * info->height * 3 / 2;
		}
		
		if (info->format == 422) 
		{
			info->pixfmt    = AV_PIX_FMT_YUV422P;
			info->framesize = info->width * info->height * 2;
		}
		
		if (info->format == 444) 
		{
			info->pixfmt    = AV_PIX_FMT_YUV422P;
			info->framesize = info->width * info->height * 3;
		}
	}
	
	info->chunksize = info->framesize + strlen("FRAME\n");
	
	fprintf(stderr,"Header  output %s %d %d %d F%d:%d A%d:%d I%c %s framesize=%d\n",
			group,info->width,info->height,info->format,
			info->fps_num,info->fps_den,
			info->aspect_num,info->aspect_den,
			info->mode,av_get_pix_fmt_name(info->pixfmt),
			info->framesize);
	
	//
	// If current buffer size is too small for frame,
	// reallocate buffer.
	//
	
	if (info->bufsiz < info->chunksize)
	{
		byte *newbuf = (byte *) malloc(info->chunksize);
		
		memcpy(newbuf,info->buffer,info->bufsiz);
		
		free(info->buffer);
		
		info->buffer = newbuf;
		info->bufsiz = info->chunksize;
	}
	
	//
	// Remove header from buffer.
	//
	
	memcpy(info->buffer,info->buffer + headlen,info->bufsiz - headlen);
	info->iobytes -= headlen;
	
	info->isheader = true;
}

//
// Create input YUV4MPEG header and write.
//

void kappa_fifo_write_yuv4mpeg(char *group,kafifo_t *output,kafifo_t *input)
{
	input->width		= output->width;
	input->height		= output->height;
	input->format		= output->format;
	input->pixfmt		= output->pixfmt;
	
	input->aspect_num	= output->aspect_num;
	input->aspect_den	= output->aspect_den;
	
	input->fps_num		= output->fps_num;
	input->fps_den		= output->fps_den;
	
	input->mode			= output->mode;
	
	char yheader[ 256 ];
	
	snprintf(yheader,sizeof(yheader),
			 "YUV4MPEG2 W%d H%d F%d:%d I%c A%d:%d C%03dmpeg2 XYSCSS=%03dMPEG2\n",
			 input->width,input->height,
			 input->fps_num,input->fps_den,
			 input->mode,
			 input->aspect_num,input->aspect_den,
			 input->format,input->format
			);
	
	int xfer = write(input->fd,yheader,strlen(yheader));
	
	fprintf(stderr,"Header  input  %s %d %s",group,xfer,yheader);
		
	input->isheader = true;
}

//
// Thread main loop.
//

void *kappa_fifo_threadreader(void *data)
{
	int grp = (int) data;
		
	char *name = kappa_fifo_groupname[ grp ];

	fprintf(stderr,"Started thread %s reader\n",name);
	
	//
	// Identify output pipe.
	//
	
	int inx = -1;
	int cnt;

	while (true)
	{
		for (cnt = 0; cnt < kappa_fifo_outputscnt; cnt++)
		{
			if (grp == kappa_fifo_outputinfo[ cnt ].group)
			{
				inx = cnt;
				
				break;
			}
		}
		
		if (inx >= 0) break;
		
		usleep(100000);
	}
	
	fprintf(stderr,"Running thread %s reader\n",kappa_fifo_groupname[ grp ]);

	//
	// Derive basic info from pipe names.
	//
	
	kafifo_t *output = &kappa_fifo_outputinfo[ inx ];

	if (rindex(output->name,'.') && ! strcmp(rindex(output->name,'.'),".y4m"))
	{
		output->isvideo    = true;
		output->isyuv4mpeg = true;
	}	
	
	kafifo_t *input;
	
	int ends;
	int offs;
	int xfer;
	int yfer;
	int wait;
	int done;
		
	while (true)
	{
		wait = true;
		
		//
		// Write as much as possible.
		//
		
		for (cnt = 0; cnt < kappa_fifo_inputscnt; cnt++)
		{
			input = &kappa_fifo_inputinfo[ cnt ];
		
			if (input->group != grp) continue;

			if (output->isyuv4mpeg && ! input->isheader) 
			{
				if (! output->isheader) continue;
			
				kappa_fifo_write_yuv4mpeg(name,output,input);
			}
			
			if (input->iobytes == output->iobytes) continue;
			
			offs = input->iobytes;
			
			if (offs < output->iobytes)
			{
				xfer = output->iobytes - offs;
			}
			else
			{
				xfer = output->bufsiz - offs;
			}
			
			if (xfer > RDWRSIZE) xfer = RDWRSIZE;

			yfer = write(input->fd,output->buffer + offs,xfer);
			
			if ((grp == 999) && (yfer > 0)) fprintf(stderr,"Send %s %7d %6d\n",name,offs,yfer);

			if (yfer > 0)
			{
				input->iobytes += yfer;
				input->total   += yfer;
				
				wait = false;
			}
		}
		
		//
		// Read as much as possible.
		//
		
		ends = output->bufsiz;

		if (kappa_fifo_grouptodo[ grp ] != 0)
		{
			//
			// Some inputs are not yet ready. Read
			// until buffer filled completely and wait.
			//
		}
		else
		{
			done = true;
			
			if (output->iobytes == output->bufsiz)
			{
				//
				// Wrap buffer now.
				//
			
				output->iobytes = 0;
			}

			for (cnt = 0; cnt < kappa_fifo_inputscnt; cnt++)
			{
				input = &kappa_fifo_inputinfo[ cnt ];
				
				if (input->group != grp) continue;
				
				if (input->fd < 0) continue;
				
				if (input->iobytes == output->bufsiz)
				{
					//
					// Input has written until end of buffer,
					// wrap it now.
					//
				
					input->iobytes = 0;
				}
			
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
				
				if ((output->iobytes == input->iobytes) && (output->fd < 0))
				{
					//
					// Output pipe is closed, input pipe has all written,
					// close this now.
					//
					
					close(input->fd);
					input->fd = -1;
					
					continue;
				}
				
				done = false;
			}
			
			if (done)
			{
				//
				// Read and write pipes are closed,
				// break main loop now.
				//
				 
				break;
			}
		}

		if ((output->fd >= 0) && (output->iobytes < ends))
		{
			offs = output->iobytes;
			xfer = ends - offs;
			
			if (xfer > RDWRSIZE) xfer = RDWRSIZE;
			
			yfer = read(output->fd,output->buffer + offs,xfer);
			
			if ((grp == 999) && (yfer > 0))  fprintf(stderr,"Read %s %7d %6d\n",name,offs,yfer);
			
			if (yfer > 0)
			{
				if (output->isyuv4mpeg && ! output->isheader) 
				{
					kappa_fifo_parse_yuv4mpeg(name,output);
				}
				
				output->iobytes += yfer;
				output->total   += yfer;
				
				wait = false;
			}
			else
			if (yfer == 0)
			{
				if (output->total > 0)
				{
					//
					// We had already input, means pipe
					// is at end, so close it.
					//
					
					close(output->fd);
					output->fd = -1;
				}
			}
		}
						
		if (wait) usleep(1000);
	}
	
	fprintf(stderr,"Closing thread %s reader\n",name);
						
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
	
	char pattern_in [ 64 ];
	char pattern_out[ 64 ];
	
	snprintf(pattern_in ,sizeof(pattern_in ),"Kappa.inp.%d" ,pass);
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
	
	DIR *dir = opendir(".");
	struct dirent *entry;
	
	while (true)
	{
		entry = readdir(dir);
		if (! entry) break;
		
		if (strncmp(entry->d_name,pattern_out,strlen(pattern_out)) == 0)
		{
			for (dup = false, cnt = 0; cnt < kappa_fifo_outputscnt; cnt++)
			{
				if (! strcmp(kappa_fifo_outputinfo[ cnt ].name,entry->d_name))
				{
					dup = true;
					break;
				}
			}
			
			if (dup) continue;
						
			grp = kappa_fifo_groupindex(entry->d_name);
			
			tfd = open(entry->d_name,O_RDONLY | O_NONBLOCK);
			
			fprintf(stdout,"Opening output %s = %2d => %s\n",
					kappa_fifo_groupname[ grp ],tfd,entry->d_name);
			
			if (tfd < 0)
			{
				temptodo[ grp ]++;
				tempmore++;
			}
			else
			{
				memset(&kappa_fifo_outputinfo[ kappa_fifo_outputscnt ],0,sizeof(kafifo_t));
				strcpy( kappa_fifo_outputinfo[ kappa_fifo_outputscnt ].name,entry->d_name);
				
				kappa_fifo_outputinfo[ kappa_fifo_outputscnt ].fd     = tfd;
				kappa_fifo_outputinfo[ kappa_fifo_outputscnt ].group  = grp;
				kappa_fifo_outputinfo[ kappa_fifo_outputscnt ].bufsiz = BUFSIZE;
				kappa_fifo_outputinfo[ kappa_fifo_outputscnt ].buffer = (byte *) malloc(BUFSIZE);
	
				pthread_create(
					&kappa_fifo_outputinfo[ kappa_fifo_outputscnt ].thread,
					NULL,
					kappa_fifo_threadreader,
					(void *) grp
					);

				kappa_fifo_outputscnt++;

				continue;
			}
		}

		if (strncmp(entry->d_name,pattern_in,strlen(pattern_in)) == 0)
		{
			for (dup = false, cnt = 0; cnt < kappa_fifo_inputscnt; cnt++)
			{
				if (! strcmp(kappa_fifo_outputinfo[ cnt ].name,entry->d_name))
				{
					dup = true;
					break;
				}
			}
			
			if (dup) continue;
			
			grp = kappa_fifo_groupindex(entry->d_name);
			
			tfd = open(entry->d_name,O_RDWR | O_NONBLOCK);
			
			fprintf(stdout,"Opening input  %s = %2d => %s\n",
					kappa_fifo_groupname[ grp ],tfd,entry->d_name);
			
			if (tfd < 0)
			{
				temptodo[ grp ]++;
				tempmore++;
			}
			else
			{
				memset(&kappa_fifo_inputinfo[ kappa_fifo_inputscnt ],0,sizeof(kafifo_t));
				strcpy( kappa_fifo_inputinfo[ kappa_fifo_inputscnt ].name,entry->d_name);

				kappa_fifo_inputinfo[ kappa_fifo_inputscnt ].fd    = tfd;
				kappa_fifo_inputinfo[ kappa_fifo_inputscnt ].group = grp;
							
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
			
	fprintf(stdout,"Looking for Kappa pipes on pass %d.\n",pass);

	while (! kappa_fifo_outputscnt)
	{
		kappa_fifo_open_all(pass);
	}
	
	//
	// Working and open loop.
	//

	while (kappa_fifo_outputdone < kappa_fifo_outputscnt)
	{
		if (kappa_fifo_groupmore)
		{
			kappa_fifo_open_all(pass);
		}
		
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
	fprintf(stdout,"Kappa Fifo Manager %s.\n",kappa_fifo_version);

	if (argc == 0) kappa_fifo_usage();
	
	int inx;
	
	for (inx = 1; inx + 1 < argc; inx++)
	{
		if (! strcmp(argv[ inx ],"--passes"))
		{
			kappa_fifo_passes = atoi(argv[ inx + 1 ]);
		}
	}
	
	//
	// Execute number of passes.
	//
	
	int res = 0;
	
	for (inx = 0; inx < kappa_fifo_passes; inx++)
	{
		res = kappa_fifo_execute_pass(inx + 1);
		
		if (res) break;
	}

	return res;
}

