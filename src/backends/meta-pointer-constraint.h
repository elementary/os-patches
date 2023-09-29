/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2015 Red Hat
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by:
 *     Jonas Ådahl <jadahl@gmail.com>
 */

#ifndef META_POINTER_CONSTRAINT_H
#define META_POINTER_CONSTRAINT_H

#include <glib-object.h>

#include "clutter/clutter.h"

G_BEGIN_DECLS

#define META_TYPE_POINTER_CONSTRAINT (meta_pointer_constraint_get_type ())
G_DECLARE_FINAL_TYPE (MetaPointerConstraint, meta_pointer_constraint,
                      META, POINTER_CONSTRAINT, GObject);

MetaPointerConstraint * meta_pointer_constraint_new (const cairo_region_t *region);
cairo_region_t * meta_pointer_constraint_get_region (MetaPointerConstraint *constraint);

#define META_TYPE_POINTER_CONSTRAINT_IMPL (meta_pointer_constraint_impl_get_type ())
G_DECLARE_DERIVABLE_TYPE (MetaPointerConstraintImpl, meta_pointer_constraint_impl,
                          META, POINTER_CONSTRAINT_IMPL, GObject);

/**
 * MetaPointerConstraintImplClass:
 * @constrain: the virtual function pointer for
 *             meta_pointer_constraint_impl_constrain().
 */
struct _MetaPointerConstraintImplClass
{
  GObjectClass parent_class;

  void (* constrain) (MetaPointerConstraintImpl *constraint_impl,
                      ClutterInputDevice        *device,
                      uint32_t                   time,
                      float                      prev_x,
                      float                      prev_y,
                      float                     *x,
                      float                     *y);
  void (* ensure_constrained) (MetaPointerConstraintImpl *constraint_impl,
                               ClutterInputDevice        *device);
};

void meta_pointer_constraint_impl_constrain (MetaPointerConstraintImpl *constraint_impl,
                                             ClutterInputDevice        *device,
                                             uint32_t                   time,
                                             float                      prev_x,
                                             float                      prev_y,
                                             float                     *x,
                                             float                     *y);
void meta_pointer_constraint_impl_ensure_constrained (MetaPointerConstraintImpl *constraint_impl,
                                                      ClutterInputDevice        *device);

G_END_DECLS

#endif /* META_POINTER_CONSTRAINT_H */
