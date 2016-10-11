/*
 *	album.c
 *				Given a set of raw images living in a user-specified directory:
 *				- Generates a thumbnail version (10%, using 'convert')
 *				- Displays the thumbnail to the user (using 'display')
 *				- Asks the user if it should be rotated (rotates if so, using 'convert')
 *				- Collects a caption from the user
 *				- Generates a properly oriented 25% version of the photo (using 'convert')
 *
 *	inputs: 	photos to convert
 *	outputs: 	- the thumbnails/medium sized versions
 *				- index.html, containing (for each photo)
 *				- a properly oriented thumbnail
 *				- a caption
 *				- a link from the thumbnail to a properly-oriented 
 *				  medium sized version
 *
 *	assumptions: number of photos input is less than 10^95 (per thumb array len)
 *				 no captions greater than 99 characters
 *				 if connected by ssh, user must include the -Y tag
 *
 *	implementation:	the program taks advantage of concurrent processing to create
 *				a pipeline for the tasks and perform certain tasks 
 *				simultaneously. Below is the implementation. 
 *	pipeline:
 *	thumb1	disp1	query1	capt1	med1	append1
 *									thumb2	disp2	query2	capt2	med2	append2
 *																	thumb3	disp3	query3	capt3	med3	append3
 *																									4...
 *
 *	Each integer represents its own exec of album. E.g. thumb1 is the child process
 *	that generates the thumbnail in the first process, etc. The next process is executed
 *	after user input is collected and the display is closed. To implement this, when
 *	we hit the "med" stage, we execv() a new process with the same args (minus the one
 *	we operated on here).
 *
 *
 *	Note: the user may include the optional tag '-n' followed by an integer, and album
 *	will begin numbering the output files (e.g. thumb#.jpg, med#.jpg) with that number.
 *	This is intended to allow the passing of a counter through the recursion, although
 *	the user could conceivably choose to exploit this fact. This is not the intended use,
 *	however, and it is suggested that the user invoke album with its normal syntax:
 *	./album photo1.jpg photo2.jpg photo3.jpg ...
 *	If the user chooses to exploit this fact, then the index.html file will likely not
 *	be formatted correctly.
 *
 *
 *	Last note: one might argue that this does not take full advantage of concurrency. I 
 *	would argue otherwise; the limiting factors in this process are waiting for ImageMagick
 *	to finally display the photo (which takes ages), and also collecting user input. There-
 *	fore, there is no real advantage to, say, concurrently generating all of the thumbnails
 *	(and in fact this would actually lead to a slower program), because the user then still
 *	has to wait for each individual one to display and give individual input before moving
 *	forward. For that reason, the only concurrent tasks are the ones that overlap in the
 *	pipeline above.
 *
 *	limitations: can't figure out how to put the files into an output folder. Sorry.
 *		afterwards, just type:
 *		mkdir output
 *		mv -t output index.html med*.jpg thumb*.jpg
 *	
 *		In theory, it should be fairly simple to include those commands in here via a
 *		syscall, but it won't work...
 *
 */

#include <unistd.h>					//execlp, execv
#include <stdlib.h>					//exit, malloc
#include <stdio.h>					//sprintf, printf, etc.
#include <sys/wait.h>				//waitpid
#include <sys/stat.h>				//mkdir()
#include <string.h>					//strlen,etc.
#include <getopt.h>					//getopt

#define BUFLEN 100

const char *indent = "=====";

int input_string(char *message, char *buffer, int len);

int main(int argc, char *argv[])
{
	char capt[BUFLEN];		//no captions over 99 characters
 	char thumb[BUFLEN];		//prolly won't have an image name over 99 characters. hope not
 	char med[BUFLEN];
 	char promptbuf[BUFLEN];
 	char numbuf[10];		//for rotation step
 	int i,pid1,pid2,pid3,pid4,pid5,pid6,has_num = 0,rot=0;
 	char c;
 	
  	while ((c = getopt(argc, argv, "n:")) != -1) {
		if(c == 'n'){
			has_num = 1;
			i=atoi(optarg);
		}
	}
	
	if(argc == 1){
		fprintf(stderr, "please specify some parameters.\n");
		exit(1);
	} else if (argc == 3 && has_num){		//this means we're done
		FILE *infil = fopen("index.html", "a");
		fprintf(infil, "\n</body>\n\n");
		fclose(infil);
		
//		//move the index file into the output folder
// 		int pid = fork();
// 		if (0 == pid){
// 			execlp("mv", "mv", "-t", "output", "index.html", NULL);
// 			exit(-1);
// 		} else{
// 			waitpid(pid, NULL, 0);
// 		}
		
		//for some reason even this doesn't work... wtf
// 		system("mkdir output");
// 		system("mv -t output index.html med*.jpg thumb*.jpg");
		
		return(0);
	}
	
	if(!has_num)					//initial call: e.g. ./album thisphoto.jpg thatphoto.jpg
		i=0;
		
	//create a name for the thumbnail
	sprintf(thumb, "thumb%d.jpg", i);
		
	//create a new process to generate thumbnail
	pid1 = fork();
	if(0 == pid1){
		if(!has_num)
			execlp("convert", "convert", argv[1], "-resize", "10%%", thumb, NULL);
		else
			execlp("convert", "convert", argv[3], "-resize", "10%%", thumb, NULL);
  		exit(-1);
	} else{
		waitpid(pid1, NULL, 0);	//parent waits for this to finish before moving on
	}
		
	//create a new process to display the thumbnail
	pid2 = fork();
	if(0 == pid2){
		execlp("display", "display", thumb, NULL);
 		exit(-1);
	} else{
		printf("%sDisplaying photo %d (be patient.. XQuartz sucks)\n", indent, i);
		
		//ask to rotate
		sprintf(promptbuf,"Enter degrees to rotate photo %d(0,90,180,270)",i);
		input_string(promptbuf,numbuf,BUFLEN);
		rot = atoi(numbuf);	//get numerical value
		printf("You opted to rotate %d degrees.\n", rot);
		
		//ask for caption
		sprintf(promptbuf,"Enter a caption for photo %d ",i);
		input_string(promptbuf,capt,BUFLEN);
		printf("Your caption is '%s'\n", capt);
		
		
		waitpid(pid2, NULL, 0);	//parent waits for this to finish before moving on
	}
		
	//fork an additional process: ./album -n (i+1) args...
	//	could accomplish by execv("./album", paramlist), where paramlist is
	//		the same as argv with argv[1] omitted	
	
	char **params;
	
	if(!has_num){
		//create new paramlist: ex) turn ./album arg1 arg2 into ./album -n 1 arg2
		params = malloc(sizeof(char*) * (argc + 2) );
		
		sprintf(numbuf, "%d", i+1);
		
		//./album -n (i+1) argv[2:argc-1]
		params[0] = argv[0];
		params[1] = "-n";
		params[2] = numbuf;
		for(int i = 2; i<argc; i++){	//skip argv[0] bc we already assigned it
			params[i+1] = argv[i];		//skip argv[1] because we just worked on it
				
		}
		params[argc+1] = NULL;			//list must be null terminated
		
	} else if(has_num && argc == 4){ 
		//create new paramlist : ex) turn album -n 2 arg3 into album -n 3
		params = malloc(sizeof(char*) * 4);
		params[0] = argv[0];			//0 and 1 stay the same
		params[1] = argv[1];
		sprintf(numbuf, "%d", i+1);		//increment 2
		params[2] = numbuf;
		params[3] = NULL;				// list is null terminated
		
	}else{
		//create new paramlist : ex) turn ./album -n 1 arg2 arg3 into album -n 2 arg3
		params = malloc(sizeof(char*) * (argc) );
		
		params[0] = argv[0];			//first two args are the same
		params[1] = argv[1];
		sprintf(numbuf, "%d", i+1);		//increment i and set to string for arg
		params[2] = numbuf;				//change n
		
		//copy remaining arguments
		for(int i = 3; i < argc-1; i++)
			params[i] = argv[i+1];		//set other params
			
		params[argc-1] = NULL;			//list must be null terminated
	}
	
	//fork our new process with the right params!
	pid3 = fork();
	if(0 == pid3){
		execv(params[0], params);
		exit(-1);
	}	//no waitpid here, because we'd like this process to continue on its own

	//avoid invalid free
	if(!has_num)
		params[argc+1] = "";
	else
		params[argc-1] = "";
		
	//cleanup
	free(params);
		
	//so that we can send it to execlp
	sprintf(numbuf, "%d", rot);
		
	//name for the medium sized image
	sprintf(med, "med%d.jpg", i);
	
	//if the user wants to rotate it, then so be it
	if(rot){
		pid4 = fork();
		if(0 == pid4){
			execlp("convert", "convert", thumb, "-rotate", numbuf, thumb, NULL);
			exit(-1);
		} else{
			waitpid(pid4, NULL, 0);
		}
			
		//generate a properly oriented 25% version
		//convert argv[1] -resize 25% (-rotate nDegrees) med1.jpg
		pid5 = fork();
		if(0 == pid5){
			if(!has_num)
				execlp("convert", "convert", argv[1], "-resize", "25%%","-rotate",numbuf,med, NULL);
			else
				execlp("convert", "convert", argv[3], "-resize", "25%%","-rotate",numbuf,med, NULL);
			exit(-1);
		} //don't need to wait for this to finish to do other stuff
		
	//if not, don't rotate
	}else{
	
		//generate a properly oriented 25% version
		//convert argv[1] -resize 25% med1.jpg
		pid5 = fork();
		if(0 == pid5){
			if(!has_num)
				execlp("convert", "convert", argv[1], "-resize", "25%%", med, NULL);
			else
				execlp("convert", "convert", argv[3], "-resize", "25%%", med, NULL);
			exit(-1);
		} //don't need to wait for this to finish to do other stuff
	}
		
	//append index.html
	if(!has_num){
		FILE *infil = fopen("index.html", "w");
		fprintf(infil,"<html><title>a sample index.html</title> \
						\n<h1>a sample index.html</h1>\n \
						\nPlease click on a thumbnail to view a medium-size image\n \
						\n<h2> %s</h2>\n \
						\n<a href=\"%s\"><img src=\"%s\" border=\"1\"></a>\n\n",capt,med,thumb);
		fclose(infil);
	} else{
		FILE *infil = fopen("index.html", "a");
		fprintf(infil,"<h2> %s</h2>\n \
					  \n<a href=\"%s\"><img src=\"%s\" border=\"1\"></a>\n\n",
					  capt,med,thumb);
		fclose(infil);
	}
 	
 	waitpid(pid3, NULL, 0);	//wait for other pipes to finish before pooping out
 	
//  //create output directory
// 	mkdir("output", S_IRWXU);	//this will only execute once, thankfully!
// 	
// 	//move our files into that directory
// 	pid6 = fork();
// 	if(0 == pid6){
// 		execlp("mv", "mv", "-t", "output", thumb, med, NULL);
// 		exit(-1);
// 	}else{
// 		waitpid(pid6, NULL, 0);
// 	}
 	
 	return(0);
}

// prompt the user with message, and save input at buffer
// (which should have space for at least len bytes)
// NOTE: borrowed from Sean Smith's code, demo.c, available on Canvas
int input_string(char *message, char *buffer, int len) 
{

  int rc = 0, fetched, lastchar;

  if (NULL == buffer)
    return -1;

  if (message)
    printf("%s: ", message);

  // get the string.  fgets takes in at most 1 character less than
  // the second parameter, in order to leave room for the terminating null.  
  // See the man page for fgets.
  fgets(buffer, len, stdin);
  
  fetched = strlen(buffer);


  // warn the user if we may have left extra chars
  if ( (fetched + 1) >= len) {
    fprintf(stderr, "warning: might have left extra chars on input\n");
    rc = -1;
  }

  // consume a trailing newline
  if (fetched) {
    lastchar = fetched - 1;
    if ('\n' == buffer[lastchar])
      buffer[lastchar] = '\0';
  }

  return rc;
}