ACLOCAL_AMFLAGS=-I m4
plugindir=@rofi_PLUGIN_INSTALL_DIR@

plugin_LTLIBRARIES = file_browser.la

file_browser_la_SOURCES=\
		 src/filebrowser.c

file_browser_la_CFLAGS= @glib_CFLAGS@ @rofi_CFLAGS@
file_browser_la_LIBADD= @glib_LIBS@ @rofi_LIBS@
file_browser_la_LDFLAGS= -module -avoid-version
