#include <bonobo-conf/bonobo-property-editor.h>
#include <gtk/gtkrange.h>
#include <gtk/gtksignal.h>
#include <bonobo.h>

static void
changed_cb (GtkAdjustment *adj, BonoboPEditor *editor)
{
	CORBA_Environment ev;
	DynamicAny_DynAny dyn;
	BonoboArg *arg;
	gulong val;

	CORBA_exception_init (&ev);

	val = adj->value;

	dyn = CORBA_ORB_create_basic_dyn_any (bonobo_orb (), TC_ulong, &ev);
	DynamicAny_DynAny_insert_ulong (dyn, val, &ev);

	if (BONOBO_EX (&ev) || dyn == NULL)
		return;

	arg = DynamicAny_DynAny_to_any (dyn, &ev);
	bonobo_peditor_set_value (editor, arg, &ev);

	bonobo_arg_release (arg);
	CORBA_Object_release ((CORBA_Object) dyn, &ev);
	CORBA_exception_free (&ev);
}

static void
adj_set_value_cb (BonoboPEditor     *editor,
		  BonoboArg 	    *value,
		  CORBA_Environment *ev)
{
	GtkAdjustment *adj;
	gulong v;

	adj = gtk_range_get_adjustment (GTK_RANGE (bonobo_peditor_get_widget (editor)));

	if (!bonobo_arg_type_is_equal (value->_type, TC_ulong, NULL))
		return;

	v = BONOBO_ARG_GET_GENERAL (value, TC_ulong, CORBA_unsigned_long, NULL);

	gtk_signal_handler_block_by_func (GTK_OBJECT (adj), changed_cb,
					  editor);
	
	gtk_adjustment_set_value (adj, v);

	gtk_signal_handler_unblock_by_func (GTK_OBJECT (adj), changed_cb,
					    editor);
}

GtkObject* bonobo_peditor_range_construct (GtkWidget *widget)
{
	BonoboPEditor *editor;
	GtkAdjustment *adj;

	g_return_val_if_fail (widget != NULL, NULL);
	g_return_val_if_fail (GTK_IS_RANGE (widget), NULL);

	editor = bonobo_peditor_construct (widget, adj_set_value_cb, TC_ulong); 
	adj = gtk_range_get_adjustment (GTK_RANGE (widget));
	gtk_signal_connect (GTK_OBJECT (adj), "value_changed",
		       	    GTK_SIGNAL_FUNC (changed_cb), editor);

	return GTK_OBJECT (editor);
}
