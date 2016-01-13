
#ifndef ___nma_marshal_MARSHAL_H__
#define ___nma_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* VOID:ENUM (nma-marshal.list:1) */
#define _nma_marshal_VOID__ENUM	g_cclosure_marshal_VOID__ENUM

/* VOID:POINTER,POINTER,STRING,POINTER,UINT,POINTER,POINTER (nma-marshal.list:2) */
extern void _nma_marshal_VOID__POINTER_POINTER_STRING_POINTER_UINT_POINTER_POINTER (GClosure     *closure,
                                                                                    GValue       *return_value,
                                                                                    guint         n_param_values,
                                                                                    const GValue *param_values,
                                                                                    gpointer      invocation_hint,
                                                                                    gpointer      marshal_data);

/* VOID:STRING,BOXED (nma-marshal.list:3) */
extern void _nma_marshal_VOID__STRING_BOXED (GClosure     *closure,
                                             GValue       *return_value,
                                             guint         n_param_values,
                                             const GValue *param_values,
                                             gpointer      invocation_hint,
                                             gpointer      marshal_data);

/* VOID:UINT,STRING,STRING (nma-marshal.list:4) */
extern void _nma_marshal_VOID__UINT_STRING_STRING (GClosure     *closure,
                                                   GValue       *return_value,
                                                   guint         n_param_values,
                                                   const GValue *param_values,
                                                   gpointer      invocation_hint,
                                                   gpointer      marshal_data);

/* VOID:UINT,UINT (nma-marshal.list:5) */
extern void _nma_marshal_VOID__UINT_UINT (GClosure     *closure,
                                          GValue       *return_value,
                                          guint         n_param_values,
                                          const GValue *param_values,
                                          gpointer      invocation_hint,
                                          gpointer      marshal_data);

G_END_DECLS

#endif /* ___nma_marshal_MARSHAL_H__ */

