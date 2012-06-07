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

	/* initialize write data */
	for (i = 0; i < PACKET_INT_LEN; i++) question[i] = 0x00 + i;

	/* write to interrupt-out endpoint */
	r = libusb_interrupt_transfer(devh, ENDPOINT_INT_OUT, question, PACKET_INT_LEN,
			&transferred, TIMEOUT);
	if (r < 0) {
		fprintf(stderr, "Interrupt write error %d\n", r);
		return r;
	}
	printf("Written %d bytes\n", transferred);

	/* read from interrupt-in endpoint */
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
	printf("Read %d bytes\n", transferred);

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

