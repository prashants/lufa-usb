/*
             LUFA Library
     Copyright (C) Dean Camera, 2012.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org
*/

/*
  Copyright 2012  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaim all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/** \file
 *
 *  Main source file for the GenericHID demo. This file contains the main tasks of the demo and
 *  is responsible for the initial application hardware configuration.
 */

#include "GenericHID.h"

volatile uint8_t counter = 1;
RingBuffer_t Buffer;
uint8_t BufferData[70];

void initTimer3(void);

/** Main program entry point. This routine configures the hardware required by the application, then
 *  enters a loop to run the application tasks in sequence.
 */
int main(void)
{
	SetupHardware();

        RingBuffer_InitBuffer(&Buffer, BufferData, sizeof(BufferData));
        counter = 1;

	sei();

	for (;;)
	{
		HID_Task();
		USB_USBTask();
	}
}

/** Configures the board hardware and chip peripherals for the demo's functionality. */
void SetupHardware(void)
{
	/* Disable watchdog if enabled by bootloader/fuses */
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	/* Disable clock division */
	clock_prescale_set(clock_div_1);

	/* Hardware Initialization */
	USB_Init();
        Serial_Init(9600, false);
}

/** Event handler for the USB_Connect event. This indicates that the device is enumerating via the status LEDs and
 *  starts the library USB task to begin the enumeration and USB management process.
 */
void EVENT_USB_Device_Connect(void)
{
	/* Indicate USB enumerating */
}

/** Event handler for the USB_Disconnect event. This indicates that the device is no longer connected to a host via
 *  the status LEDs and stops the USB management task.
 */
void EVENT_USB_Device_Disconnect(void)
{
	/* Indicate USB not ready */
}

/** Event handler for the USB_ConfigurationChanged event. This is fired when the host sets the current configuration
 *  of the USB device after enumeration, and configures the generic HID device endpoints.
 */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	bool ConfigSuccess = true;

	/* Setup HID Report Endpoints */
	ConfigSuccess &= Endpoint_ConfigureEndpoint(GENERIC_IN_EPNUM, EP_TYPE_INTERRUPT, ENDPOINT_DIR_IN,
	                                            GENERIC_EPSIZE, ENDPOINT_BANK_SINGLE);
	ConfigSuccess &= Endpoint_ConfigureEndpoint(GENERIC_OUT_EPNUM, EP_TYPE_INTERRUPT, ENDPOINT_DIR_OUT,
	                                            GENERIC_EPSIZE, ENDPOINT_BANK_SINGLE);

	/* Indicate endpoint configuration success or failure */
}

/** Event handler for the USB_ControlRequest event. This is used to catch and process control requests sent to
 *  the device from the USB host before passing along unhandled control requests to the library for processing
 *  internally.
 */
void EVENT_USB_Device_ControlRequest(void)
{
	/* Handle HID Class specific requests */
	switch (USB_ControlRequest.bRequest)
	{
		case HID_REQ_GetReport:
			if (USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				uint8_t GenericData[GENERIC_REPORT_SIZE];
				CreateGenericHIDReport(GenericData);

				Endpoint_ClearSETUP();

				/* Write the report data to the control endpoint */
				Endpoint_Write_Control_Stream_LE(&GenericData, sizeof(GenericData));
				Endpoint_ClearOUT();
			}

			break;
		case HID_REQ_SetReport:
			if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				uint8_t GenericData[GENERIC_REPORT_SIZE];

				Endpoint_ClearSETUP();

				/* Read the report data from the control endpoint */
				Endpoint_Read_Control_Stream_LE(&GenericData, sizeof(GenericData));
				Endpoint_ClearIN();

				ProcessGenericHIDReport(GenericData);
			}

			break;
	}
}

/** Function to process the last received report from the host.
 *
 *  \param[in] DataArray  Pointer to a buffer where the last received report has been stored
 */
void ProcessGenericHIDReport(uint8_t* DataArray)
{
	/*
		This is where you need to process reports sent from the host to the device. This
		function is called each time the host has sent a new report. DataArray is an array
		holding the report sent from the host.
	*/
	Serial_SendString("DAQ START COMMAND RECEIVED\r\n");
        counter = 1;
        initTimer3();
}

/** Function to create the next report to send back to the host at the next reporting interval.
 *
 *  \param[out] DataArray  Pointer to a buffer where the next report data should be stored
 */
void CreateGenericHIDReport(uint8_t* DataArray)
{
	/*
		This is where you need to create reports to be sent to the host from the device. This
		function is called each time the host is ready to accept a new report. DataArray is
		an array to hold the report to the host.
	*/
        uint16_t c = 0;

        for (c = 0; c < GENERIC_REPORT_SIZE; c++) {
              DataArray[c] = 0;
        }
	if (!RingBuffer_IsEmpty(&Buffer)) {
		DataArray[0] = RingBuffer_Remove(&Buffer);
	}

	/* returning the free ring buffer count as 2nd byte in the data stream */
	c = RingBuffer_GetFreeCount(&Buffer);
	DataArray[1] = c;
}

void HID_Task(void)
{
	/* Device must be connected and configured for the task to run */
	if (USB_DeviceState != DEVICE_STATE_Configured)
	  return;

	Endpoint_SelectEndpoint(GENERIC_OUT_EPNUM);

	/* Check to see if a packet has been sent from the host */
	if (Endpoint_IsOUTReceived())
	{
		/* Check to see if the packet contains data */
		if (Endpoint_IsReadWriteAllowed())
		{
			/* Create a temporary buffer to hold the read in report from the host */
			uint8_t GenericData[GENERIC_REPORT_SIZE];

			/* Read Generic Report Data */
			Endpoint_Read_Stream_LE(&GenericData, sizeof(GenericData), NULL);

			/* Process Generic Report Data */
			ProcessGenericHIDReport(GenericData);
		}

		/* Finalize the stream transfer to send the last packet */
		Endpoint_ClearOUT();
	}

	Endpoint_SelectEndpoint(GENERIC_IN_EPNUM);

	/* Check to see if the host is ready to accept another packet */
	if (Endpoint_IsINReady())
	{
		/* Create a temporary buffer to hold the report to send to the host */
		uint8_t GenericData[GENERIC_REPORT_SIZE];

		/* Create Generic Report Data */
		CreateGenericHIDReport(GenericData);

		/* Write Generic Report Data */
		Endpoint_Write_Stream_LE(&GenericData, sizeof(GenericData), NULL);

		/* Finalize the stream transfer to send the last packet */
		Endpoint_ClearIN();
	}
}

/* Copied from the serial library */
void Serial_SendString(const char* StringPtr)
{
        uint8_t CurrByte;

        while ((CurrByte = *StringPtr) != 0x00)
        {
                Serial_SendByte(CurrByte);
                StringPtr++;
        }
}

void initTimer3()
{
       TCCR3A|=(1<<COM3A0);                         // Toggle mode
       TCCR3B|=(1<<WGM32)|(1<<CS32);                // CTC mode and CPU clock/256
       TIMSK3|=(1<<OCIE3A);                         // Output compare interrupt enable
       OCR3A = 200;                                 // 200 = 3.2 milliseconds
       TCNT3 = 0;
}

ISR(TIMER3_COMPA_vect)
{
        if (counter == 255)
                counter = 1;
        else
                counter++;
        if (!RingBuffer_IsFull(&Buffer))
                RingBuffer_Insert(&Buffer, counter);
}

