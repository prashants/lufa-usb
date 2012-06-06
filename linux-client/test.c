#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <libusb-1.0/libusb.h>

#define VERSION "0.1.0"
#define VENDOR_ID 0x03eb
#define PRODUCT_ID 0x204f
#define TEST_COUNT 100

// HID Class-Specific Requests values. See section 7.2 of the HID specifications
#define HID_GET_REPORT                0x01
#define HID_GET_IDLE                  0x02
#define HID_GET_PROTOCOL              0x03
#define HID_SET_REPORT                0x09
#define HID_SET_IDLE                  0x0A
#define HID_SET_PROTOCOL              0x0B
#define HID_REPORT_TYPE_INPUT         0x01
#define HID_REPORT_TYPE_OUTPUT        0x02
#define HID_REPORT_TYPE_FEATURE       0x03

#define CTRL_IN        LIBUSB_ENDPOINT_IN|LIBUSB_REQUEST_TYPE_CLASS|LIBUSB_RECIPIENT_INTERFACE
#define CTRL_OUT       LIBUSB_ENDPOINT_OUT|LIBUSB_REQUEST_TYPE_CLASS|LIBUSB_RECIPIENT_INTERFACE

const static int PACKET_INT_LEN = 8;
const static int INTERFACE = 0;
const static int ENDPOINT_INT_IN = 0x81; /* endpoint 0x81 address for IN */
const static int ENDPOINT_INT_OUT = 0x02; /* endpoint 0x02 address for OUT */
const static int TIMEOUT = 5000; /* timeout in ms */

void bad(const char *why)
{
	fprintf(stderr,"Fatal error> %s\n",why);
	exit(17);
}

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
	char answer[PACKET_INT_LEN];
	char question[PACKET_INT_LEN];

	struct timeval start, end;
	long delay, seconds, useconds;
	int count;

	/* write to interrupt-out endpoint */
	for (i = 0; i < PACKET_INT_LEN; i++) question[i] = 0x00 + i;
	r = libusb_interrupt_transfer(devh, ENDPOINT_INT_OUT, question, PACKET_INT_LEN,
			&transferred, TIMEOUT);
	if (r < 0) {
		fprintf(stderr, "Interrupt write error %d\n", r);
		return r;
	}

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
	}
	gettimeofday(&end, NULL);
	count--;

	/* calculating delay */
	seconds = end.tv_sec - start.tv_sec;
	useconds = end.tv_usec - start.tv_usec;
	delay = ((seconds * 1000) + (useconds / 1000.0));
	printf("Time taken to read %d packets is %ld micro seconds, average read delay is %ld micro seconds\n", count, delay, delay/count);

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

	libusb_release_interface(devh, 0);
out:
	libusb_reset_device(devh);
	libusb_close(devh);
	libusb_exit(NULL);
	return r >= 0 ? r : -r;
} 

