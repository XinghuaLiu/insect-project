#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <stdint.h>
#include <dirent.h>
#include <unistd.h>
#include <signal.h>

#define PATH "/home/ema/Downloads/cameraTeo/datapart"
#define PATH2 "/home/ema/Downloads/cameraTeo/data/line"

// The following is the command used to compile this C file, in order to make it compatible with CHowardWrap.py

//gcc -Wall -fPIC -O -g CHoward.c -c -o CHoward.pic.o && gcc -shared CHoward.pic.o -lkxclib_x86_64 -o libcstuff.so

// Set up flag for ualarm
volatile sig_atomic_t delay_flag=1;

void handle_alarm(int sig){
    delay_flag=1-delay_flag;
}

// Get a time-stamp with microsecond precision.
uint64_t get_gtod_clock_time()
{
    struct timeval tv;

    if (gettimeofday (&tv, NULL) == 0)
        return (uint64_t) (tv.tv_sec * 1000000 + tv.tv_usec);
    else
        return 0;
}

// Every time we use an XCLIB function, we check for successful execution.
void checkEpixError(char *fun, int retval)
{
// Although my old python code checked whether readushort was successful or not, I did not bother to translate that part to C, so this code simply ignores readushort.
    if(strcmp(fun,"readushort")!=0 && retval!=0)
    {
        printf("%s returned an erroneous value of %d\n",fun,retval);
        pxd_mesgFault(0x01);
    }
}

// Divide by 10 with only addition and bit-shifting.
unsigned short divten(unsigned short n)
{
    unsigned short q, r;
    q = (n >> 1) + (n >> 2);
    q = q + (q >> 4);
    q = q + (q >> 8);
    q = q + (q >> 16);
    q = q >> 3;
    r = n - (((q << 2) + q) << 1);
    return q + (r > 9);
}

// Find n mod 10 with only addition and bit-shifting.
unsigned short modten(unsigned short n)
{
    unsigned short x = divten(n);
    return n-x-x-x-x-x-x-x-x-x-x;
}

// Turn a short into a string.
// This function is not actually used. It is only here for comparison purposes.
void shortToStr_(unsigned short s, char * ret)
{
    sprintf(ret,"%hu",s);
}

// Turn a short into a string.
// This function should be equivalent to the previous function.
// Last I checked, it has a shorter run-time. This is the most frequently called function in the entire program; this is why I attempted to make it more efficient.
void shortToStr(unsigned short s, char * ret)
{   
    char n1, n2, n3;
    if(s>100)
    {   
        n3=modten(s)+'0';
        s=divten(s);
        n2=modten(s)+'0';
        s=divten(s);
        n1=s+'0';
        char res[4]={n1,n2,n3,'\0'};
        memcpy(ret,res,4);
        return;
    }
    if(s>10)
    {   
        n2=modten(s)+'0';
        s=divten(s);
        n1=s+'0';
        char res[3]={n1,n2,'\0'};
        memcpy(ret,res,3);
        return;
    }
    n1=s+'0';
    char res[2]={n1,'\0'};
    memcpy(ret,res,2);
    return;
}

// Turn an array of shorts into a string, with commas in-between each short.
int shortsToStr(unsigned short * s, int len, char * ret)
{
    int ctr=0;
    for(int i=0; i<len; i++){
        ret[ctr++]=',';
        char tmp[4];
        shortToStr(s[i],tmp);
        for(int j=0; j<4; j++){
            ret[ctr++]=tmp[j];
            if(tmp[j+1]=='\0')
            {
                break;
            }
        }
    }
    ret[ctr]='\0';
    return ctr;
}

// Save the image current in the camera's memory to a file.
void savePixelData(int bn, char *fn){
    checkEpixError("saveRawBuffers",pxd_saveRawBuffers(0x1,fn,bn,bn,0,0,0,0));
}

// Go through all the datapart* raw image data files, load them into the camera's memory, split them apart by line, and save them to the line*.csv files.
void printPixelData(const int xdim, const int nLines, const int nins, char outs[nLines][sizeof(char)*12], char ins[nins][sizeof(char)*28])
{

// Create handles to the line*.csv files.
// Perhaps things may be faster if the buffer-size is increased.
// The process of writing to disk is fast enough though.
    FILE *fouts[nLines];
    for(int i=0; i<nLines; i++){
        char temp[1000];
        sprintf(temp,"%s%s","./data/",*outs++);
        fouts[i]=fopen(temp,"w");
    }

// Define a buffer into which to read pixel data from the camera's registers.
    const int bufsize=xdim*nLines;
    unsigned short buf[bufsize];

// Process each datapart* raw image data file.
    for(int h=0; h<nins; h++){

// Clear the buffer
        memset(buf,0,sizeof(buf));

// Get the time-stamp out of the file name, and reassemble the file-name.
// The "reassembly" seems to be entire unnecessary.
        char * in = ins[h];
        char * ts = in+8;
        char pth[100];
        strcpy(pth,PATH);
        strcat(pth,ts);

// Load the raw image data files into the camera
        pxd_loadRawBuffers(0x1,pth,1,1,0,0,0,0);

// Dump the numerical pixel values into the buffer
        pxd_readushort(0x1,1,0,0,-1,nLines,buf,bufsize,"Gray");

// Process the numerical pixel values
        for(int i=0; i<nLines; i++){
// Write the time-stamp into each line*.csv file
            fwrite(ts,strlen(ts),1,fouts[i]);
// Get the appropriate line from the buffer
            char * tmp = malloc(sizeof(char)*20*xdim);
            shortsToStr(&buf[i*xdim],xdim,tmp);
// Write it to the appropriate line*.csv file
            fwrite(tmp,strlen(tmp),1,fouts[i]);
            fwrite("\n",strlen("\n"),1,fouts[i]);
// Prevent memory leaks
            free(tmp);
        }

    }
// Flush and close all line*.csv files
    for(int i=0; i<nLines; i++){
        fflush(fouts[i]);
        fclose(fouts[i]);
    }
    return;
}

// Shorthand, due to the length of the function
void _setResAndTime(int aoitop, int aoiwidth, int aoiheight, int scandirection, double clock, double frame)
{
    checkEpixError("setResolutionAndTiming",pxd_SILICONVIDEO_setResolutionAndTiming(0x1,0,0x0101,0,aoitop,aoiwidth,aoiheight,scandirection,10,0,0,clock,frame,0.0,0.0,0.0));
}

// Sets resolution and timing to certain settings
void setResAndTim(int aoitop, int aoiwidth, int aoiheight, double delay)
{
    int scandirection=19540;
    aoiwidth=pxd_SILICONVIDEO_getMinMaxAoiWidth(0x01,aoiwidth);
    aoiheight=pxd_SILICONVIDEO_getMinMaxAoiHeight(0x01,aoiheight);
    _setResAndTime(aoitop,aoiwidth,aoiheight,scandirection,70.0,delay);
}

// Handle the conversion of image data from raw data to .csv files
void handleEnd(int aoiwidth, int aoiheight)
{
// Turn the camera's live function off
// IMPORTANT: take this out if the program is modified to use "doSnap" instead of "goLive"
    checkEpixError("goUnLive",pxd_goUnLive(0x1));

    printf("\nCompiling data to the proper files\n");

// Figure out how many lines there are. Create an array of filenames for each line.
    char outs[aoiheight][sizeof(char)*12];
    for(int i=0; i<aoiheight; i++){
        char tmp[sizeof(char)*12];
        sprintf(tmp,"line%d.csv",i);
        strcpy(outs[i],tmp);
    }


// This next section of code can be made neater.
// It uses both readdir and scandir to scan a folder for files.
// It can get everything done in one single scan.
// But this works well enough, and there are more important things to fix.

// Figure out how many raw data files we have by scanning through the folder, looking for datapart* files
// Note: if the previous instance of the program crashed or was prevented for completing in some way, there will be left-over datapart* files, and the program will make no distinction.
    DIR *dir=opendir(".");
    struct dirent *de;
// Artificially reduce count by 2, because we will discard the very first two frames. They may sometimes not be valid data.
    int ctr=-2;

    while ((de=readdir(dir))!=NULL){
        char * tmp = de->d_name;
        if(strncmp(tmp,"datapart",strlen("datapart"))==0){
            ctr++;
        }
    }
    int nins=ctr;

// Make an array of the filenames of all the datapart* files.
    struct dirent **de2;
    ctr=-2;
    char ins[nins][sizeof(char)*28];

// Scan the files alphabetically and store their names into an array.
    int n=scandir(".",&de2,NULL,alphasort);
    int i=0;
    while(i<n){
        char tmp[sizeof(char)*28];
        strcpy(tmp,de2[i]->d_name);
        if(strncmp(tmp,"datapart",strlen("datapart"))==0){
            if(ctr==-1){
                ctr++;
            } else {
                strcpy(ins[ctr++],tmp);
            }
        }
        free(de2[i]);
        i++;
    }
    free(de2);

// Move the rest of the work onto the next function.
    printPixelData(aoiwidth,aoiheight,nins,outs,ins);

// Close the camera.
    checkEpixError("PIXCIclose",pxd_PIXCIclose("","NSTC",""));
}


// Do the main work of actually taking the images.
void work(int nLines, int nImages, double delay, int linePos)
{
// Give this process the highest CPU priority
// Note that this seems to require sudo privileges to function.
    nice(-50);

    int aoiwidth=640;
    int aoiheight=nLines;
// Open the camera and wait a bit.
    checkEpixError("PIXCIopen",pxd_PIXCIopen("","",""));
    usleep(100);

// Set resolution and timing to the desired values.
    setResAndTim(linePos,aoiwidth,aoiheight,delay);
// Reobtain the resolution, just in case it's changed?
    aoiwidth=pxd_imageXdim();
    aoiheight=pxd_imageYdim();

// Hook SIGALRM to our handle_alarm.
    signal(SIGALRM,handle_alarm);

// Continuous live capture.
// Discrete capture through the use of doSnap allows for far slower frequencies (under 100Hz), but is significantly slower.
// If frequencies that slow are required, a workaround can be easily found.
    checkEpixError("goLive",pxd_goLive(0x1,1));

    printf("Starting\n");
    int ctr=0;

// Set the initial alarm.
    ualarm((useconds_t)(int)(1000*delay),0);
    while(ctr<nImages){
        ctr+=1;

// Wait for the alarm to trigger.
        while(delay_flag){}
// Immediately reset the alarm. It seems the alarm introduces a delay of about 2 microseconds.
// Taking video at 1000 Hz, with a 2us drift for every frame taken, will result in 1 out of every 500 frames being missed.
// Attempting to correct for it, as I have below, may reduce this problem. However, overcorrecting will unpredictably result in duplicate frames.
// Furthermore, duplicate frames may behave in unexpected and unpredictable fashion. 

// I leave both versions of the next line. Commented out is the version that does not attempt to correct the innate delay, and results in 1 out of every 500 or 1000 frames being missed.
//        ualarm((useconds_t)(int)(1000*delay),0);

//Uncommented out is the version that does attempt to correct inate delay, and might or might not result in faulty data. I personally believe that if faulty data does arise, it will be primarily concentrated in the first few columns, which may or may not be discardable.
        ualarm((useconds_t)(int)(1000*delay-2),0);

// Unset the delay_flag.
        handle_alarm(0);

// Grab the timestamp for each frame, and dump the raw image data to a file.
        char fname[strlen(PATH)+17];
        strcpy(fname,PATH);
        char ts[17];
        sprintf(ts,"%lu",get_gtod_clock_time());
        strcat(fname,ts);
        savePixelData(1,fname);
    }

// Once enough images have been taken, make the data usable by humans.
    handleEnd(aoiwidth,aoiheight);
}

int main()
{
    return 0;
}
