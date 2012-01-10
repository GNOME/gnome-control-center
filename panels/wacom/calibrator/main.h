/*
 * Copyright (c) 2009 Tias Guns
 * Copyright (c) 2009 Soren Hauberg
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

#ifndef _main_h
#define _main_h

#include "calibrator.h"


int find_device(const char*, gboolean, gboolean, XID*, const char**, XYinfo*);

static void usage(char* cmd, unsigned thr_misclick);

struct Calib* main_common(int argc, char** argv);

struct Calib* CalibratorXorgPrint(const char* const device_name, const XYinfo *axis,
        const gboolean verbose, const int thr_misclick, const int thr_doubleclick,
        const char* geometry);

gboolean finish_data(struct Calib*, const XYinfo new_axis, int swap_xy);
gboolean output_xorgconfd(struct Calib*, const XYinfo new_axis, int swap_xy, int new_swap_xy);

int main(int argc, char** argv);

#endif
