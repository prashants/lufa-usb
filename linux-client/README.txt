This is the Ubuntu Linux based test client that communicates with the
Generic USB HID device over the Interrupt-In and Interrupt-Out endpoints

Interrupt-In : 0x81
Interrupt-Out : 0x02

Compile the test program :
$gcc -DLINUX test.c -lusb-1.0 -o test

Output of lsusb : lsusb.txt

