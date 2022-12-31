/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef USB_DESCRIPTORS_H_
#define USB_DESCRIPTORS_H_

// Interfaces
enum
{
  ITF_NUM_KEYBOARD,
  ITF_NUM_HID,
  ITF_NUM_CDC,
  ITF_NUM_CDC_DATA, // CDC needs 2 interfaces
  ITF_NUM_TOTAL
};

enum
{
  REPORT_ID_MOUSE = 1,
  REPORT_ID_CONSUMER_CONTROL,
  REPORT_ID_SYSTEM_CONTROL,
  REPORT_ID_COUNT
};

// 20-byte NKRO: support keycodes upto LANG8(0x97)
#define KEYBOARD_REPORT_SIZE    20
#define KEYBOARD_REPORT_KEYS    (KEYBOARD_REPORT_SIZE - 2)
#define KEYBOARD_REPORT_BITS    (KEYBOARD_REPORT_SIZE - 1)

#endif /* USB_DESCRIPTORS_H_ */
