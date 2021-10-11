/*
 asix_eepromtool
 eeprom programming tool for ASIX-based USB ethernet interfaces

 <github@karosium.e4ward.com>

 Extended by Albin Söderqvist <albin.soderqvist@hostmobility.com>

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

static int open_device(int bus, int devnum, unsigned int vid, unsigned int pid)
{
	int i = 0;
	int last_bus = 0;
	int last_device = 0;
	int status = libusb_init(NULL);
	ssize_t cnt;
	struct libusb_device_descriptor desc;
	unsigned char serial_number[100];

	if (status < 0) {
		fprintf(stderr, "failed to initialize libusb");
		return status;
	}
#if defined(LIBUSB_API_VERSION) && (LIBUSB_API_VERSION >= 0x01000106)
	libusb_set_option(NULL, LIBUSB_OPTION_LOG_LEVEL, 0);
#else
	libusb_set_debug(NULL, 0);
#endif

	cnt = libusb_get_device_list(NULL, &devs);
	if (cnt < 0) {
		libusb_exit(NULL);
		return (int) cnt;
	}

	if (bus && devnum) {
		/* Find the last bus matching vid and pid */
		if (bus < 0) {
			while ((dev = devs[i++]) != NULL) {
				status = libusb_get_device_descriptor(dev, &desc);
				if (status < 0) {
					fprintf(stderr, "failed to get device descriptor");
					return status;
				}
				if (desc.idVendor == vid
				    && desc.idProduct == pid
				    && libusb_get_bus_number(dev) > last_bus) {
					last_bus = libusb_get_bus_number(dev);
				}
			}
			if (last_bus > 0) {
				bus = last_bus;
				i = 0;
			} else {
				fprintf(stderr, "failed to get a valid address on bus %d", bus);
				return -1;
			}
		}
		/* Find the last device on the bus matching vid and pid */
		if (devnum < 0) {
			while ((dev = devs[i++]) != NULL) {
				status = libusb_get_device_descriptor(dev, &desc);
				if (status < 0) {
					fprintf(stderr, "failed to get device descriptor");
					return status;
				}

				if (desc.idVendor == vid
				    && desc.idProduct == pid
				    && libusb_get_bus_number(dev) == bus
				    && libusb_get_device_address(dev) > last_device) {
					last_device = libusb_get_device_address(dev);
				}
			}
			if (last_device > 0) {
				devnum = last_device;
				i = 0;
			} else {
				fprintf(stderr, "failed to get a valid address on bus %d", bus);
				return -1;
			}
		}
		/* Find the device matching vid, pid, bus and device number */
		while ((dev = devs[i++]) != NULL) {
			status = libusb_get_device_descriptor(dev, &desc);
			if (status < 0) {
				fprintf(stderr, "failed to get device descriptor");
				return status;
			}
			if (desc.idVendor == vid
			    && desc.idProduct == pid
			    && libusb_get_bus_number(dev) == bus
			    && libusb_get_device_address(dev) == devnum) {
				printf("Accessing bus %d, device %d, vid:pid %04x:%04x",
				       libusb_get_bus_number(dev), libusb_get_device_address(dev),
				       desc.idVendor, desc.idProduct);
				break;
			}
		}
		if (dev == NULL || libusb_get_device_address(dev) != devnum) {
			fprintf(stderr, "Could not find USB device %d\n", devnum);
			exit(1);
		}
		status = libusb_open(dev, &device);
		if (status) {
			printf("\nlibusb_open() failed. Is device connected? Are you root?\n");
			exit(0);
		}
	} else {
		device = libusb_open_device_with_vid_pid(NULL, (uint16_t)vid, (uint16_t)pid);
		if (device == NULL) {
			printf("libusb_open() failed. Is device connected? Are you root?\n");
			exit(0);
		}
	}

	status = libusb_get_string_descriptor_ascii(device, desc.iSerialNumber, serial_number, 100);
	if (status > 0) {
		printf(", serial number %s\n", serial_number);
	} else {
		printf("\n");
	}
	libusb_detach_kernel_driver(device, 0);
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
	  printf("--bus=<bus> -b  <bus>                   =   bus number, e.g. 2 or -1 to select the last\n");
	  printf("--device-number=<n> -n  <n>             =   device number, e.g. 4 or -1 to select the last\n");
	  printf("--read=<file> ,  -r <file>              =   save the eeprom contents to <file>\n");
	  printf("--write=<file> ,   -w <file>            =   write <file> to eeprom\n");
	  printf("--size=<# of bytes> , -s <# of bytes>   =   size of eeprom in bytes (e.g. 256 or 512)\n");
	  printf("\n");
	  printf("example:\n");
	  printf("asix_eepromtool -d 0b95:772b -b 2 -n 10 -r eep.bin -s 256\n");
	  printf("\n");
	  printf("ps. Run this tool as root\n");
}



int main(int argc, char **argv) {
	int status;
	uint8_t eepbuf[65536];
	int c;
	int devnum = 0;
	int bus = 0;
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
          {"bus",  required_argument, 0, 'b'},
          {"device-number",  required_argument, 0, 'n'},
          {"read",  required_argument, 0, 'r'},
          {"write",    required_argument, 0, 'w'},
          {"size",    required_argument, 0, 's'},
	          {0, 0, 0, 0}
       	};

	while (1)
	{

	      int option_index = 0;

	      c = getopt_long (argc, argv, "d:b:n:r:w:s:",
                       long_options, &option_index);

	      if (c == -1)
	        break;

	      switch (c)
	        {
	        case 0:
	          if (long_options[option_index].flag != 0)
	            break;

		case 'b':
			bus = atoi(optarg);
			if (bus < -1) {
				printUsage();
				exit(1);
			}
			break;

		case 'n':
			devnum = atoi(optarg);
			if (devnum < -1) {
				printUsage();
				exit(1);
			}
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

	if (bus && devnum) {
		printf("Bus %d, device %d\n", bus, devnum);
	}
	printf("EEPROM is %d bytes\n",eepsize);

	status = open_device(bus,devnum,vid,pid);

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
