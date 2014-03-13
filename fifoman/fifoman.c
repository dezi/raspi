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

#define BUFSIZE  1024 * 1024
#define RWSIZE     64 * 1024

//
// Globals
//

char *kappa_fifo_version = "v1.0";

int   kappa_fifo_passes = 1;

int	  kappa_fifo_groupscnt = 0;
int	  kappa_fifo_groupmore = 0;
char  kappa_fifo_groupname[ MAXSLOTS ][ 64 ];
int   kappa_fifo_grouptodo[ MAXSLOTS ];

int	  kappa_fifo_inputscnt = 0;
char  kappa_fifo_inputname[ MAXSLOTS ][ MAXPATHLEN ];	
int   kappa_fifo_inputginx[ MAXSLOTS ];
int   kappa_fifo_inputfdwr[ MAXSLOTS ];
int   kappa_fifo_inputdone[ MAXSLOTS ];

int	  kappa_fifo_outputscnt = 0;
char  kappa_fifo_outputname[ MAXSLOTS ][ MAXPATHLEN ];
int   kappa_fifo_outputginx[ MAXSLOTS ];
int   kappa_fifo_outputfdrd[ MAXSLOTS ];
byte *kappa_fifo_outputbufs[ MAXSLOTS ];
int   kappa_fifo_outputread[ MAXSLOTS ];
long  kappa_fifo_outputtotl[ MAXSLOTS ];

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
	
	kappa_fifo_grouptodo[ inx ] = 0;
	
	kappa_fifo_groupscnt++;
	
	fprintf(stderr,"Created group %2d named %s ...\n",inx,indx);

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
	
	fprintf(stdout,"Looking for Kappa pipes.\n");
	
	kappa_fifo_groupmore = 0;
	
	for (grp = 0; grp < kappa_fifo_groupscnt; grp++)
	{
		kappa_fifo_grouptodo[ inx ] = 0;
	}

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
				if (! strcmp(kappa_fifo_outputname[ cnt ],entry->d_name))
				{
					dup = true;
					break;
				}
			}
			
			if (dup) continue;
						
			grp = kappa_fifo_groupindex(entry->d_name);
			
			tfd = open(entry->d_name,O_RDONLY | O_NONBLOCK);
			fprintf(stdout,"Opening output pipe %2d = %2d => %s\n",tfd,grp,entry->d_name);
			
			if (tfd < 0)
			{
				kappa_fifo_grouptodo[ grp ]++;
				kappa_fifo_groupmore++;
			}
			else
			{
				strcpy(kappa_fifo_outputname[ kappa_fifo_outputscnt ],entry->d_name);
				
				kappa_fifo_outputfdrd[ kappa_fifo_outputscnt ] = tfd;
				kappa_fifo_outputginx[ kappa_fifo_outputscnt ] = grp;
				kappa_fifo_outputbufs[ kappa_fifo_outputscnt ] = (byte *) malloc(BUFSIZE);
				kappa_fifo_outputread[ kappa_fifo_outputscnt ] = 0;
				kappa_fifo_outputtotl[ kappa_fifo_outputscnt ] = 0;

				kappa_fifo_outputscnt++;
			
				continue;
			}
		}

		if (strncmp(entry->d_name,pattern_in,strlen(pattern_in)) == 0)
		{
			for (dup = false, cnt = 0; cnt < kappa_fifo_inputscnt; cnt++)
			{
				if (! strcmp(kappa_fifo_inputname[ cnt ],entry->d_name))
				{
					dup = true;
					break;
				}
			}
			
			if (dup) continue;
			
			grp = kappa_fifo_groupindex(entry->d_name);
			
			tfd = open(entry->d_name,O_RDWR | O_NONBLOCK);
			fprintf(stdout,"Opening input  pipe %2d = %2d => %s\n",tfd,grp,entry->d_name);
			
			if (tfd < 0)
			{
				kappa_fifo_grouptodo[ grp ]++;
				kappa_fifo_groupmore++;
			}
			else
			{
				strcpy(kappa_fifo_inputname[ kappa_fifo_inputscnt ],entry->d_name);
				
				kappa_fifo_inputfdwr[ kappa_fifo_inputscnt ] = tfd;
				kappa_fifo_inputginx[ kappa_fifo_inputscnt ] = grp;
				kappa_fifo_inputdone[ kappa_fifo_inputscnt ] = 0;
							
				kappa_fifo_inputscnt++;
			
				continue;
			}
		}
	}

	closedir(dir);
}

void kappa_fifo_close_all()
{
	int inx;
	
	for (inx = 0; inx < kappa_fifo_inputscnt; inx++)
	{
		if (kappa_fifo_inputfdwr[ inx ] >= 0)
		{
			close(kappa_fifo_inputfdwr[ inx ]);
			kappa_fifo_inputfdwr[ inx ] = -1;
		}
	}

	for (inx = 0; inx < kappa_fifo_outputscnt; inx++)
	{
		if (kappa_fifo_outputfdrd[ inx ] >= 0)
		{
			close(kappa_fifo_outputfdrd[ inx ]);
			kappa_fifo_outputfdrd[ inx ] = -1;
		}
	}
}

//
// Execute a single pass until input closed.
//

int kappa_fifo_execute_pass(int pass)
{	
	//
	// Main read loop.
	//
		
	int	 inx;
	int	 cnt;
	int	 grp;
	int	 offs;
	int	 xfer;
	int	 yfer;
	int  todo;
	
	long  totalr = 0;
	long  totalw = 0;
	
	int  done = 0;
	int  modu = 0;
	
	kappa_fifo_open_all(pass);
	
	while (done < kappa_fifo_outputscnt)
	{
		if (kappa_fifo_groupmore)
		{
			kappa_fifo_open_all(pass);
		}
		
		for (inx = 0; inx < kappa_fifo_outputscnt; inx++)
		{
			grp = kappa_fifo_outputginx[ inx ];
	
			//
			// Check for undelivered buffer parts.
			//
			
			if (kappa_fifo_outputread[ inx ] > 0)
			{
				if (kappa_fifo_grouptodo[ grp ]) continue;
				
				todo = 0;
			
				for (cnt = 0; cnt < kappa_fifo_inputscnt; cnt++)
				{
					if (kappa_fifo_inputginx[ cnt ] != grp) continue;
					
					if (kappa_fifo_inputdone[ cnt ] == kappa_fifo_outputread[ inx ]) continue;
				
					offs = kappa_fifo_inputdone[ cnt ];
					xfer = kappa_fifo_outputread[ inx ] - offs;
					if (xfer > RWSIZE) xfer = RWSIZE;
					
					yfer = write(kappa_fifo_inputfdwr[ cnt ],kappa_fifo_outputbufs[ inx ] + offs,xfer);
					
					if (yfer > 0)
					{
						totalw += yfer;
						
						kappa_fifo_inputdone[ cnt ] += yfer;
					}
					else
					{
						usleep(1000);
					}
					
					todo++;
				}
				
				if (todo == 0) kappa_fifo_outputread[ inx ] = 0;
			}
			
			if (kappa_fifo_outputread[ inx ] == 0)
			{	
				for (cnt = 0; cnt < kappa_fifo_inputscnt; cnt++)
				{
					if (kappa_fifo_inputginx[ cnt ] != grp) continue;
			
					kappa_fifo_inputdone[ cnt ] = 0;
					
					if (kappa_fifo_outputfdrd[ inx ] < 0)
					{
						close(kappa_fifo_inputfdwr[ cnt ]);
						kappa_fifo_inputfdwr[ cnt ] = -1;
					}
				}
			}
			
			if (kappa_fifo_outputfdrd[ inx ] < 0) continue;

			if (kappa_fifo_outputread[ inx ] < BUFSIZE)
			{
				offs = kappa_fifo_outputread[ inx ];
				xfer = BUFSIZE - offs;
				if (xfer > RWSIZE) xfer = RWSIZE;
				
				yfer = read(kappa_fifo_outputfdrd[ inx ],kappa_fifo_outputbufs[ inx ] + offs,xfer);
				
				if (yfer > 0)
				{
					totalr += yfer;
					kappa_fifo_outputread[ inx ] += yfer;
					kappa_fifo_outputtotl[ inx ] += yfer;
				}
				else
				if (yfer == 0)
				{
					usleep(1000);
					
					if ((kappa_fifo_outputtotl[ inx ] >  0) &&
						(kappa_fifo_outputread[ inx ] == 0))
					{
						close(kappa_fifo_outputfdrd[ inx ]);
						kappa_fifo_outputfdrd[ inx ] = -1;
						
						done++;
					}
				}
			}
			
			if (! (++modu % 777))
			{
				/*
				fprintf(stdout,"Buffer %4d MB %s\n",
					kappa_fifo_outputtotl[ inx ] >> 20,
					kappa_fifo_outputname[ inx ]
					);
				*/
			}
		}
	}
	
	fprintf(stdout,"Totalr=%ld\n",totalr);
	fprintf(stdout,"Totalw=%ld\n",totalw);
	
	kappa_fifo_close_all();
}

//
// Main entry.
//

int main(int argc, char **argv)
{
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

