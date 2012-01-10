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

#define SWAP(x,y)  do { int t; t=(x); x=(y); y=t; } while (0)

/* reset clicks */
void
reset (struct Calib *c)
{
    c->num_clicks = 0;
}

/* add a click with the given coordinates */
gboolean
add_click (struct Calib *c,
           int           x,
           int           y)
{
    /* Double-click detection */
    if (c->threshold_doubleclick > 0 && c->num_clicks > 0)
    {
        int i = c->num_clicks-1;
        while (i >= 0)
        {
            if (abs(x - c->clicked_x[i]) <= c->threshold_doubleclick &&
                abs(y - c->clicked_y[i]) <= c->threshold_doubleclick)
            {
                return FALSE;
            }
            i--;
        }
    }

    /* Mis-click detection */
    if (c->threshold_misclick > 0 && c->num_clicks > 0)
    {
        gboolean misclick = TRUE;

        if (c->num_clicks == 1)
        {
            /* check that along one axis of first point */
            if (along_axis(c, x,c->clicked_x[0],c->clicked_y[0]) ||
                along_axis(c, y,c->clicked_x[0],c->clicked_y[0]))
            {
                misclick = FALSE;
            }
        }
        else if (c->num_clicks == 2)
        {
            /* check that along other axis of first point than second point */
            if ((along_axis(c, y,c->clicked_x[0],c->clicked_y[0]) &&
                 along_axis(c, c->clicked_x[1],c->clicked_x[0],c->clicked_y[0])) ||
                (along_axis(c, x,c->clicked_x[0],c->clicked_y[0]) &&
                 along_axis(c, c->clicked_y[1],c->clicked_x[0],c->clicked_y[0])))
            {
                misclick = FALSE;
            }
        }
        else if (c->num_clicks == 3)
        {
            /* check that along both axis of second and third point */
            if ((along_axis(c, x,c->clicked_x[1],c->clicked_y[1]) &&
                 along_axis(c, y,c->clicked_x[2],c->clicked_y[2])) ||
                (along_axis(c, y,c->clicked_x[1],c->clicked_y[1]) &&
                 along_axis(c, x,c->clicked_x[2],c->clicked_y[2])))
            {
                misclick = FALSE;
            }
        }

        if (misclick)
        {
            reset(c);
            return FALSE;
        }
    }

    c->clicked_x[c->num_clicks] = x;
    c->clicked_y[c->num_clicks] = y;
    c->num_clicks++;

    return TRUE;
}

/* check whether the coordinates are along the respective axis */
gboolean
along_axis (struct Calib *c,
            int           xy,
            int           x0,
            int           y0)
{
    return ((abs(xy - x0) <= c->threshold_misclick) ||
            (abs(xy - y0) <= c->threshold_misclick));
}

/* calculate and apply the calibration */
gboolean
finish (struct Calib *c,
        int           width,
        int           height,
        XYinfo       *new_axys,
        gboolean         *swap)
{
    gboolean swap_xy;
    float scale_x;
    float scale_y;
    int delta_x;
    int delta_y;
    XYinfo axys = {-1, -1, -1, -1};

    if (c->num_clicks != 4)
        return FALSE;

    /* Should x and y be swapped? */
    swap_xy = (abs (c->clicked_x [UL] - c->clicked_x [UR]) < abs (c->clicked_y [UL] - c->clicked_y [UR]));
    if (swap_xy)
    {
        SWAP(c->clicked_x[LL], c->clicked_x[UR]);
        SWAP(c->clicked_y[LL], c->clicked_y[UR]);
    }

    /* Compute min/max coordinates. */
    /* These are scaled using the values of old_axys */
    scale_x = (c->old_axys.x_max - c->old_axys.x_min)/(float)width;
    axys.x_min = ((c->clicked_x[UL] + c->clicked_x[LL]) * scale_x/2) + c->old_axys.x_min;
    axys.x_max = ((c->clicked_x[UR] + c->clicked_x[LR]) * scale_x/2) + c->old_axys.x_min;
    scale_y = (c->old_axys.y_max - c->old_axys.y_min)/(float)height;
    axys.y_min = ((c->clicked_y[UL] + c->clicked_y[UR]) * scale_y/2) + c->old_axys.y_min;
    axys.y_max = ((c->clicked_y[LL] + c->clicked_y[LR]) * scale_y/2) + c->old_axys.y_min;

    /* Add/subtract the offset that comes from not having the points in the
     * corners (using the same coordinate system they are currently in)
     */
    delta_x = (axys.x_max - axys.x_min) / (float)(NUM_BLOCKS - 2);
    axys.x_min -= delta_x;
    axys.x_max += delta_x;
    delta_y = (axys.y_max - axys.y_min) / (float)(NUM_BLOCKS - 2);
    axys.y_min -= delta_y;
    axys.y_max += delta_y;

    /* If x and y has to be swapped we also have to swap the parameters */
    if (swap_xy)
    {
        SWAP(axys.x_min, axys.y_max);
        SWAP(axys.y_min, axys.x_max);
    }

    *new_axys = axys;
    *swap = swap_xy;

    return TRUE;
}

