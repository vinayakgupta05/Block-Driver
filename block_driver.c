////////////////////////////////////////////////////////////////////////////////
//
//  File           : block_driver.c
////  Description    : This is the implementation of the standardized IO functions
//                   for used to access the BLOCK storage system.
//
//  Author         : Vinayak Gupta
//

// Includes
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
// Project Includes
#include <block_controller.h>
#include <block_driver.h>
#include <cmpsc311_log.h>
#include <cmpsc311_util.h>


typedef struct { //file structure design , cache to keep information for file handle
	char filepath[BLOCK_MAX_PATH_LENGTH];
	int16_t fhandle;
	int32_t filesize; //total file size
	int32_t  position; //byte position in total file
	int filestatus; //1 if open, 0 if closed
	int32_t no_of_frame; //total no of frames used
	BlockFrameIndex currentFrame;//currentFrame as per FrameList
	int currentframeno; //position in usedFrame array
	int currentframePosition; //byte position in currentframe
	BlockFrameIndex usedFrame[BLOCK_BLOCK_SIZE]; //array of framenos used for this file
} filestructure; 

typedef  struct {  // Index of available frames in a block
	BlockFrameIndex Frameno; //frame nos for all frames
	int status; //1 if used, 0 if not used
} FrameStructure; 

//structure for file system, cache to keep information about all files
struct filesystem{ 
	int sysstatus; // system status 0 for off & 1 for on
	filestructure Filelist[BLOCK_MAX_TOTAL_FILES]; //total file list
	FrameStructure Framelist[BLOCK_BLOCK_SIZE]; //total framelist
	int NextFileNo; //NextFileNo to be allotted
	BlockFrameIndex NextFrameNo; //Next Empty FrameNo to be allotted
}filesystem;  
//
// Presently, all frames in the block are used as data blocks, 
//actually starting one block can be used to keep information for file system.
// next few block are to be used for keeping frame information for each file, I will implment later. 


//
// Implementation
////////////////////////////////////////////////////////////////////////////////
//
// Function     : create_opcode
// Description  : create opccoe function to create 64 bit opcode
//
// Inputs       : KY1, FM1, CS1, RT1
// Outputs      : regstate
BlockXferRegister create_opcode(BlockXferRegister KY1, BlockXferRegister FM1, BlockXferRegister CS1, BlockXferRegister RT1)
{
	BlockXferRegister regstate = 0x00, KY1temp, FM1temp, CS1temp, RT1temp;
	KY1temp = (KY1) <<56; //8 bits opcode
	FM1temp = (FM1) <<40; //16 bits frame number
	CS1temp = (CS1) <<8;  //32 bits checksum
	RT1temp = (RT1) <<0;  //8 bits return code
	// prepare regstate 
	regstate = regstate | KY1temp;
	regstate = regstate | FM1temp;
	regstate = regstate | CS1temp;
	regstate = regstate | RT1temp;

	return (regstate);
}

//
// Implementation
////////////////////////////////////////////////////////////////////////////////
//
// Function     : get_RTcode            
// Description  : extract RT code from opcode
//
// Inputs       :  regstate(opcode)
// Outputs      : Only RT code ( 8 bits)
int get_RTcode(BlockXferRegister regstate){
	BlockXferRegister _RT1;
	_RT1 = 0x00000000000000FF; //mask for RT1
	regstate = regstate & _RT1;
	regstate = regstate >> 00;
	return (regstate);
}
//
// Implementation
////////////////////////////////////////////////////////////////////////////////
//
// Function     : get_CScode  
// Description  : extract CS from opcode
//
// Inputs       : regstate(opcode) 
// Outputs      : Only CS code( 32 bits)
int get_CScode(BlockXferRegister regstate){
        BlockXferRegister _CS1;
        _CS1 = 0x000000FFFFFFFF00; //mask for CS1
        regstate = regstate & _CS1;
		regstate = regstate >> 8;
        return (regstate);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : compute_frame_checksum
// Description  : Compute checksum - provided function 
//
// Inputs       : frame & 32 bit CS/ checksum
//                
// Outputs      : -1 if successful, 0 if failure
int computeframechecksum(void* frame, uint32_t* checksum){
	uint32_t sigsz = sizeof(uint32_t);
	if (generate_md5_signature(frame, BLOCK_FRAME_SIZE, (char*)checksum, &sigsz)){
		return -1;
	}
	return 0;
}

//
// Implementation
////////////////////////////////////////////////////////////////////////////////
//
// Function     : block_poweron               // Filesystkem.currentFrame =0; 
// Description  : Startup up the BLOCK interface, initialize filesystem
//
// Inputs       : non
// Outputs      : 0 if successful, -1 if failure

int32_t block_poweron(void){
	BlockXferRegister RT1 ;
	BlockXferRegister regstate;
	int i ;
	regstate = create_opcode(BLOCK_OP_INITMS, 0 , 0 , 0); //initialize block
	regstate = block_io_bus(regstate, NULL); //block_io_bus call
	RT1 = get_RTcode(regstate);
	if (RT1 == -1){
		logMessage(LOG_ERROR_LEVEL, " Failed to initialize Block driver"); 
		return -1;
	}
	else 	{
		filesystem.sysstatus=1; //change system status
	}
	for (i = 0; i<=BLOCK_MAX_TOTAL_FILES; i++){
		filesystem.Filelist[i].filepath[0] = 0x0; // set file path as null
		filesystem.Filelist[i].fhandle = 0; // set file handle
		filesystem.Filelist[i].filestatus = 0; // set file status
	} filesystem.NextFileNo = 0;
	for (i = 0; i<=BLOCK_FRAME_SIZE; i++){
		filesystem.Framelist[i].Frameno = 0; //set file status
		filesystem.Framelist[i].status = 0;
		} filesystem.NextFrameNo = 0;
   // Return successfully
    return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : block_poweroff
// Description  : Shut down the BLOCK interface, close all files
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int32_t block_poweroff(void)
{
	BlockXferRegister RT1 ;
	BlockXferRegister regstate;
	if (filesystem.sysstatus == 0){
		logMessage(LOG_ERROR_LEVEL,"Block driver already off");
		return -1;}
	regstate = create_opcode(BLOCK_OP_POWOFF, 0 , 0 , 0);
	regstate= block_io_bus(regstate, NULL);
	RT1 = get_RTcode(regstate);
	if (RT1 == -1){
		logMessage(LOG_ERROR_LEVEL, " Failed to PowerOFF Block Driver");
		return -1;
	}
	else {
		filesystem.sysstatus = 0;      //1 as started
	}
    // Return successfully
    return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : block_open
// Description  : This function opens the file and returns a file handle
//
// Inputs       : path - filename of the file to open
// Outputs      : file handle if successful, -1 if failure

int16_t block_open(char* path)
{
	int pathlen = strlen(path);
	int i;
	if (filesystem.sysstatus==0){
		logMessage(LOG_ERROR_LEVEL, "Failed, System status power off");
		return -1;}
	for (i = 0; i<=  BLOCK_MAX_TOTAL_FILES; i++){
		 if (strcmp(filesystem.Filelist[i].filepath, path)==0){
			if  (filesystem.Filelist[i].filestatus == 1){
				logMessage(LOG_ERROR_LEVEL, " Failed to open file: file is already open \n");
				return (-1);}
			else {
				filesystem.Filelist[i].filestatus = 1;
				filesystem.Filelist[i].currentFrame = filesystem.Filelist[i].usedFrame[0];
				filesystem.Filelist[i].position = 0;
				filesystem.Filelist[i].currentframePosition=0;
				logMessage(LOG_INFO_LEVEL,"%s file already exists as %s Reopening now with handle %d \n",path,filesystem.Filelist[i].filepath,filesystem.Filelist[i].fhandle);
				return (filesystem.Filelist[i].fhandle);
			}
			}
			}
	if (filesystem.NextFileNo < BLOCK_MAX_TOTAL_FILES){
		i = filesystem.NextFileNo++;
		strncpy(filesystem.Filelist[i].filepath, path, pathlen);
		filesystem.Filelist[i].filestatus =  1;
		filesystem.Filelist[i].filesize =  0;
		filesystem.Filelist[i].position = 0;
		filesystem.Filelist[i].fhandle = i;
		logMessage(LOG_INFO_LEVEL, "%s file opened %d \n",path,filesystem.Filelist[i].fhandle);
	}
	else {
		return -1; //error adding file > total_files
	}
	filesystem.Filelist[i].no_of_frame = 0;
	if (addNewFrame(i)!=0) { //error adding frame > total_frames
		return -1;
	}
	return filesystem.Filelist[i].fhandle;
    // THIS SHOULD RETURN A FILE HANDLE
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : block_close
// Description  : This function closes the file
//
// Inputs       : fd - the file descriptor
// Outputs      : 0 if successful, -1 if failure

int16_t block_close(int16_t fd)
{
	 if (checkFileHandle(fd)){
		logMessage(LOG_ERROR_LEVEL, " Failed to close file");
		return -1;}
	filesystem.Filelist[fd].filestatus =  0;
    // Return successfully
    return (0);
}
////////////////////////////////////////////////////////////////////////////////
// Function     : checkFileSize
// Description  : Reads "count" bytes from the file handle "fd" 
//
// Inputs       : fd - filename of the file to read from
//                count - number of bytes to read
// Outputs      : bytes read if successful, -1 if failure
int32_t checkFileSize(int16_t fd,int32_t count)
{
	if (filesystem.Filelist[fd].position + count > filesystem.Filelist[fd].filesize) {
		count = filesystem.Filelist[fd].filesize - filesystem.Filelist[fd].position;
		logMessage(LOG_INFO_LEVEL,"trying to read beyond file size. will read till end of file %d \n",count);
		}
	return count;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : readCurrentFrame
// Description  : Reads "count" bytes from the file handle "fd" into the
//                buffer "buf"
//
// Inputs       : fd - filename of the file to read from
//                buf - pointer to buffer to read into
//                count - number of bytes to read
// Outputs      : bytes read if successful, -1 if failure
int32_t readCurrentFrame(int16_t fd, void* buf, int32_t count)
{
	uint32_t newCScode,CScode;
	BlockXferRegister regstate, RT ;
	int success;
	success = 1;
	while (success >= 1 && success<=1000){
		if (success>1);
		regstate = create_opcode(BLOCK_OP_RDFRME, filesystem.Filelist[fd].currentFrame,0 , 0);
		regstate = block_io_bus(regstate, buf);
		RT = get_RTcode(regstate);
		CScode = get_CScode(regstate);
		logMessage(LOG_INFO_LEVEL,"read_recd_Checksum %0x %d \n", CScode,CScode);
		if (computeframechecksum(buf, &newCScode) < 0){
			return -1; // this returns ( 0 or -1) (it will not match CS code)
		}
	//	logMessage(LOG_INFO_LEVEL,"read_MD5_Checksum %0x %d %s \n", newCScode,newCScode,filesystem.Filelist[fd].currentFrame);
		if (CScode != newCScode){
			success++;
		}
		else {
			success = 0;
			if (RT==-1){
				return -1;
			}
		}
	}

    return (0);
}
////////////////////////////////////////////////////////////////////////////////
//
// Function     : checkFileHandle
// Description  : check for valid file handle "fd" & files status
//
// Inputs       : fd - filehandle of the file to read from
//
// Outputs      : bytes read if successful, -1 if failure
int16_t checkFileHandle(int16_t fd)
{	
	if (filesystem.sysstatus == 0){
		logMessage(LOG_ERROR_LEVEL, "File system powered off: task failed");
		return -1;}
	if ( fd>=  BLOCK_MAX_TOTAL_FILES || fd  < 0){
		logMessage(LOG_ERROR_LEVEL, "Invalid File Handle");
		return -1;}
	if (filesystem.Filelist[fd].filestatus  == 0){
		logMessage(LOG_ERROR_LEVEL, " File is closed.");
		return -1;}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////
//
// Function     : checkSize
// Description  : check for if increase in file size is needed
//
// Inputs       : fd - filename of the file to read from
//                count - number of bytes to read
// Outputs      : return 1 if increase size of file, ekse return 0
/*int32_t checkSize(int16_t fd,int32_t count)
{
	if (filesystem.Filelist[fd].position + count > filesystem.Filelist[fd].filesize)
		{
		return 1;
		}
	return 0;
}*/ //not used
////////////////////////////////////////////////////////////////////////////////
//
// Function     : checkCurrentFrame
// Description  : check if currentframe has space left in frame
//		 "count" bytes
// Inputs       : fd - filename of the file to read from
//                count - number of bytes to read
// Outputs      : return remain frame size in count otherwise if full new frane

int32_t checkCurrentFrame(int16_t fd,int32_t count)
{
	if (filesystem.Filelist[fd].currentframePosition + count > BLOCK_FRAME_SIZE){
		count = BLOCK_FRAME_SIZE - filesystem.Filelist[fd].currentframePosition;
		}
	else {
		if (count>BLOCK_FRAME_SIZE) {
			count = BLOCK_FRAME_SIZE ;
		}
	}
	return count;
}
////////////////////////////////////////////////////////////////////////////////
//
// Function     : addNewFrame
// Description  : add new frame in the file name "fd"
//
// Inputs       : fd - filename of the file to read from
//
// Outputs      : bytes read if successful, -1 if failure
int16_t addNewFrame(int16_t fd)
{
	if (filesystem.NextFrameNo < BLOCK_BLOCK_SIZE){
		//allot next available frame
		filesystem.Framelist[filesystem.NextFrameNo].status=1;
		//add frame to framelist of file
		filesystem.Filelist[fd].usedFrame[filesystem.Filelist[fd].no_of_frame]=filesystem.NextFrameNo;
		filesystem.Filelist[fd].currentFrame = filesystem.NextFrameNo;
		//set current frame position
		filesystem.Filelist[fd].currentframePosition = 0;
		filesystem.Filelist[fd].currentframeno = filesystem.Filelist[fd].no_of_frame; //starts with 0
		filesystem.Filelist[fd].usedFrame[filesystem.Filelist[fd].currentframeno] = filesystem.Filelist[fd].currentFrame;
		filesystem.NextFrameNo++;
		filesystem.Filelist[fd].no_of_frame++;
		logMessage(LOG_INFO_LEVEL,"Added new frame count %d. current frame(starts with 0) %d \n", filesystem.Filelist[fd].no_of_frame,filesystem.Filelist[fd].currentFrame);
		return 0;
	}
	else { //Frames exhausted in block
		return -1;
	}
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : setNextFrame
// Description  : set Next frame using inputs file handle fd and count bytes
//
// Inputs       : fd - filename of the file to read from
//                count - number of bytes to read
// Outputs      : calls addNewFrame if more than available frames, return 0
int16_t setNextFrame(int16_t fd, int32_t count)
{//update currentframePostion
	if (filesystem.Filelist[fd].currentframePosition+count < BLOCK_FRAME_SIZE-1) {
		filesystem.Filelist[fd].currentframePosition += count;
		logMessage(LOG_INFO_LEVEL,"frame %d position updated %d with %d file position %d file size %d \n", filesystem.Filelist[fd].currentFrame, filesystem.Filelist[fd].currentframePosition, count, filesystem.Filelist[fd].position, filesystem.Filelist[fd].filesize);
	}
	else { //beyond currentframePosition, so move to next frame for this file
		if (filesystem.Filelist[fd].currentframeno+1 < (filesystem.Filelist[fd].no_of_frame)) {
			filesystem.Filelist[fd].currentframeno++;
			filesystem.Filelist[fd].currentFrame = filesystem.Filelist[fd].usedFrame[filesystem.Filelist[fd].currentframeno];
			filesystem.Filelist[fd].currentframePosition = 0;
			logMessage(LOG_INFO_LEVEL,"moved to new frame no %d frame %d \n", filesystem.Filelist[fd].currentframeno+1,filesystem.Filelist[fd].currentFrame);
		}
		else { //beyond available frames, so add new frame
			logMessage(LOG_INFO_LEVEL,"Adding new frame %d. earlier %d frame position was %d count %d \n", filesystem.Filelist[fd].no_of_frame+1,filesystem.Filelist[fd].currentframeno+1,filesystem.Filelist[fd].currentframePosition,count);
			return addNewFrame(fd);
		}
	}
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : writeCurrentFrame
// Description  : Reads "count" bytes from the file handle"fd" into the
//                buffer "buf"
//
// Inputs       : fd - filename of the file to read from
//                buf - pointer to buffer to read into
//                count - number of bytes to read
// Outputs      : bytes writen if successful, -1 if failure
int32_t writeCurrentFrame(int16_t fd, void* buf, int32_t count)
{
	int success = 0;
	uint32_t testCScode,CScode;
	BlockXferRegister regstate ,RT ;

	if (computeframechecksum(buf, &testCScode)) {
		return -1; //error in checksum
	}
//	printf("Write_Sent_MD5Checksum Buffer %0x %d %s at %d \n", testCScode, testCScode,buf,filesystem.Filelist[fd].currentFrame);
	while (success == 0){
		regstate = create_opcode(BLOCK_OP_WRFRME, filesystem.Filelist[fd].currentFrame, testCScode, 0);
      		regstate = block_io_bus(regstate, buf);
        	RT = get_RTcode(regstate);
        	CScode = get_CScode(regstate);
		logMessage(LOG_INFO_LEVEL, " Write_recd_Checksum %0x %d \n", CScode, CScode);
        	if (CScode == testCScode){
            		success = 1;
				if (RT == -1){ 
					logMessage(LOG_ERROR_LEVEL,"writecurrentframe fails \n");
					return -1;
				}
			}
      }
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : block_read
// Description  : Reads "count" bytes from the file handle "fh" into the
//                buffer "buf"
//
// Inputs       : fd - filename of the file to read from
//                buf - pointer to buffer to read into
//                count - number of bytes to read
// Outputs      : bytes read if successful, -1 if failure

int32_t block_read(int16_t fd, char* buf, int32_t count)
{
	int32_t totalcount,readcount,curpos;
	char* totalbuf;

	if (checkFileHandle(fd))	{return -1;}
	//if position+count > file size, reduce count to file size
	count=checkFileSize(fd,count);
	readcount=0;

	for(totalcount=count;totalcount>0;totalcount-=count)
	{
		count=totalcount;
		count=checkCurrentFrame(fd,count);
		//create buffer to write a frame
		totalbuf = malloc(BLOCK_FRAME_SIZE);
		//memset(totalbuf,0,BLOCK_FRAME_SIZE);
		//currentframeposition will be 0 for new frame or entire frame
		curpos = filesystem.Filelist[fd].currentframePosition;
		if ((readCurrentFrame(fd,totalbuf,BLOCK_FRAME_SIZE))<0) {
			logMessage(LOG_ERROR_LEVEL,"read fails %d \n",count);
			return -1; //error
		}
		
		else {
			logMessage(LOG_INFO_LEVEL,"Read this time %d frames, earlier read %d frames out of %d frames \n",count,readcount,readcount+totalcount);
		}
		//memcpy(buf,totalbuf,count); //rectify
		memcpy(buf+readcount,totalbuf+curpos,count);
		readcount+=count;
		setNextFrame(fd,count);
		filesystem.Filelist[fd].position = filesystem.Filelist[fd].position + readcount;

	}
	return readcount;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : block_write
// Description  : Writes "count" bytes to the file handle "fh" from the
//                buffer  "buf"
//
// Inputs       : fd - filename of the file to write to
//                buf - pointer to buffer to write from
//                count - number of bytes to write
// Outputs      : bytes written if successful, -1 if failure

int32_t block_write(int16_t fd, char* buf, int32_t count)
 {
	int32_t totalcount,writecount,curpos;
	char* totalbuf;

	
	if (checkFileHandle(fd)){
		return -1;
	}
	
	writecount=0;
	for(totalcount=count;totalcount>0;totalcount-=count)
	{
		count=totalcount;
		count=checkCurrentFrame(fd,count);
		//create buffer to write a frame
		totalbuf = malloc(BLOCK_FRAME_SIZE);
		//memset(totalbuf,0x0,BLOCK_FRAME_SIZE);
		//currentframeposition will be 0 for new frame or entire frame
		curpos = filesystem.Filelist[fd].currentframePosition;
		//read existing frame data if not writing entire frame
		if (count<BLOCK_FRAME_SIZE) {
			readCurrentFrame(fd,totalbuf,BLOCK_FRAME_SIZE);
		}
		memcpy(totalbuf+curpos,buf+writecount,count);
		//memcpy(totalbuf,buf,count); //rectify
		if ((writeCurrentFrame(fd,totalbuf,BLOCK_FRAME_SIZE))<0) {
			logMessage(LOG_ERROR_LEVEL,"write fails %d \n",count);
			return -1; //error
		}
		else {
//			logMessage(LOG_INFO_LEVEL,"write this time %d earlier write %d out of %d %s \n",count,writecount,writecount+totalcount,totalbuf);
		}
		writecount+=count;
		setNextFrame(fd,count); //it may call addNewFrame(fd);  if required
	}
	filesystem.Filelist[fd].position = filesystem.Filelist[fd].position + writecount;
	if (filesystem.Filelist[fd].position > filesystem.Filelist[fd].filesize) {
		filesystem.Filelist[fd].filesize = filesystem.Filelist[fd].position;
	}
	return writecount;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : block_seek
// Description  : Seek to specific point in the file
//
// Inputs       : fd - filename of the file to write to
//                loc - offfset of file in relation to beginning of file
// Outputs      : 0 if successful, -1 if failure

int32_t block_seek(int16_t fd, uint32_t loc)
{
	if (checkFileHandle(fd))	{return -1;}
	
	if (filesystem.Filelist[fd].filesize < loc) {
		logMessage(LOG_ERROR_LEVEL, "Moving to %d beyond Size of File %d",loc,filesystem.Filelist[fd].filesize);
		return -1; }

	filesystem.Filelist[fd].currentframeno = loc/BLOCK_FRAME_SIZE;
	filesystem.Filelist[fd].currentframePosition = loc%BLOCK_FRAME_SIZE;
	filesystem.Filelist[fd].currentFrame = filesystem.Filelist[fd].usedFrame[filesystem.Filelist[fd].currentframeno];
	filesystem.Filelist[fd].position = loc;
	logMessage(LOG_INFO_LEVEL,"Successfully positioned %d (frame %d position %d) file size %d \n",filesystem.Filelist[fd].position,filesystem.Filelist[fd].currentframeno,filesystem.Filelist[fd].currentframePosition,filesystem.Filelist[fd].filesize);
    // Return successfully
    return (0);
}
