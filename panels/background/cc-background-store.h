/*                                                                              
 * Copyright (C) 2017 jsparber
 *                                                                              
 * This program is free software; you can redistribute it and/or modify         
 * it under the terms of the GNU General Public License as published by         
 * the Free Software Foundation; either version 2 of the License, or            
 * (at your option) any later version.                                          
 *                                                                              
 * This program is distributed in the hope that it will be useful,              
 * but WITHOUT ANY WARRANTY; without even the implied warranty of               
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                
 * GNU General Public License for more details.                                 
 *                                                                              
 * You should have received a copy of the GNU General Public License            
 * along with this program; if not, see <http://www.gnu.org/licenses/>.         
 *                                                                              
 * Author: Julian Sparber <julian@sparber.net>
 *                                                                              
 */   

#ifndef __CC_BACKGROUND_STORE_H__
#define __CC_BACKGROUND_STORE_H__

#include <glib-object.h>
/*
 * Potentially, include other headers on which this header depends.
 */

G_BEGIN_DECLS

/*
 * Type declaration.
 */
#define CC_TYPE_BACKGROUND_STORE (cc_background_store_get_type ())
G_DECLARE_FINAL_TYPE (CcBackgroundStore, cc_background_store, CC, BACKGROUND_STORE, GObject)

/*
 * Method definitions.
 */
CcBackgroundStore *cc_backgroud_store_new (void);

G_END_DECLS

#endif /* __CC_BACKGROUND_STORE_H__ */
