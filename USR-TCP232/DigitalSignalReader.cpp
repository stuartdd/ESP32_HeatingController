/*
The MIT License (MIT)

Copyright (c) 2016 British Broadcasting Corporation.
This software is provided by Lancaster University by arrangement with the BBC.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

/*
http://192.168.1.70:8233
*/

#include "MicroBit.h"
#include "MicroBitPin.h"

#define BUFFER_SIZE 1200
#define DELAY_NS 200
#define WAIL_FOT_TRIGGER 20000
int OFF_POS = 0;
int ON_POS = 1;
int COLON_POS = 2;
int SPACE_POS = 3;
MicroBit uBit;
MicroBitSerial serial(USBTX, USBRX);
MicroBitPin P8(MICROBIT_ID_IO_P8, MICROBIT_PIN_P8, PIN_CAPABILITY_DIGITAL);
MicroBitPin P11(MICROBIT_ID_IO_P11, MICROBIT_PIN_P11, PIN_CAPABILITY_DIGITAL);


uint8_t buf[BUFFER_SIZE+1];
uint8_t numBufRev[10];
uint8_t numBuf[10];
int numBufPos;
//                     0, 1, 2, 3, 4, 5, 6, 7, 8, 9
uint8_t numCodes[] = {48,49,50,51,52,53,54,55,56,57};
//                  -, +, :,
uint8_t codes[] = {45,43,58,32};

int pos;
int current;
unsigned long tim;
unsigned long syncStart;

void sendNum(long num);

int main()
{
    // Initialise the micro:bit runtime.
    uBit.init();
    serial.baud(460800);
    buf[BUFFER_SIZE] = codes[COLON_POS];
    while(1) {
        uBit.display.print('-');
        syncStart = uBit.systemTime();
        current = P8.getDigitalValue();
        while (P8.getDigitalValue()==current) {
            if ((uBit.systemTime() - syncStart) > WAIL_FOT_TRIGGER) {
                break;
            } else {
                if (P11.getDigitalValue() == 0) {
                    P11.setDigitalValue(1);
                    break;
                }
                wait_us(1);
            }
        }
        uBit.display.print('*');
        tim = uBit.systemTime();
        for (pos = 0; pos < BUFFER_SIZE; pos++) {
            buf[pos] = codes[P8.getDigitalValue()];
            wait_us(DELAY_NS);
        }
        uBit.display.print('>');
        sendNum(tim);
        sendNum(uBit.systemTime() - tim);
        serial.send((uint8_t *)buf, BUFFER_SIZE + 1);
    }
}

void sendNum(long num)
{
    if (num == 0) {
        numBuf[0] = numCodes[0];
        numBufPos = 1;
    } else {
        numBufPos = 0;
        while (num != 0) {
            numBufRev[numBufPos] = numCodes[num % 10];
            num = num / 10;
            numBufPos++;
        }
        for (int j = 0; j < numBufPos; j++) {
            numBuf[j] = numBufRev[numBufPos-(j+1)];
        }
    }
    numBuf[numBufPos] = codes[SPACE_POS];
    numBufPos ++;
    serial.send((uint8_t *)numBuf, numBufPos);
}
