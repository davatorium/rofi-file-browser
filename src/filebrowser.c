/**
 * rofi-file_browser
 *
 * MIT/X11 License
 * Copyright (c) 2017 Qball Cow <qball@gmpclient.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <gmodule.h>

#include <rofi/mode.h>
#include <rofi/helper.h>
#include <rofi/mode-private.h>

#include <stdint.h>

G_MODULE_EXPORT Mode mode;

/**
 * The internal data structure holding the private data of the TEST Mode.
 */
enum FBFileType {
    DIRECTORY,
    RFILE,
};
typedef struct {
    char *name;
    char *path;
    enum FBFileType type;
} FBFile;

typedef struct
{
    char *current_dir;
    FBFile *array;
    unsigned int array_length;
} FileBrowserModePrivateData;

static void free_list ( FileBrowserModePrivateData *pd )
{
    for ( unsigned int i = 0; i < pd->array_length; i++ ) {
        FBFile *fb = & ( pd->array[i] );
        g_free ( fb->name );
        g_free ( fb->path );
    }
    g_free (pd->array);
    pd->array  = NULL;
    pd->array_length = 0;
}
#include <sys/types.h>
#include <dirent.h>

static gint compare ( gconstpointer a, gconstpointer b, gpointer data )
{
    FBFile *fa = (FBFile*)a;
    FBFile *fb = (FBFile*)b;
    if ( fa->type != fb->type ){
        return (fa->type == DIRECTORY)? -1:1;
    }

    return g_strcmp0 ( fa->name, fb->name );
}

static void get_file_browser (  Mode *sw )
{
    FileBrowserModePrivateData *pd = (FileBrowserModePrivateData *) mode_get_private_data ( sw );
    /** 
     * Get the entries to display.
     * this gets called on plugin initialization.
     */
    DIR *dir = opendir ( pd->current_dir );
    if ( dir ) {
        struct dirent *rd = NULL; 
        while ((rd = readdir (dir)) != NULL )
        {
            if ( g_strcmp0 ( rd->d_name, ".." ) == 0 ){
            } else if ( rd->d_name[0] == '.' ) {
                continue;
            }

            switch ( rd->d_type )
            {
                case DT_BLK:
                case DT_CHR:
                case DT_FIFO:
                case DT_UNKNOWN:
                case DT_SOCK:
                    break;
                case DT_REG:
                case DT_DIR:
                    pd->array = g_realloc ( pd->array, (pd->array_length+1)*sizeof(FBFile));
                    pd->array[pd->array_length].name = g_strdup ( rd->d_name );
                    pd->array[pd->array_length].path = g_build_filename ( pd->current_dir, rd->d_name, NULL );
                    pd->array[pd->array_length].type = (rd->d_type == DT_DIR)? DIRECTORY: RFILE;
                    pd->array_length++; 
            }
        }
        closedir ( dir );
    }
    g_qsort_with_data ( pd->array, pd->array_length, sizeof (FBFile ), compare, NULL );
}


static int file_browser_mode_init ( Mode *sw )
{
    /**
     * Called on startup when enabled (in modi list)
     */
    if ( mode_get_private_data ( sw ) == NULL ) {
        FileBrowserModePrivateData *pd = g_malloc0 ( sizeof ( *pd ) );
        mode_set_private_data ( sw, (void *) pd );
        pd->current_dir = g_strdup(g_get_home_dir () );
        // Load content.
        get_file_browser ( sw );
    }
    return TRUE;
}
static unsigned int file_browser_mode_get_num_entries ( const Mode *sw )
{
    const FileBrowserModePrivateData *pd = (const FileBrowserModePrivateData *) mode_get_private_data ( sw );
    return pd->array_length;
}

static ModeMode file_browser_mode_result ( Mode *sw, int mretv, char **input, unsigned int selected_line )
{
    ModeMode           retv  = MODE_EXIT;
    FileBrowserModePrivateData *pd = (FileBrowserModePrivateData *) mode_get_private_data ( sw );
    if ( mretv & MENU_NEXT ) {
        retv = NEXT_DIALOG;
    } else if ( mretv & MENU_PREVIOUS ) {
        retv = PREVIOUS_DIALOG;
    } else if ( mretv & MENU_QUICK_SWITCH ) {
        retv = ( mretv & MENU_LOWER_MASK );
    } else if ( ( mretv & MENU_OK ) ) {
        if ( selected_line < pd->array_length )
        {
            if ( pd->array[selected_line].type == DIRECTORY ) {
                g_free ( pd->current_dir );
                pd->current_dir = pd->array[selected_line].path;
                pd->array[selected_line].path = NULL;
                free_list (pd);
                get_file_browser ( sw );
                return RESET_DIALOG;
            } else if ( pd->array[selected_line].type == RFILE ) {
                char *d = g_strescape ( pd->array[selected_line].path,NULL );
                char *cmd = g_strdup_printf("xdg-open '%s'", d );
                g_free(d);
                helper_execute_command ( pd->current_dir,cmd, FALSE );
                g_free ( cmd ); 
                return MODE_EXIT;
            }
        }
        retv = RELOAD_DIALOG;
    } else if ( ( mretv & MENU_ENTRY_DELETE ) == MENU_ENTRY_DELETE ) {
        retv = RELOAD_DIALOG;
    }
    return retv;
}


static void file_browser_mode_destroy ( Mode *sw )
{
    FileBrowserModePrivateData *pd = (FileBrowserModePrivateData *) mode_get_private_data ( sw );
    if ( pd != NULL ) {
        g_free( pd->current_dir );
        free_list ( pd );
        g_free ( pd );
        mode_set_private_data ( sw, NULL );
    }
}

static char *_get_display_value ( const Mode *sw, unsigned int selected_line, G_GNUC_UNUSED int *state, G_GNUC_UNUSED GList **attr_list, int get_entry )
{
    FileBrowserModePrivateData *pd = (FileBrowserModePrivateData *) mode_get_private_data ( sw );

    // Only return the string if requested, otherwise only set state.
    if ( !get_entry ) return NULL;
    if ( pd->array[selected_line].type == DIRECTORY ){
        return g_strdup_printf ( " %s", pd->array[selected_line].name);
    } else {
        return g_strdup_printf ( " %s", pd->array[selected_line].name);
    }
    return get_entry ? g_strdup(pd->array[selected_line].name): NULL; 
}

/**
 * @param sw The mode object.
 * @param tokens The tokens to match against.
 * @param index  The index in this plugin to match against.
 *
 * Match the entry.
 *
 * @param returns try when a match.
 */
static int file_browser_token_match ( const Mode *sw, GRegex **tokens, unsigned int index )
{
    FileBrowserModePrivateData *pd = (FileBrowserModePrivateData *) mode_get_private_data ( sw );

    // Call default matching function.
    return helper_token_match ( tokens, pd->array[index].name);
}


Mode mode =
{
    .abi_version        = ABI_VERSION,
    .name               = "file_browser",
    .cfg_name_key       = "display-file_browser",
    ._init              = file_browser_mode_init,
    ._get_num_entries   = file_browser_mode_get_num_entries,
    ._result            = file_browser_mode_result,
    ._destroy           = file_browser_mode_destroy,
    ._token_match       = file_browser_token_match,
    ._get_display_value = _get_display_value,
    ._get_message       = NULL,
    ._get_completion    = NULL,
    ._preprocess_input  = NULL,
    .private_data       = NULL,
    .free               = NULL,
};
