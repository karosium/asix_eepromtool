/*
 asix_eepromtool
 eeprom programming tool for ASIX-based USB ethernet interfaces

 <github@karosium.e4ward.com>

- UNLICENSE -

This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>
*/

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/types.h>
#include <libusb.h>
#include <strings.h>
#include <endian.h>
#include <getopt.h>

#define ASIX_CMD_READ_EEPROM		0x0b 
#define ASIX_CMD_WRITE_EEPROM		0x0c 
#define ASIX_CMD_WRITE_EEPROM_EN	0x0d 
#define ASIX_CMD_WRITE_EEPROM_DIS	0x0e

static libusb_device *dev, **devs;
static libusb_device_handle *device = NULL;


int OpenDeviceVIDPID(unsigned int vid,unsigned int pid){
	int status;

	status = libusb_init(NULL);
	if (status < 0) {
		printf("libusb_init() failed: %s\n", libusb_error_name(status));
		exit(0);
	}
	libusb_set_option(NULL, LIBUSB_OPTION_LOG_LEVEL, 0);
	
	device = libusb_open_device_with_vid_pid(NULL, (uint16_t)vid, (uint16_t)pid);
		if (device == NULL) {
			printf("libusb_open() failed. Is device connected? Are you root?\n");
			exit(0);
		}	

	libusb_set_auto_detach_kernel_driver(device, 1);
	status = libusb_claim_interface(device, 0);
	if (status != LIBUSB_SUCCESS) {
		libusb_close(device);
		printf("libusb_claim_interface failed: %s\n", libusb_error_name(status));
		exit(0);
	}

	return status;
}


static int asix_read(uint8_t cmd, uint16_t value, uint16_t index, uint16_t size, void *data)
{

	return libusb_control_transfer(device,
					LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
					cmd,
					value, 
					index,
					data, 
					size, 
					100);
}

static int asix_write(uint8_t cmd, uint16_t value, uint16_t index, uint16_t size, void *data)
{

	return libusb_control_transfer(device,
					LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
					cmd,
					value, 
					index,
					data, 
					size, 
					100);
}

int read_eeprom(int rsize, uint16_t* buf) {

	int i, retval = 0;
	uint16_t tmp16;	

	for (i = 0; i < rsize/2; i++) {
		retval = asix_read(ASIX_CMD_READ_EEPROM, i, 0, 2, &tmp16);

		if(retval < 0) {
			return retval;
		}

		*(buf + i) = be16toh(tmp16);
	}
	return retval;
	
}

int write_eeprom(int wsize, uint16_t* buf) {
	int i, retval;

	retval = asix_write(ASIX_CMD_WRITE_EEPROM_EN, 0, 0, 0, NULL);
	if (retval < 0) {
		 return retval;
	}

	sleep(1);

	for (i = 0; i < wsize/2; i++){
		retval = asix_write(ASIX_CMD_WRITE_EEPROM, i, htobe16(*(buf + i)), 0, NULL);

		if (retval < 0) {
			return retval;
		}
		usleep(50000);
	}

	retval = asix_write(ASIX_CMD_WRITE_EEPROM_DIS, 0, 0, 0, NULL);
	if (retval < 0) {
			return retval;
	}

	return retval;
}

void printHeader() {

	  printf("------------------------------------\n");
	  printf("        asix_eepromtool\n");
 	  printf("------------------------------------\n");
}
void printUsage() {
	  printHeader();
	  printf("options:\n");
	  printf("--device=<vid:pid> -d  <vid:pid>        =   vid and pid of device in hex eg. 0b95:772b\n");
	  printf("--read=<file> ,  -r <file>              =   save the eeprom contents to <file>\n");
	  printf("--write=<file> ,   -w <file>            =   write <file> to eeprom\n");
	  printf("--size=<# of bytes> , -s <# of bytes>   =   size of eeprom in bytes (probably 256)\n");
	  printf("\n");
	  printf("example:\n");
	  printf("asix_eepromtool -d 0b95:772b -r eep.bin -s 256\n");
	  printf("\n");
	  printf("ps. Run this tool as root\n");
}



int main(int argc, char **argv) {
	int status;
	uint8_t eepbuf[65536];
	int c;
	uint16_t vid = 0;
	uint16_t pid = 0;
	uint16_t eepsize = 0;
	char *tmp;
	FILE *readFile = NULL;
	FILE *writeFile = NULL;

	if (argc==1) {
		 printUsage();
		 exit(1);
	}

	static struct option long_options[] =
        {
          {"device",  required_argument, 0, 'd'},
          {"read",  required_argument, 0, 'r'},
          {"write",    required_argument, 0, 'w'},
          {"size",    required_argument, 0, 's'},
	          {0, 0, 0, 0}
       	};

	while (1)
	{

	      int option_index = 0;

	      c = getopt_long (argc, argv, "d:r:w:s:",
                       long_options, &option_index);

	      if (c == -1)
	        break;

	      switch (c)
	        {
	        case 0:
	          if (long_options[option_index].flag != 0)
	            break;

        	case 'd':
			tmp = strtok(optarg,":");
			vid = strtoul(tmp,NULL,16);
			tmp = strtok(NULL,":");
			if (tmp != NULL)
				pid=strtoul(tmp,NULL,16);
	        break;

        	case 'r':
			readFile = fopen(optarg,"wb");				          
	        break;

	        case 'w':
	        	writeFile = fopen(optarg,"rb");
	        break;
		case 's':		
			eepsize = strtoul(optarg,NULL,10);
		break;
	        case '?':
			printUsage();
			exit(0);
	          break;
	        default:
		  abort;
	        }
    }

	printHeader();
	if ((vid==0) | (pid==0)) {
		printf("Device VID:PID missing or wrong format\n");
		exit(0);
	} 
	if (eepsize ==0) {
		printf("EEPROM size not specified\n");
		exit(0);
	} 

	if ((readFile ==NULL)&&(writeFile==NULL)) {
		printf("Read/write filename not specified or file open error\n");
		exit(0);
	}

	if (eepsize %2 != 0) {
		printf("EEPROM size must be a multiple of 2\n");
		exit(0);
	}


        printf("Device is %04X:%04X\n",vid,pid);
	printf("EEPROM is %d bytes\n",eepsize);

	status = OpenDeviceVIDPID(vid,pid);

	if (status!=0) {
		printf("Device open error %d\n",status);
		exit(0);
	} else {
		printf("Device opened\n");
	}

	if (readFile !=NULL) {
		printf("Reading...\n");
		status = read_eeprom(eepsize,(uint16_t*)&eepbuf);
		fwrite(&eepbuf,1,eepsize,readFile);
		fclose(readFile);

	}

	if (writeFile != NULL) {
		printf("Writing...\n");
		fread(&eepbuf,1,eepsize,writeFile);
		status = write_eeprom(eepsize,(uint16_t*)&eepbuf);
        	fclose(writeFile);
		
	}
	if (status <0) {
		printf("Error %d\n",status);
	} else {
		printf("Done.\n");	
	}
}
