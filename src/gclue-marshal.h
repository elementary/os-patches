
#ifndef __gclue_marshal_MARSHAL_H__
#define __gclue_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* VOID:UINT,UINT,ULONG,ULONG (./gclue-marshal.list:1) */
extern void gclue_marshal_VOID__UINT_UINT_ULONG_ULONG (GClosure     *closure,
                                                       GValue       *return_value,
                                                       guint         n_param_values,
                                                       const GValue *param_values,
                                                       gpointer      invocation_hint,
                                                       gpointer      marshal_data);

/* VOID:DOUBLE,DOUBLE (./gclue-marshal.list:2) */
extern void gclue_marshal_VOID__DOUBLE_DOUBLE (GClosure     *closure,
                                               GValue       *return_value,
                                               guint         n_param_values,
                                               const GValue *param_values,
                                               gpointer      invocation_hint,
                                               gpointer      marshal_data);

G_END_DECLS

#endif /* __gclue_marshal_MARSHAL_H__ */

