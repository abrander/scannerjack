#include <gtk/gtk.h>
#include <string.h>
#include "uniden.h"

static gint
sort_int(GtkTreeModel *model, GtkTreeIter *tia, GtkTreeIter *tib, gpointer userdata)
{
	gint ret;
	gint a = 0;
	gint b = 0;

	gtk_tree_model_get(model, tia, CHANNELSTORE_CHANNEL, &a, -1);
	gtk_tree_model_get(model, tib, CHANNELSTORE_CHANNEL, &b, -1);

	return(a - b);
}

static void
frequency_edited(GtkCellRendererText *renderer, gchar *path_str, gchar *new_text, gpointer user_data)
{
	SJUnidenWidget *uniden = SJ_UNIDEN_WIDGET(user_data);
	GtkListStore *store = sj_uniden_get_channelstore(uniden);
	GtkTreePath *path = gtk_tree_path_new_from_string(path_str);
	GtkTreeIter iter;
	gint channel;
	gint i;
	gint frequency = 0;
	gint frequency_decimal = 0;
	gint point = 0;
	gchar buffer[20];

	gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, path);
	gtk_tree_path_free(path);

	gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
		CHANNELSTORE_CHANNEL, &channel,
		-1);

	for(i = 0; i < strlen(new_text); i++)
	{
		if(g_ascii_isdigit(new_text[i]))
		{
			frequency = frequency * 10 + g_ascii_digit_value(new_text[i]);
			if(point != 0)
				frequency_decimal++;
		}

		if((point == 0) && g_ascii_ispunct(new_text[i]))
			point = i;
		else if((point != 0) && g_ascii_ispunct(new_text[i]))
			break;
	}

	while(frequency_decimal > 4)
	{
		frequency /= 10;
		frequency_decimal--;
	}

	while(frequency_decimal < 4)
	{
		frequency *= 10;
		frequency_decimal++;
	}

	sj_uniden_channel_set_frequency(uniden, channel, frequency);
}

static void
tag_edited(GtkCellRendererText *renderer, gchar *path_str, gchar *new_text, gpointer user_data)
{
	SJUnidenWidget *uniden = SJ_UNIDEN_WIDGET(user_data);
	GtkListStore *store = sj_uniden_get_channelstore(uniden);
	GtkTreePath *path = gtk_tree_path_new_from_string(path_str);
	GtkTreeIter iter;
	gint channel;

	gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, path);
	gtk_tree_path_free(path);

	gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
		CHANNELSTORE_CHANNEL, &channel,
		-1);

	sj_uniden_channel_set_tag(uniden, channel, new_text);
}

static void
lockout_toggled(GtkCellRendererToggle *cell_renderer, gchar *path_str, gpointer user_data)
{
	SJUnidenWidget *uniden = SJ_UNIDEN_WIDGET(user_data);
	GtkListStore *store = sj_uniden_get_channelstore(uniden);
	GtkTreePath *path = gtk_tree_path_new_from_string(path_str);
	GtkTreeIter iter;
	gint channel;
	gboolean lockout;

	gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, path);
	gtk_tree_path_free(path);

	/* Get current state */
	gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
		CHANNELSTORE_CHANNEL, &channel,
		CHANNELSTORE_LOCKOUT, &lockout,
		-1);

	/* Flip the toggle */
	lockout = !lockout;

	sj_uniden_channel_set_lockout(uniden, channel, lockout);
}

static void
delay_toggled(GtkCellRendererToggle *cell_renderer, gchar *path_str, gpointer user_data)
{
	SJUnidenWidget *uniden = SJ_UNIDEN_WIDGET(user_data);
	GtkListStore *store = sj_uniden_get_channelstore(uniden);
	GtkTreePath *path = gtk_tree_path_new_from_string(path_str);
	GtkTreeIter iter;
	gint channel;
	gboolean delay;

	gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, path);
	gtk_tree_path_free(path);

	/* Get current state */
	gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
		CHANNELSTORE_CHANNEL, &channel,
		CHANNELSTORE_DELAY, &delay,
		-1);

	delay = !delay;

	sj_uniden_channel_set_delay(uniden, channel, delay);
}

int
main(int argc, char **argv)
{
	GtkWidget *window;
	GtkWidget *uniden;
	GtkWidget *hbox;
	GtkListStore *store;

	GtkWidget *scroller;
	GtkWidget *view;
	GtkTreeViewColumn *col;
	GtkCellRenderer *renderer;

	gtk_init(&argc, &argv);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(window, "destroy", gtk_main_quit, NULL);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);

	uniden = sj_uniden_widget_new();

	store = sj_uniden_get_channelstore(SJ_UNIDEN_WIDGET(uniden));

	scroller = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));

	gtk_container_add(GTK_CONTAINER(scroller), view);

	/* Channel */
	renderer = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes("Cha", renderer,
		"text", CHANNELSTORE_CHANNEL,
		"cell-background", CHANNELSTORE_BGCOLOR, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
	gtk_tree_view_column_set_sort_column_id(col, CHANNELSTORE_CHANNEL);
	gtk_tree_view_column_set_sizing(col, GTK_TREE_VIEW_COLUMN_GROW_ONLY);

	/* Tag */
	renderer = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes("Tag", renderer,
		"text", CHANNELSTORE_TAG,
		"cell-background", CHANNELSTORE_BGCOLOR, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
	gtk_tree_view_column_set_sort_column_id(col, CHANNELSTORE_TAG);
	g_object_set(renderer, "editable", TRUE, NULL);
	g_signal_connect(renderer, "edited", G_CALLBACK(tag_edited), uniden);

	/* Frequency */

	/* Allocate renderer and column */
	renderer = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new();

	gtk_tree_view_column_set_title(col, "Frequency");
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

	gtk_tree_view_column_pack_start(col, renderer, TRUE);
	gtk_tree_view_column_set_cell_data_func(col, renderer, frequency_cell_data_function, NULL, NULL);
	gtk_tree_view_column_set_attributes(col, renderer,
		"cell-background", CHANNELSTORE_BGCOLOR, NULL);

	g_object_set(renderer, "editable", TRUE, NULL);
	g_signal_connect(renderer, "edited", G_CALLBACK(frequency_edited), uniden);

	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(store), 0, sort_int, NULL, NULL);
	gtk_tree_view_column_set_sort_column_id(col, CHANNELSTORE_FREQUENCY);

	/* L/O */
	renderer = gtk_cell_renderer_toggle_new();

	col = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(col, "L/O");

	g_object_set(renderer, "xalign", 0.0, NULL);
	g_signal_connect(renderer, "toggled", G_CALLBACK(lockout_toggled), uniden);
	gtk_tree_view_column_pack_start(col, renderer, TRUE);
	gtk_tree_view_column_set_attributes(col, renderer,
		"active", CHANNELSTORE_LOCKOUT,
		"cell-background", CHANNELSTORE_BGCOLOR,
		NULL);

	gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
	gtk_tree_view_column_set_sort_column_id(col, CHANNELSTORE_LOCKOUT);

	/* CTCSS */
	renderer = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new();

	gtk_tree_view_column_set_title(col, "CTCSS");
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

	gtk_tree_view_column_pack_start(col, renderer, TRUE);
	gtk_tree_view_column_set_cell_data_func(col, renderer, ctcss_cell_data_function, NULL, NULL);

	gtk_tree_view_column_set_attributes(col, renderer,
		"cell-background", CHANNELSTORE_BGCOLOR, NULL);

	/* Delay */
	renderer = gtk_cell_renderer_toggle_new();

	col = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(col, "Delay");

	g_object_set(renderer, "xalign", 0.0, NULL);
	g_signal_connect(renderer, "toggled", G_CALLBACK(delay_toggled), uniden);
	gtk_tree_view_column_pack_start(col, renderer, TRUE);
	gtk_tree_view_column_set_attributes(col, renderer,
		"active", CHANNELSTORE_DELAY,
		"cell-background", CHANNELSTORE_BGCOLOR,
		NULL);

	gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
	gtk_tree_view_column_set_sort_column_id(col, CHANNELSTORE_LOCKOUT);

	/* Hits */
	renderer = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes("Hits",
		renderer,
		"text", CHANNELSTORE_HITS,
		"cell-background", CHANNELSTORE_BGCOLOR,
		NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
	gtk_tree_view_column_set_sort_column_id(col, CHANNELSTORE_HITS);

	/* Pack it up */
	gtk_box_pack_start(GTK_BOX(hbox), uniden, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), scroller, TRUE, TRUE, 0);

	gtk_container_add(GTK_CONTAINER(window), hbox);

	gtk_widget_show_all(window);
	gtk_main();
}
