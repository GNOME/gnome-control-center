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

#pragma once

#include <glib.h>
#include "calibrator-gui.h"

G_BEGIN_DECLS

typedef enum {
        CC_CALIBRATOR_STATE_UPPER_LEFT,
        CC_CALIBRATOR_STATE_UPPER_RIGHT,
        CC_CALIBRATOR_STATE_LOWER_LEFT,
        CC_CALIBRATOR_STATE_LOWER_RIGHT,
        CC_CALIBRATOR_STATE_COMPLETE
} CcCalibratorState;

#define CC_TYPE_CALIBRATOR (cc_calibrator_get_type ())
G_DECLARE_FINAL_TYPE (CcCalibrator, cc_calibrator, CC, CALIBRATOR, GObject)

CcCalibrator * cc_calibrator_new (int threshold_doubleclick,
                                  int threshold_mislick);

void cc_calibrator_get_thresholds (CcCalibrator *c,
                                   int *threshold_doubleclick,
                                   int *threshold_misclick);

void cc_calibrator_reset         (CcCalibrator *c);

void cc_calibrator_update_geometry (CcCalibrator *c,
                                    int           width,
                                    int           height);

CcCalibratorState cc_calibrator_get_state (CcCalibrator *c);

gboolean cc_calibrator_add_click (CcCalibrator *c,
                                  int           x,
                                  int           y);
gboolean cc_calibrator_finish    (CcCalibrator *c,
                                  XYinfo       *new_axis,
                                  gboolean     *swap);

G_END_DECLS
