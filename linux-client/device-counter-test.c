#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <libusb-1.0/libusb.h>

#define VENDOR_ID 0x03eb
#define PRODUCT_ID 0x204f

#define TEST_COUNT 60000
#define DUMMY_READ

const static int PACKET_INT_LEN = 64;
const static int INTERFACE = 0;
const static int ENDPOINT_INT_IN = 0x81;	/* endpoint 0x81 address for IN */
const static int ENDPOINT_INT_OUT = 0x02;	/* endpoint 0x02 address for OUT */
const static int TIMEOUT = 5000;		/* timeout in ms */

static struct libusb_device_handle *devh = NULL;

static int find_lvr_hidusb(void)
{
	devh = libusb_open_device_with_vid_pid(NULL, VENDOR_ID, PRODUCT_ID);
	return devh ? 0 : -EIO;
}

static int test_interrupt_transfer(void)
{
	int r, i;
	int transferred;
	unsigned char answer[PACKET_INT_LEN];
	unsigned char question[PACKET_INT_LEN];

	unsigned int data;
	unsigned int temp;

	struct timeval start, end;
	long delay, seconds, useconds;
	int count;

	for (i = 0; i < PACKET_INT_LEN; i++) question[i] = 0x00 + i;

	while (1) {
		data = 0;

		/* read from interrupt-in endpoint */
		printf("Starting read test...\n");
		gettimeofday(&start, NULL);
		for (count = 1; count <= TEST_COUNT; count++) {
			r = libusb_interrupt_transfer(devh, ENDPOINT_INT_IN, answer, PACKET_INT_LEN,
					&transferred, TIMEOUT);
			if (r < 0) {
				fprintf(stderr, "Interrupt read error %d\n", r);
				return r;
			}
			if (transferred < PACKET_INT_LEN) {
				fprintf(stderr, "Interrupt transfer short read (%d)\n", r);
				return -1;
			}

			/* validating data received. whether it is one more than last read - skip if first packet */
			if (data == 0) {
				data = answer[1] << 8;
				data |= answer[0];
			} else {
				temp = data;
				data = answer[1] << 8;
				data |= answer[0];
				printf("%d\n", data);
				if ((data - temp) != 1) {
					fprintf(stderr, "Data Loss at data %d\n", data);
					return -1;
				}
			}
		}
		gettimeofday(&end, NULL);
		count--;	/* removing the last undesired count from the previous for loop */

		/* calculating delay */
		seconds = end.tv_sec - start.tv_sec;
		useconds = end.tv_usec - start.tv_usec;
		delay = ((seconds * 1000) + (useconds / 1000.0));
		printf("Time taken to read %d packets is %ld micro seconds, average read delay is %ld micro seconds\n", count, delay, delay/count);

	} // END OF MAIN WHILE

	for (i = 0; i < PACKET_INT_LEN; i++) {
		if (i % 8 == 0)
			printf("\n");
		printf("%02d  ", answer[i]);
	}

	printf("\n");

	return 0;
}

int main(void)
{
	int r = 1;
	time_t cur_time; /* current time */

	r = libusb_init(NULL);
	if (r < 0) {
		fprintf(stderr, "Failed to initialise libusb\n");
		exit(1);
	}

	r = find_lvr_hidusb();
	if (r < 0) {
		fprintf(stderr, "Could not find/open LVR Generic HID device\n");
		goto out;
	}
	printf("Successfully find the LVR Generic HID device\n");

#ifdef LINUX
	libusb_detach_kernel_driver(devh, 0);
#endif

	r = libusb_set_configuration(devh, 1);
	if (r < 0) {
		fprintf(stderr, "libusb_set_configuration error %d\n", r);
		goto out;
	}
	printf("Successfully set usb configuration 1\n");
	r = libusb_claim_interface(devh, 0);
	if (r < 0) {
		fprintf(stderr, "libusb_claim_interface error %d\n", r);
		goto out;
	}
	printf("Successfully claimed interface\n");

	printf("Testing interrupt transfer using loop back test of input/output report\n");
	test_interrupt_transfer();

	/* printing current time at end of experiment */
	time(&cur_time);
	printf("The current Date and Time is: %s\n", ctime(&cur_time));

	libusb_release_interface(devh, 0);
out:
	libusb_reset_device(devh);
	libusb_close(devh);
	libusb_exit(NULL);
	return r >= 0 ? r : -r;
} 

