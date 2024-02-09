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

#include <stdlib.h>

#include "calibrator.h"

/*
 * Number of blocks. We partition the screen into 'num_blocks' x 'num_blocks'
 * rectangles of equal size. We then ask the user to press points that are
 * located at the corner closes to the center of the four blocks in the corners
 * of the screen. The following ascii art illustrates the situation. We partition
 * the screen into 8 blocks in each direction. We then let the user press the
 * points marked with 'O'.
 *
 *   +--+--+--+--+--+--+--+--+
 *   |  |  |  |  |  |  |  |  |
 *   +--O--+--+--+--+--+--O--+
 *   |  |  |  |  |  |  |  |  |
 *   +--+--+--+--+--+--+--+--+
 *   |  |  |  |  |  |  |  |  |
 *   +--+--+--+--+--+--+--+--+
 *   |  |  |  |  |  |  |  |  |
 *   +--+--+--+--+--+--+--+--+
 *   |  |  |  |  |  |  |  |  |
 *   +--+--+--+--+--+--+--+--+
 *   |  |  |  |  |  |  |  |  |
 *   +--+--+--+--+--+--+--+--+
 *   |  |  |  |  |  |  |  |  |
 *   +--O--+--+--+--+--+--O--+
 *   |  |  |  |  |  |  |  |  |
 *   +--+--+--+--+--+--+--+--+
 */
#define NUM_BLOCKS 8

typedef enum {
    HORIZONTAL,
    VERTICAL,
} Alignment;

typedef struct
{
    int x, y;
} Point;

struct _CcCalibrator
{
    /* Geometry of the calibration window */
    GdkRectangle geometry;

    CcCalibratorState state;

    /* click coordinates */
    Point clicked[CC_CALIBRATOR_STATE_COMPLETE];

    /* Threshold to keep the same point from being clicked twice.
     * Set to zero if you don't want this check
     */
    int threshold_doubleclick;

    /* Threshold to detect mis-clicks (clicks not along axes)
     * A lower value forces more precise calibration
     * Set to zero if you don't want this check
     */
    int threshold_misclick;
};

G_DEFINE_TYPE (CcCalibrator, cc_calibrator, G_TYPE_OBJECT)

#define SWAP(valtype,x,y)		\
    G_STMT_START {			\
    valtype t; t = (x); x = (y); y = t;	\
    } G_STMT_END

static void
cc_calibrator_init (CcCalibrator *c)
{
    c->state = CC_CALIBRATOR_STATE_UPPER_LEFT;
}

static void
cc_calibrator_class_init (CcCalibratorClass *klass)
{
}

CcCalibrator *
cc_calibrator_new (int threshold_doubleclick,
                   int threshold_misclick)
{
    CcCalibrator *c = g_object_new (CC_TYPE_CALIBRATOR, NULL);

    c->threshold_doubleclick = threshold_doubleclick;
    c->threshold_misclick = threshold_misclick;

    return c;
}

void cc_calibrator_get_thresholds (CcCalibrator *c,
                                   int *threshold_doubleclick,
                                   int *threshold_misclick)
{
    *threshold_doubleclick = c->threshold_doubleclick;
    *threshold_misclick = c->threshold_misclick;
}

void
cc_calibrator_reset (CcCalibrator *c)
{
    c->state = CC_CALIBRATOR_STATE_UPPER_LEFT;
}

CcCalibratorState
cc_calibrator_get_state (CcCalibrator *c)
{
    return c->state;
}

void
cc_calibrator_update_geometry (CcCalibrator *c,
                               int           width,
                               int           height)
{
    if (c->geometry.width == width && c->geometry.height == height)
        return;

    c->geometry.width = width;
    c->geometry.height = height;

    cc_calibrator_reset (c);
};

/* Check whether the coordinates are along the respective axis and
 * return true if the value is within the range, false otherwise.
 */
static gboolean
along_axis (CcCalibrator       *c,
            const Point        *new_point,
            Alignment           alignment,
            const Point        *reference)
{
    gboolean result;

    if (c->threshold_misclick <= 0)
        return TRUE;

    switch (alignment)
    {
    case VERTICAL:
        result = (abs (new_point->x - reference->x) <= c->threshold_misclick) &&
                 (new_point->y > reference->y);
        break;
    case HORIZONTAL:
        result = (abs (new_point->y - reference->y) <= c->threshold_misclick) &&
                 (new_point->x > reference->x);
        break;
    default:
        g_return_val_if_reached (FALSE);
    }

    return result;
}

/* add a click with the given coordinates */
gboolean
cc_calibrator_add_click (CcCalibrator *c,
                         int           x,
                         int           y)
{
    gboolean misclick = TRUE;
    const Point p = { x, y };

    g_return_val_if_fail (c->state < CC_CALIBRATOR_STATE_COMPLETE, FALSE);

    g_debug ("Trying to add click (%d, %d)", x, y);
    /* Double-click detection */
    if (c->threshold_doubleclick > 0 && c->state > CC_CALIBRATOR_STATE_UPPER_LEFT)
    {
        int i = c->state - 1;
        while (i >= 0)
        {
            if (abs(x - c->clicked[i].x) <= c->threshold_doubleclick &&
                abs(y - c->clicked[i].y) <= c->threshold_doubleclick)
            {
                g_debug ("Detected double-click, ignoring");
                return FALSE;
            }
            i--;
        }
    }

    /* Mis-click detection */
    switch (c->state)
    {
    case CC_CALIBRATOR_STATE_UPPER_LEFT:
        misclick = FALSE;
        break;
    case CC_CALIBRATOR_STATE_UPPER_RIGHT: /* must be y-aligned with UL */
        misclick = !along_axis (c, &p, HORIZONTAL, &c->clicked[CC_CALIBRATOR_STATE_UPPER_LEFT]);
        break;
    case CC_CALIBRATOR_STATE_LOWER_LEFT: /* must be x-aligned with UL */
        misclick = !along_axis (c, &p, VERTICAL, &c->clicked[CC_CALIBRATOR_STATE_UPPER_LEFT]);
        break;
    case CC_CALIBRATOR_STATE_LOWER_RIGHT: /* must be x-aligned with UR, y-aligned with LL */
        misclick = !along_axis (c, &p, VERTICAL, &c->clicked[CC_CALIBRATOR_STATE_UPPER_RIGHT]) ||
                   !along_axis (c, &p, HORIZONTAL, &c->clicked[CC_CALIBRATOR_STATE_LOWER_LEFT]);
        break;
    default:
        g_return_val_if_reached (FALSE);
    }

    if (misclick)
    {
        g_debug ("Detected misclick, resetting");
        cc_calibrator_reset (c);
        return FALSE;
    }

    g_debug ("Click (%d, %d) added", x, y);
    c->clicked[c->state++] = p;

    return TRUE;
}

/* calculate and apply the calibration */
gboolean
cc_calibrator_finish (CcCalibrator *c,
                      XYinfo       *new_axis,
                      gboolean      *swap)
{
    gboolean swap_xy;
    float scale_x;
    float scale_y;
    float delta_x;
    float delta_y;
    XYinfo axis = {-1, -1, -1, -1};

    if (c->state != CC_CALIBRATOR_STATE_COMPLETE)
        return FALSE;

    /* Should x and y be swapped? If the device and output are wider
     * towards different axes, swapping must be performed
     *
     * FIXME: Would be even better to know the actual output orientation,
     * not just the direction.
     */
    swap_xy = (c->geometry.width < c->geometry.height);

    /* Compute the scale to transform from pixel positions to [0..1]. */
    scale_x = 1 / (float)c->geometry.width;
    scale_y = 1 / (float)c->geometry.height;

    axis.x_min = ((((c->clicked[CC_CALIBRATOR_STATE_UPPER_LEFT].x + c->clicked[CC_CALIBRATOR_STATE_LOWER_LEFT].x) / 2)) * scale_x);
    axis.x_max = ((((c->clicked[CC_CALIBRATOR_STATE_UPPER_RIGHT].x + c->clicked[CC_CALIBRATOR_STATE_LOWER_RIGHT].x) / 2)) * scale_x);
    axis.y_min = ((((c->clicked[CC_CALIBRATOR_STATE_UPPER_LEFT].y + c->clicked[CC_CALIBRATOR_STATE_UPPER_RIGHT].y) / 2)) * scale_y);
    axis.y_max = ((((c->clicked[CC_CALIBRATOR_STATE_LOWER_LEFT].y + c->clicked[CC_CALIBRATOR_STATE_LOWER_RIGHT].y) / 2)) * scale_y);

    /* Add/subtract the offset that comes from not having the points in the
     * corners (using the same coordinate system they are currently in)
     */
    delta_x = (axis.x_max - axis.x_min) / (float)(NUM_BLOCKS - 2);
    axis.x_min -= delta_x;
    axis.x_max += delta_x;
    delta_y = (axis.y_max - axis.y_min) / (float)(NUM_BLOCKS - 2);
    axis.y_min -= delta_y;
    axis.y_max += delta_y;

    /* If x and y has to be swapped we also have to swap the parameters */
    if (swap_xy)
    {
        SWAP (gdouble, axis.x_min, axis.y_min);
        SWAP (gdouble, axis.x_max, axis.y_max);
    }

    *new_axis = axis;
    *swap = swap_xy;

    return TRUE;
}

