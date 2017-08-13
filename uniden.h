#ifndef SJ_UNIDEN_WIDGET_H
#define SJ_UNIDEN_WIDGET_H

#include <gtk/gtk.h>

enum {
	CHANNELSTORE_CHANNEL,
	CHANNELSTORE_FREQUENCY,
	CHANNELSTORE_TRUNK,
	CHANNELSTORE_DELAY,
	CHANNELSTORE_LOCKOUT,
	CHANNELSTORE_ATTENUATOR,
	CHANNELSTORE_RECORDING,
	CHANNELSTORE_CTCSS_TONE,
	CHANNELSTORE_TAG,
	CHANNELSTORE_HITS,
	CHANNELSTORE_BGCOLOR,
	CHANNELSTORE_ACTIVITY,
	CHANNELSTORE_NUM_COLUMNS
};

typedef struct _SJUnidenWidget			SJUnidenWidget;
typedef struct _SJUnidenWidgetClass		SJUnidenWidgetClass;

struct _SJUnidenWidgetClass
{
	GtkVBoxClass parent_class;
};

extern GType sj_uniden_widget_get_type(void);
extern GtkWidget *sj_uniden_widget_new();
extern GtkListStore *sj_uniden_get_channelstore(SJUnidenWidget *uniden);

extern void frequency_cell_data_function(GtkTreeViewColumn *col,
	GtkCellRenderer *renderer,
	GtkTreeModel *model,
	GtkTreeIter *iter,
	gpointer user_data);
extern void ctcss_cell_data_function(GtkTreeViewColumn *col,
	GtkCellRenderer *renderer,
	GtkTreeModel *model,
	GtkTreeIter *iter,
	gpointer user_data);
extern void sj_uniden_channel_set_frequency(SJUnidenWidget *uniden,
	const gint channel,
	const gint frequency);
extern void sj_uniden_channel_set_tag(SJUnidenWidget *uniden,
	const gint channel,
	const gchar *tag);
extern void sj_uniden_channel_set_lockout(SJUnidenWidget *uniden,
	const gint channel,
	const gboolean lockout);
extern void sj_uniden_channel_set_delay(SJUnidenWidget *uniden,
	const gint channel,
	const gboolean lockout);

#define SJ_UNIDEN_TYPE_WIDGET            (sj_uniden_widget_get_type())
#define SJ_UNIDEN_WIDGET(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), SJ_UNIDEN_TYPE_WIDGET, SJUnidenWidget))
#define SJ_UNIDEN_WIDGET_CLASS(obj)      (G_TYPE_CHECK_CLASS_CAST((obj), SJ_UNIDEN_WIDGET, SJUnidenWidgetClass))
#define SJ_IS_UNIDEN_WIDGET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), SJ_UNIDEN_TYPE_WIDGET))
#define SJ_IS_UNIDEN_WIDGET_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((obj), SJ_UNIDEN_TYPE_WIDGET))
#define SJ_UNIDEN_WIDGET_GET_CLASS       (G_TYPE_INSTANCE_GET_CLASS((obj), SJ_UNIDEN_TYPE_WIDGET, SJUnidenWidgetClass))

#endif /* SJ_UNIDEN_WIDGET_H */
