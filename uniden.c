#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/file.h>
#include "uniden.h"

#define FROM_UNIDEN_BOOL(a) (((a)=='N') ? TRUE : FALSE)
#define TO_UNIDEN_BOOL(a) ((a) ? 'N' : 'F')

#define CSS_GREEN "* { background-color: #afa; }"
#define CSS_RED "* { background-color: #faa; }"

#define _channel_set_iter(uniden, ...) do { \
	g_signal_handler_block(uniden->channel_store, uniden->channel_store_changed_id); \
	gtk_list_store_set(uniden->channel_store, __VA_ARGS__); \
	g_signal_handler_unblock(uniden->channel_store, uniden->channel_store_changed_id); \
} while(0)

/**
 * Convenience macro for getting an iter from the model.
 */
#define _channel_get_iter(uniden, ...) gtk_tree_model_get(GTK_TREE_MODEL(uniden->channel_store), __VA_ARGS__)

static gfloat ctcss_table[] = { 0.0F,
	67.0F, 71.9F, 74.4F, 77.0F, 79.7F, 82.5F, 85.4F, 88.5F, 91.5F, 94.8F,
	97.4F, 100.0F, 103.5F, 107.2F, 110.9F, 114.8F, 118.8F, 123.0F, 127.3F, 131.8F,
	136.5F, 141.3F, 146.2F, 151.4F, 156.7F, 162.2F, 167.9F, 173.8F, 179.9F, 186.2F,
	192.8F, 203.5F, 210.7F, 218.1F, 225.7F, 233.6F, 241.8F, 250.3F, 23.0F, 25.0F,
	26.0F, 31.0F, 32.0F, 36.0F, 43.0F, 47.0F, 51.0F, 53.0F, 54.0F, 65.0F,
	71.0F, 72.0F, 73.0F, 74.0F, 114.0F, 115.0F, 116.0F, 122.0F,	125.0F, 131.0F,
	132.0F, 134.0F, 143.0F, 145.0F, 152.0F, 155.0F, 156.0F, 162.0F, 165.0F, 172.0F,
	174.0F, 205.0F, 212.0F, 223.0F, 225.0F, 226.0F, 243.0F, 244.0F, 245.0F, 246.0F,
	251.0F, 252.0F, 255.0F, 261.0F, 263.0F, 265.0F, 266.0F, 271.0F, 274.0F, 306.0F,
	311.0F, 315.0F, 325.0F, 331.0F, 332.0F, 343.0F, 346.0F, 351.0F, 356.0F, 364.0F,
	365.0F, 371.0F, 411.0F, 412.0F, 413.0F, 423.0F, 431.0F, 432.0F, 446.0F, 446.0F,
	452.0F, 454.0F, 455.0F, 462.0F, 464.0F, 465.0F, 466.0F, 503.0F, 506.0F, 516.0F,
	523.0F, 526.0F, 532.0F, 546.0F, 565.0F, 606.0F, 612.0F, 624.0F, 627.0F, 631.0F,
	632.0F, 654.0F, 662.0F, 664.0F, 702.0F, 712.0F, 723.0F, 731.0F, 732.0F, 734.0F,
	743.0F, 754.0F };
static gint ctcss_table_len = sizeof(ctcss_table) / sizeof(gfloat);

typedef enum {
	CHANNEL_SCAN = 0,
	MANUAL_MODE = 1,
	MANUEL_FREQUENCY_MODE = 8
} UNIDEN_MODE;

struct _SJUnidenWidget
{
	GtkVBox parent;
	UNIDEN_MODE mode;
	gchar *model;

	GtkWidget *channel_label;
	GtkWidget *frequency_event;
	GtkWidget *frequency_label;

	GtkCssProvider *frequency_style;

	gint frequency;
	gint channel;
	gint active_channel;
	gint stepsize;
	gchar *path;
	gint fd;
	struct termios oldtio;
	GIOChannel *io;

	gboolean squelch_open;
	gboolean got_frequency;
	gboolean getting_channels;
	gint getting_channel;
	GtkListStore *channel_store;
	gulong channel_store_changed_id;
	GtkWidget *progress;
};

static void _send_command(SJUnidenWidget *uniden, const gchar *format, ...);
static gboolean has_data(GIOChannel *source, GIOCondition condition, gpointer data);
static void _set_squelch(SJUnidenWidget *uniden, gboolean squelch_open);
static void _channel_find_iter(SJUnidenWidget *uniden, GtkTreeIter *iter, gint channel);
static void button_connect_clicked(GtkButton *button, gpointer user_data);
static void button_scan_clicked(GtkButton *button, gpointer user_data);
static void button_man_clicked(GtkButton *button, gpointer user_data);
static gboolean channel_label_scroll(GtkWidget *widget, GdkEventScroll *event, gpointer user_data);
static gboolean frequency_label_scroll(GtkWidget *widget, GdkEventScroll *event, gpointer user_data);

G_DEFINE_TYPE(SJUnidenWidget, sj_uniden_widget, GTK_TYPE_BOX);

static void
sj_uniden_widget_class_init(SJUnidenWidgetClass *klass)
{
}

/**
 * Check mode and squelch from time to time.
 */
static gboolean
_sync_slow(gpointer data)
{
	SJUnidenWidget *uniden = SJ_UNIDEN_WIDGET(data);

	/* Check mode */
	_send_command(uniden, "MD");

	/* Check squelch */
	_send_command(uniden, "SQ");

	return TRUE;
}

/**
 * Check mode as often as possible.
 */
static gboolean
_sync_fast(gpointer data)
{
	SJUnidenWidget *uniden = SJ_UNIDEN_WIDGET(data);

	switch (uniden->mode)
	{
		case CHANNEL_SCAN:
			/* Get current channel & frequency */
			if (uniden->squelch_open && (uniden->got_frequency == FALSE))
				_send_command(uniden, "MA");

			_send_command(uniden, "SG");
			break;

		case MANUAL_MODE:
		case MANUEL_FREQUENCY_MODE:
			/* Get current frequency */
			_send_command(uniden, "LCD FRQ");
			break;
	}

	return TRUE;
}

/**
 * Upload new channel configuration to radio.
 */
static void
_upload_channel(SJUnidenWidget *uniden, const gint channel)
{
	GtkTreeIter iter;
	UNIDEN_MODE mode = uniden->mode;
	gint frequency, ctcss_tone;
	gboolean lockout, delay;
	gchar *tag;

	_channel_find_iter(uniden, &iter, channel);
	_channel_get_iter(uniden, &iter,
		CHANNELSTORE_FREQUENCY, &frequency,
		CHANNELSTORE_LOCKOUT, &lockout,
		CHANNELSTORE_DELAY, &delay,
		CHANNELSTORE_CTCSS_TONE, &ctcss_tone,
		CHANNELSTORE_TAG, &tag,
		-1);

	/* Set frequency */
	_send_command(uniden, "PM%03d %08d", channel, frequency);

	/* Set lockout */
	_send_command(uniden, "LO%c", TO_UNIDEN_BOOL(lockout));

	/* Set delay */
	_send_command(uniden, "DL%c", TO_UNIDEN_BOOL(delay));

	/* Set ctcss */
	_send_command(uniden, "CS%03d", ctcss_tone);

	/* Set tag */
	if (tag)
	{
		_send_command(uniden, "TA C %03d %s", channel, tag);

		g_free(tag);
	}

	/* Restore mode if needed, setting channel will put the scanner in manual mode */
	if (mode == CHANNEL_SCAN)
		_send_command(uniden, "KEY00");

	/* Read back the new channel-settings */
	_send_command(uniden, "PM%03d", channel);
}

/**
 * Callback to be called when the user has changed a row in the channel list.
 */
static void
channel_row_changed(GtkTreeModel *tree_model, GtkTreePath *path, GtkTreeIter *iter, gpointer user_data)
{
	SJUnidenWidget *uniden = SJ_UNIDEN_WIDGET(user_data);
	gint channel;

	_channel_get_iter(uniden, iter,
		CHANNELSTORE_CHANNEL, &channel,
		-1);

	_upload_channel(uniden, channel);
}

static void
sj_uniden_widget_init(SJUnidenWidget *uniden)
{
	GtkWidget *button = gtk_button_new_with_label("Connect");
	GtkWidget *button_scan = gtk_button_new_with_label("Scan");
	GtkWidget *button_man = gtk_button_new_with_label("Manual");
	GtkWidget *channel_event = gtk_event_box_new();

	gtk_orientable_set_orientation(GTK_ORIENTABLE(uniden), GTK_ORIENTATION_VERTICAL);

	uniden->path = "/dev/ttyUSB0";

	uniden->mode = CHANNEL_SCAN;
	uniden->model = NULL;
	uniden->squelch_open = FALSE;
	uniden->got_frequency = FALSE;
	uniden->io = NULL;
	uniden->frequency = 0;
	uniden->channel = 0;
	uniden->active_channel = 0;
	uniden->stepsize = 1000;
	uniden->getting_channels = TRUE;
	uniden->getting_channel = 1;

	uniden->channel_store = gtk_list_store_new(CHANNELSTORE_NUM_COLUMNS,
		G_TYPE_INT,
		G_TYPE_INT,
		G_TYPE_BOOLEAN,
		G_TYPE_BOOLEAN,
		G_TYPE_BOOLEAN,
		G_TYPE_BOOLEAN,
		G_TYPE_BOOLEAN,
		G_TYPE_INT,
		G_TYPE_STRING,
		G_TYPE_INT,
		G_TYPE_STRING,
		G_TYPE_LONG
		);
	uniden->channel_store_changed_id = g_signal_connect(G_OBJECT(uniden->channel_store), "row_changed", G_CALLBACK(channel_row_changed), uniden);

	uniden->channel_label = gtk_label_new("-");

	uniden->frequency_event = gtk_event_box_new();
	uniden->frequency_label = gtk_label_new("-");

	uniden->progress = gtk_progress_bar_new();

	uniden->frequency_style = gtk_css_provider_new();
	gtk_css_provider_load_from_data(uniden->frequency_style, CSS_RED, -1, NULL);
	gtk_style_context_add_provider(gtk_widget_get_style_context(uniden->frequency_event), GTK_STYLE_PROVIDER(uniden->frequency_style), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	gtk_label_set_use_markup(GTK_LABEL(uniden->frequency_label), TRUE);

	g_timeout_add(125, _sync_fast, uniden);
	g_timeout_add(1000, _sync_slow, uniden);

	gtk_widget_set_events(uniden->frequency_event, GDK_SCROLL_MASK);
	g_signal_connect(G_OBJECT(uniden->frequency_event), "scroll-event", G_CALLBACK(frequency_label_scroll), uniden);

	gtk_widget_set_events(channel_event, GDK_SCROLL_MASK);
	g_signal_connect(G_OBJECT(channel_event), "scroll-event", G_CALLBACK(channel_label_scroll), uniden);

	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(button_connect_clicked), uniden);
	g_signal_connect(G_OBJECT(button_scan), "clicked", G_CALLBACK(button_scan_clicked), uniden);
	g_signal_connect(G_OBJECT(button_man), "clicked", G_CALLBACK(button_man_clicked), uniden);

	gtk_box_pack_start(GTK_BOX (uniden), button, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX (uniden), button_scan, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX (uniden), button_man, FALSE, TRUE, 0);

	gtk_container_add(GTK_CONTAINER (uniden->frequency_event), uniden->frequency_label);
	gtk_container_add(GTK_CONTAINER (channel_event), uniden->channel_label);

	gtk_box_pack_start(GTK_BOX (uniden), channel_event, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX (uniden), uniden->frequency_event, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX (uniden), uniden->progress, FALSE, TRUE, 0);
}

GtkWidget *
sj_uniden_widget_new(void)
{
	return g_object_new(SJ_UNIDEN_TYPE_WIDGET, NULL);
}

GtkListStore *
sj_uniden_get_channelstore(SJUnidenWidget *uniden)
{
	g_return_val_if_fail(SJ_IS_UNIDEN_WIDGET(uniden), NULL);

	return uniden->channel_store;
}

/**
 * Sends a command to the radio unit
 */
static void
_send_command(SJUnidenWidget *uniden, const gchar *format, ...)
{
	g_return_if_fail(SJ_IS_UNIDEN_WIDGET(uniden));

    va_list args;
	gsize written;
	gchar buffer[1024];

    va_start(args, format);

	g_vsnprintf(buffer, sizeof(buffer), format, args);

	printf("\033[33m%s\n", buffer); /* DEBUG */

	if (uniden->io)
	{
		g_io_channel_write_chars(uniden->io, buffer, strlen(buffer), &written, NULL);
		g_io_channel_write_chars(uniden->io, "\r", 1, &written, NULL);
		g_io_channel_flush(uniden->io, NULL);
	}
}

/**
 * Will find an iter matching channel. If we iter is found, one will be
 * added. This can happen if there's traffic before the full channel list
 * is read from the radio.
 */
static void
_channel_find_iter(SJUnidenWidget *uniden, GtkTreeIter *iter, const gint channel)
{
	gboolean exists = FALSE;
	GtkTreeModel *model = GTK_TREE_MODEL(uniden->channel_store);

	gtk_tree_model_get_iter_first(model, iter);

	if (gtk_list_store_iter_is_valid(uniden->channel_store, iter))
	{
		gint store_channel;
		do {
			gtk_tree_model_get(model, iter,
				CHANNELSTORE_CHANNEL, &store_channel,
				-1);

			if (channel == store_channel)
			{
				exists = TRUE;
				break;
			}
		} while (gtk_tree_model_iter_next(model, iter));
	}

	if (exists == FALSE)
		gtk_list_store_append(uniden->channel_store, iter);

	return;
}

/**
 * Update frequency in the GUI.
 */
static void
_update_frequency(SJUnidenWidget *uniden, const gint frequency)
{
	gchar buffer[40];

	if (frequency == uniden->frequency)
		return;

	uniden->frequency = frequency;

	if (uniden->frequency != 0)
	{
		sprintf(buffer, "<big><b>%d.%04d</b></big> MHz", uniden->frequency/10000, uniden->frequency%10000);
		uniden->got_frequency = TRUE;
	}
	else
		sprintf(buffer, "<big><b>-</b></big>");

	gtk_label_set_markup(GTK_LABEL(uniden->frequency_label), buffer);
}

/**
 * Update the hit counter for a channel. This will only be increased if the
 * channel has had continued activity for multiple seconds.
 */
static void
_channel_hit(SJUnidenWidget *uniden, const gint channel)
{
	gint64 now, activity;
	GtkTreeIter iter;
	gint hit;

	now = g_get_real_time();

	_channel_find_iter(uniden, &iter, channel);
	_channel_get_iter(uniden, &iter,
		CHANNELSTORE_HITS, &hit,
		CHANNELSTORE_ACTIVITY, &activity,
		-1);

	if (now > (activity+3))
	{
		hit++;

		_channel_set_iter(uniden, &iter,
			CHANNELSTORE_HITS, hit,
			-1);
	}
}

/**
 * Set an activity time for the channel. This is used to calculate hit
 * in _channel_hit
 */
static void
_channel_set_activity(SJUnidenWidget *uniden, const gint channel)
{
	gint64 now;
	GtkTreeIter iter;

	now = g_get_real_time();

	_channel_find_iter(uniden, &iter, channel);
	_channel_set_iter(uniden, &iter,
		CHANNELSTORE_ACTIVITY, now,
		-1);
}

/**
 * Update the channel label.
 */
static void
_update_channel(SJUnidenWidget *uniden, const gint channel)
{
	GtkTreeIter iter;
	gchar *tag = NULL;
	gchar buffer[40];

	if ((channel != uniden->active_channel))
	{
		if (uniden->active_channel != 0)
		{
			_channel_find_iter(uniden, &iter, uniden->active_channel);
			_channel_set_iter(uniden, &iter,
				CHANNELSTORE_BGCOLOR, NULL,
				-1);
		}

		if (channel != 0)
		{
			_channel_find_iter(uniden, &iter, channel);
			_channel_set_iter(uniden, &iter,
				CHANNELSTORE_BGCOLOR, "#99FF99",
				-1);
		}

		uniden->active_channel = channel;
	}

	/* Do nothing if nothing changes */
	if (channel == uniden->channel)
		return;

	/* Get tag */
	_channel_get_iter(uniden, &iter,
		CHANNELSTORE_TAG, &tag,
		-1);

	uniden->channel = channel;

	if (uniden->channel == 0)
		sprintf(buffer, "-");
	else
	{
		if (tag)
			sprintf(buffer, "%d: %s", uniden->channel, tag);
		else
			sprintf(buffer, "%d", uniden->channel);

		/* Count a hit */
		if (uniden->squelch_open)
		{
			_channel_hit(uniden, channel);
			_channel_set_activity(uniden, channel);
		}
	}

	/* Actually update the label */
	gtk_label_set_label(GTK_LABEL(uniden->channel_label), buffer);
}

static void
_set_squelch(SJUnidenWidget *uniden, gboolean squelch_open)
{
	/* Do nothing if there's no change */
	if (uniden->squelch_open == squelch_open)
	{
		if (uniden->squelch_open)
			_channel_set_activity(uniden, uniden->channel);

		return;
	}

	uniden->squelch_open = squelch_open;

	/* Update labels IF squelch is closed AND in channel scan mode */
	if ((squelch_open == FALSE) && (uniden->mode == CHANNEL_SCAN))
	{
		_update_channel(uniden, 0);
		_update_frequency(uniden, 0);
	}

	/* Change the color of the display according to squelch state */
	if (uniden->squelch_open)
	{
		uniden->got_frequency = FALSE;
		gtk_css_provider_load_from_data(uniden->frequency_style, CSS_GREEN, -1, NULL);
	}
	else
	{
		gtk_css_provider_load_from_data(uniden->frequency_style, CSS_RED, -1, NULL);
	}
}

static void
button_connect_clicked(GtkButton *button, gpointer user_data)
{
	SJUnidenWidget *uniden = SJ_UNIDEN_WIDGET(user_data);
	g_return_if_fail(SJ_IS_UNIDEN_WIDGET(uniden));

	struct termios newtio;

	printf("Connect %s\n", uniden->path);

	uniden->fd = open(uniden->path, O_RDWR | O_NOCTTY | O_NONBLOCK);

	if (uniden->fd<0) return;

	if (flock(uniden->fd, LOCK_EX) < 0) return;

	tcgetattr(uniden->fd,&uniden->oldtio);

	bzero(&newtio, sizeof(newtio));

	/* fix for clang needed for some reason */
	#ifndef CRTSCTS
		#define CRTSCTS	  020000000000
	#endif

	newtio.c_cflag = B19200 | CRTSCTS | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR | ICRNL;
	newtio.c_oflag = 0;
	newtio.c_lflag = ICANON;

	newtio.c_cc[VINTR]    = 0; /* Ctrl-c */
	newtio.c_cc[VQUIT]    = 0; /* Ctrl-\ */
	newtio.c_cc[VERASE]   = 0; /* del */
	newtio.c_cc[VKILL]    = 0; /* @ */
	newtio.c_cc[VEOF]     = 4; /* Ctrl-d */
	newtio.c_cc[VTIME]    = 0; /* inter-character timer unused */
	newtio.c_cc[VMIN]     = 1; /* blocking read until 1 character arrives */
	newtio.c_cc[VSWTC]    = 0; /* '\0' */
	newtio.c_cc[VSTART]   = 0; /* Ctrl-q */
	newtio.c_cc[VSTOP]    = 0; /* Ctrl-s */
	newtio.c_cc[VSUSP]    = 0; /* Ctrl-z */
	newtio.c_cc[VEOL]     = 0; /* '\0' */
	newtio.c_cc[VREPRINT] = 0; /* Ctrl-r */
	newtio.c_cc[VDISCARD] = 0; /* Ctrl-u */
	newtio.c_cc[VWERASE]  = 0; /* Ctrl-w */
	newtio.c_cc[VLNEXT]   = 0; /* Ctrl-v */
	newtio.c_cc[VEOL2]    = 0; /* '\0' */

	tcflush(uniden->fd, TCIFLUSH);
	tcsetattr(uniden->fd, TCSANOW, &newtio);

	uniden->io = g_io_channel_unix_new(uniden->fd);
	g_io_add_watch(uniden->io, G_IO_IN, has_data, uniden);

	/* Start channel download */
	_send_command(uniden, "SI");
	_send_command(uniden, "PM001");
}

static void
button_scan_clicked(GtkButton *button, gpointer user_data)
{
	SJUnidenWidget *uniden = SJ_UNIDEN_WIDGET(user_data);

	_send_command(uniden, "KEY00");
	_update_channel(uniden, 0);
	_update_frequency(uniden, 0);
}

static void
button_man_clicked(GtkButton *button, gpointer user_data)
{
	SJUnidenWidget *uniden = SJ_UNIDEN_WIDGET(user_data);

	_send_command(uniden, "KEY01");
	_send_command(uniden, "LCD FRQ");
	_send_command(uniden, "ST");
}

static gboolean
channel_label_scroll(GtkWidget *widget, GdkEventScroll *event, gpointer user_data)
{
	gint step = 1;

	SJUnidenWidget *uniden = SJ_UNIDEN_WIDGET(user_data);

	button_man_clicked(NULL, uniden);

	switch (event->direction)
	{
		case GDK_SCROLL_UP:
			_send_command(uniden, "MA%03d", uniden->channel + step);
			break;

		case GDK_SCROLL_DOWN:
			_send_command(uniden, "MA%03d", uniden->channel - step);
			break;
	}

	return TRUE;
}

static gboolean
frequency_label_scroll(GtkWidget *widget, GdkEventScroll *event, gpointer user_data)
{
	SJUnidenWidget *uniden = SJ_UNIDEN_WIDGET(user_data);

	gint step = uniden->stepsize;

	button_man_clicked(NULL, uniden);

	if (event->state & (GDK_CONTROL_MASK || GDK_SHIFT_MASK))
		step = 100000;
	else if (event->state & GDK_CONTROL_MASK)
		step = 10000;

	switch (event->direction)
	{
		case GDK_SCROLL_UP:
			_send_command(uniden, "RF%08d", uniden->frequency + step);
			break;

		case GDK_SCROLL_DOWN:
			_send_command(uniden, "RF%08d", uniden->frequency - step);
			break;
	}

	/* Update step-size */
	_send_command(uniden, "ST");

	return TRUE;
}

static gboolean
has_data(GIOChannel *source, GIOCondition condition, gpointer data)
{
	gchar *str;
	gsize len = 0;
	gint i;
	gint channel, frequency;
	gchar *tmp;
	GtkTreeIter iter;
	SJUnidenWidget *uniden = SJ_UNIDEN_WIDGET(data);

	g_io_channel_read_line(uniden->io, &str, &len, NULL, NULL);
	if (!str)
		return TRUE;

	printf("\033[32m%s", str);

	/* Check input */
	for (i=0; i<len; i++)
	{
		if (str[i] == '\r')
			if (i != len)
			{
				printf("Something is wrong. i: %d len: %ld str: %s\n", i, len, str);
				return TRUE;
			}
	}

	if (str[0] == '-')
	{
		_set_squelch(uniden, FALSE);
	}
	else if (str[0] == '+')
		_set_squelch(uniden, TRUE);
	else if ((len>9)
		&& (str[0] == 'T')
		&& (str[1] == 'A')
		&& (str[2] == ' '))
	{
		switch (str[3])
		{
			case 'C':
				str[8] = '\0';
				channel = atoi(str+4);
				tmp = rindex(str+9, '\n');
				if (tmp)
					*tmp = '\0';
				_channel_find_iter(uniden, &iter, channel);
				_channel_set_iter(uniden, &iter,
					CHANNELSTORE_CHANNEL, channel,
					CHANNELSTORE_TAG, str+9,
					-1);
				break;

			default:
				break;
		}
	}
	else if ((len == 35) /* Cxxx - Channel info */
		&& (str[0] == 'C')
		&& g_ascii_isdigit(str[1])
		&& g_ascii_isdigit(str[2])
		&& g_ascii_isdigit(str[3]))
	{
		/* Channel */
		str[4] = '\0';
		channel = atoi(str+1);

		/* Frequency */
		str[14] = '\0';
		frequency = atoi(str+6);

		str[34] = '\0';

		/* Find a iter to save info to */
		_channel_find_iter(uniden, &iter, channel);

		/* Save frequency */
		_channel_set_iter(uniden, &iter,
			CHANNELSTORE_CHANNEL, channel,
			CHANNELSTORE_FREQUENCY, frequency,
			-1);

		if ((uniden->getting_channels == TRUE) && (uniden->getting_channel == channel))
		{
			/* Save everything else if we're getting channels */
			_channel_set_iter(uniden, &iter,
				CHANNELSTORE_CHANNEL, channel,
				CHANNELSTORE_FREQUENCY, frequency,
				CHANNELSTORE_TRUNK, FROM_UNIDEN_BOOL(str[16]),
				CHANNELSTORE_DELAY, FROM_UNIDEN_BOOL(str[19]),
				CHANNELSTORE_LOCKOUT, FROM_UNIDEN_BOOL(str[22]),
				CHANNELSTORE_ATTENUATOR, FROM_UNIDEN_BOOL(str[25]),
				CHANNELSTORE_RECORDING, FROM_UNIDEN_BOOL(str[28]),
				CHANNELSTORE_CTCSS_TONE, atoi(str+31),
				-1);

			uniden->getting_channel++;

			if (uniden->getting_channel <= 500)
			{
				_send_command(uniden, "PM%03d", uniden->getting_channel);
				_send_command(uniden, "TA C %03d", uniden->getting_channel);
			}
		}

		/* Update channel and frequency unless we're in CHANNEL_SCAN and squelch is closed */
		else if (!((uniden->mode == CHANNEL_SCAN) && (uniden->squelch_open == FALSE)))
		{
			_update_channel(uniden, channel);
			_update_frequency(uniden, frequency);
		}
	}
	else if ((len == 5) /* MDxx - Mode */
		&& (str[0] == 'M')
		&& (str[1] == 'D')
		&& g_ascii_isdigit(str[2])
		&& g_ascii_isdigit(str[3]))
	{
		str[4] = '\0';
		uniden->mode = atoi(str+2);
		if (uniden->mode == MANUEL_FREQUENCY_MODE)
			_update_channel(uniden, 0);
	}
	else if ((len == 15) /* SGccc Fxxxxxxxx */
		&& (str[0] == 'S')
		&& g_ascii_isdigit(str[1])
		&& g_ascii_isdigit(str[2])
		&& g_ascii_isdigit(str[3])
		&& g_ascii_isspace(str[4])
		&& (str[5] == 'F'))
	{
		gint s;
		str[4] = '\0';
		str[14] = '\0';
		s = atoi(str+1);
		frequency = atoi(str+6);
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(uniden->progress), s/255.0F);
	}
	else if ((len == 11) /* RFxxxxxxxx - frequency */
		&& (str[0] == 'R')
		&& (str[1] == 'F')
		&& g_ascii_isdigit(str[2])
		&& g_ascii_isdigit(str[3])
		&& g_ascii_isdigit(str[4])
		&& g_ascii_isdigit(str[5])
		&& g_ascii_isdigit(str[6])
		&& g_ascii_isdigit(str[7])
		&& g_ascii_isdigit(str[8])
		&& g_ascii_isdigit(str[9]))
	{
		str[10] = '\0';
		_update_frequency(uniden, atoi(str+2));
	}
	else if ((len == 16) /* FRQ - Display frequency read-out */
		&& (strncmp("FRQ [", str, 5)==0))
	{
		str[9] = '\0';
		str[14] = '\0';
		_update_frequency(uniden, atoi(str+5)*10000 + atoi(str+10));
	}
	else if ((str[0] == 'S') /* ST - Step size */
		&& (str[1] == 'T'))
	{
		str[len-1] = '\0';
		/* 5K / 12.5K / 25K / 50K / 10K / 100K / 7.5K */
		if (strcmp(str, "ST 5K")==0)
			uniden->stepsize = 50;
		else if (strcmp(str, "ST 12.5K")==0)
			uniden->stepsize = 125;
		else if (strcmp(str, "ST 25K")==0)
			uniden->stepsize = 250;
		else if (strcmp(str, "ST 50K")==0)
			uniden->stepsize = 500;
		else if (strcmp(str, "ST 10K")==0)
			uniden->stepsize = 100;
		else if (strcmp(str, "ST 100K")==0)
			uniden->stepsize = 1000;
		else if (strcmp(str, "ST 7.5K")==0)
			uniden->stepsize = 75;
	}
	else if ((str[0] == 'S') /* SI - system information */
		&& (str[1] == 'I'))
	{
		for (i=0; i<len; i++)
		{
			if (str[i] == ',')
			{
				str[i] = '\0';
				uniden->model = g_strdup(str+3);
			}
		}
	}
	else if (!((len == 3)
		&& (str[0] == 'N')
		&& (str[1] == 'G')
		))
		printf("got data[%ld]: %s", len, str);

	g_free(str);

	has_data(source, condition, data);

	return TRUE;
}

void
frequency_cell_data_function(GtkTreeViewColumn *col, GtkCellRenderer *renderer,
	GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
	gint frequency;
	gchar buffer[20];

	gtk_tree_model_get(model, iter, CHANNELSTORE_FREQUENCY, &frequency, -1);

	g_snprintf(buffer, sizeof(buffer), "%d.%04d", frequency/10000, frequency%10000);
	g_object_set(renderer, "text", buffer, NULL);
}

void
ctcss_cell_data_function(GtkTreeViewColumn *col, GtkCellRenderer *renderer,
	GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
	gint ctcss_tone;
	gfloat real_ctcss_tone = 0.0F;
	gchar buffer[20];
	gint i;

	gtk_tree_model_get(model, iter, CHANNELSTORE_CTCSS_TONE, &ctcss_tone, -1);

	if (ctcss_tone < ctcss_table_len)
		real_ctcss_tone = ctcss_table[ctcss_tone];

	g_snprintf(buffer, sizeof(buffer), "%.1f", real_ctcss_tone);
	g_object_set(renderer, "text", buffer, NULL);
}

void
sj_uniden_channel_set_frequency(SJUnidenWidget *uniden, const gint channel, const gint frequency)
{
	GtkTreeIter iter;

	g_return_if_fail(SJ_IS_UNIDEN_WIDGET(uniden));

	_channel_find_iter(uniden, &iter, channel);
	_channel_set_iter(uniden, &iter,
		CHANNELSTORE_FREQUENCY, frequency,
		-1);

	_upload_channel(uniden, channel);
}

void
sj_uniden_channel_set_tag(SJUnidenWidget *uniden, const gint channel, const gchar *tag)
{
	GtkTreeIter iter;
	gchar *oldtag;

	g_return_if_fail(SJ_IS_UNIDEN_WIDGET(uniden));

	_channel_find_iter(uniden, &iter, channel);
	_channel_get_iter(uniden, &iter,
		CHANNELSTORE_TAG, &oldtag,
		-1);

	if ((!g_str_equal(oldtag, tag)) || (oldtag==NULL))
	{
		_channel_set_iter(uniden, &iter,
			CHANNELSTORE_TAG, tag,
			-1);

		_send_command(uniden, "TA C %03d %s", channel, tag);
	}
}

void
sj_uniden_channel_set_lockout(SJUnidenWidget *uniden, const gint channel, const gboolean lockout)
{
	UNIDEN_MODE mode;
	GtkTreeIter iter;
	gboolean oldlockout;

	g_return_if_fail(SJ_IS_UNIDEN_WIDGET(uniden));

	/* Save mode for restoring later */
	mode = uniden->mode;

	/* Find the channel in the store */
	_channel_find_iter(uniden, &iter, channel);

	/* Get current lockout state */
	_channel_get_iter(uniden, &iter,
		CHANNELSTORE_LOCKOUT, &oldlockout,
		-1);

	/* Only update if we actually differ */
	if (oldlockout != lockout)
	{
		_channel_set_iter(uniden, &iter,
			CHANNELSTORE_LOCKOUT, lockout,
			-1);

		/* Change to the affected channel */
		_send_command(uniden, "MA%03d", channel);

		/* Update lockout state */
		_send_command(uniden, "LO%c", TO_UNIDEN_BOOL(lockout));

		/* Restore saved mode */
		if (mode == CHANNEL_SCAN)
			_send_command(uniden, "KEY00");

		_send_command(uniden, "PM%03d", channel);
	}
}

void
sj_uniden_channel_set_delay(SJUnidenWidget *uniden, const gint channel, const gboolean delay)
{
	UNIDEN_MODE mode;
	GtkTreeIter iter;
	gboolean olddelay;

	g_return_if_fail(SJ_IS_UNIDEN_WIDGET(uniden));

	/* Save mode for restoring later */
	mode = uniden->mode;

	/* Find the channel in the store */
	_channel_find_iter(uniden, &iter, channel);

	/* Get current lockout state */
	_channel_get_iter(uniden, &iter,
		CHANNELSTORE_DELAY, &olddelay,
		-1);

	/* Only update if we actually differ */
	if (olddelay != delay)
	{
		_channel_set_iter(uniden, &iter,
			CHANNELSTORE_DELAY, delay,
			-1);

		/* Change to the affected channel */
		_send_command(uniden, "MA%03d", channel);

		/* Update delay state */
		_send_command(uniden, "DL%c", TO_UNIDEN_BOOL(delay));

		/* Restore saved mode */
		if (mode == CHANNEL_SCAN)
			_send_command(uniden, "KEY00");

		/* Read back new channel settings */
		_send_command(uniden, "PM%03d", channel);
	}
}
