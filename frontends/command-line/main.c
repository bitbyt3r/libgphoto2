/* $Id$ */

#include <config.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>

#ifndef WIN32
#include <signal.h>
#endif

#include "main.h"

#include "actions.h"
#include "foreach.h"
#include "interface.h"
#include "options.h"
#include "range.h"
#include "shell.h"

#ifdef HAVE_CDK
#include "gphoto2-cmd-config.h"
#endif

#if HAVE_AA
#include "gphoto2-cmd-capture.h"
#endif

#include "gphoto2-port-info-list.h"
#include "gphoto2-port-log.h"
#include "gphoto2-setting.h"

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif 
#else
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

#define CHECK_RESULT(result) {int r = (result); if (r < 0) return (r);}

/* Takes the current globals, and sets up the gPhoto lib with them */
static int set_globals (void);

/* Command-line option
   -----------------------------------------------------------------------
   this is funky and may look a little scary, but sounded cool to do.
   it makes sense since this is just a wrapper for a functional/flow-based
   library.
   AFTER NOTE: whoah. gnome does something like this. cool deal :)

   When the program starts it calls (in order):
        1) verify_options() to make sure all options are valid,
           and that any options needing an argument have one (or if they
           need an argument, they don't have one).
        2) (optional) option_is_present() to see if a particular option was
           typed in the command-line. This can be used to set up something
           before the next step.
        3) execute_options() to actually parse the command-line and execute
           the command-line options in order.

   This might sound a little complex, but makes adding options REALLY easy,
   and it is very flexible.

   How to add a command-line option:
*/

/* 1) Add a forward-declaration here                                    */
/*    ----------------------------------------------------------------- */
/*    Use the OPTION_CALLBACK(function) macro.                          */

OPTION_CALLBACK(abilities);
#ifdef HAVE_CDK
OPTION_CALLBACK(config);
#endif
OPTION_CALLBACK(help);
OPTION_CALLBACK(version);
OPTION_CALLBACK(shell);
OPTION_CALLBACK(list_cameras);
OPTION_CALLBACK(print_usb_usermap);
OPTION_CALLBACK(auto_detect);
OPTION_CALLBACK(list_ports);
OPTION_CALLBACK(filename);
OPTION_CALLBACK(port);
OPTION_CALLBACK(speed);
OPTION_CALLBACK(model);
OPTION_CALLBACK(quiet);
#ifndef DISABLE_DEBUGGING
OPTION_CALLBACK(debug);
#endif
OPTION_CALLBACK(use_folder);
OPTION_CALLBACK(recurse);
OPTION_CALLBACK(no_recurse);
OPTION_CALLBACK(use_stdout);
OPTION_CALLBACK(use_stdout_size);
OPTION_CALLBACK(list_folders);
OPTION_CALLBACK(list_files);
OPTION_CALLBACK(num_pictures);
OPTION_CALLBACK(get_picture);
OPTION_CALLBACK(get_all_pictures);
OPTION_CALLBACK(get_thumbnail);
OPTION_CALLBACK(get_all_thumbnails);
OPTION_CALLBACK(get_raw_data);
OPTION_CALLBACK(get_all_raw_data);
OPTION_CALLBACK(get_audio_data);
OPTION_CALLBACK(get_all_audio_data);
OPTION_CALLBACK(delete_picture);
OPTION_CALLBACK(delete_all_pictures);
OPTION_CALLBACK(upload_picture);
OPTION_CALLBACK(capture_image);
OPTION_CALLBACK(capture_preview);
OPTION_CALLBACK(capture_movie);
OPTION_CALLBACK(capture_sound);
OPTION_CALLBACK(summary);
OPTION_CALLBACK(manual);
OPTION_CALLBACK(about);
OPTION_CALLBACK(make_dir);
OPTION_CALLBACK(remove_dir);

/* 2) Add an entry in the option table                          */
/*    ----------------------------------------------------------------- */
/*    Format for option is:                                             */
/*     {"short", "long", "argument", "description", callback_function, required}, */
/*    if it is just a flag, set the argument to "".                     */
/*    Order is important! Options are exec'd in the order in the table! */

Option option[] = {

/* Settings needed for formatting output */
#ifndef DISABLE_DEBUGGING
{"",  "debug", "", N_("Turn on debugging"),              debug,         0},
#endif
{"q", "quiet", "", N_("Quiet output (default=verbose)"), quiet,         0},

/* Display and die actions */

{"v", "version",     "", N_("Display version and exit"),     version,        0},
{"h", "help",        "", N_("Displays this help screen"),    help,           0},
{"",  "list-cameras","", N_("List supported camera models"), list_cameras,   0},
{"",  "print-usb-usermap", "", N_("Create output for usb.usermap"), print_usb_usermap, 0},
{"",  "list-ports",  "", N_("List supported port devices"),  list_ports,     0},
{"",  "stdout",      "", N_("Send file to stdout"),          use_stdout,     0},
{"",  "stdout-size", "", N_("Print filesize before data"),   use_stdout_size,0},
{"",  "auto-detect", "", N_("List auto-detected cameras"),   auto_detect,    0},

/* Settings needed for camera functions */
{"" , "port",     "path",     N_("Specify port device"),           port,     0},
{"" , "speed",    "speed",    N_("Specify serial transfer speed"), speed,    0},
{"" , "camera",   "model",    N_("Specify camera model"),          model,    0},
{"" , "filename", "filename", N_("Specify a filename"),            filename, 0},

/* Actions that depend on settings */
{"a", "abilities", "",       N_("Display camera abilities"), abilities,    0},
{"f", "folder",    "folder", N_("Specify camera folder (default=\"/\")"),use_folder,0},
{"R", "recurse", "",  N_("Recursion (default - do not use)"), recurse, 0},
{"", "no-recurse", "",  N_("Turn off recursion"), no_recurse, 0},
{"l", "list-folders",   "", N_("List folders in folder"), list_folders,   0},
{"L", "list-files",     "", N_("List files in folder"),   list_files,     0},
{"m", "mkdir", N_("name"),  N_("Create a directory"),     make_dir,       0},
{"r", "rmdir", N_("name"),  N_("Remove a directory"),     remove_dir,     0},
{"n", "num-images", "",           N_("Display number of pictures"),     num_pictures,   0},
{"p", "get-image", "range",       N_("Get pictures given in range"),    get_picture,    0},
{"P", "get-all-images","",        N_("Get all pictures from folder"),   get_all_pictures,0},
{"t", "get-thumbnail",  "range",  N_("Get thumbnails given in range"),  get_thumbnail,  0},
{"T", "get-all-thumbnails","",    N_("Get all thumbnails from folder"), get_all_thumbnails,0},
{"r", "get-raw-data", "range",    N_("Get raw data given in range"),    get_raw_data, 0},
{"", "get-all-raw-data", "",      N_("Get all raw data from folder"),   get_all_raw_data, 0},
{"", "get-audio-data", "range",   N_("Get audio data given in range"),  get_audio_data, 0},
{"", "get-all-audio-data", "",    N_("Get all audio data from folder"), get_all_audio_data, 0},
{"d", "delete-picture", "range",  N_("Delete pictures given in range"), delete_picture, 0},
{"D", "delete-all-images","",     N_("Delete all pictures in folder"), delete_all_pictures,0},
{"u", "upload-image", "filename", N_("Upload a picture to camera"),    upload_picture, 0},
{"" , "capture-preview", "", N_("Capture a quick preview"), capture_preview, 0},
{"" , "capture-image",  "",  N_("Capture an image"),        capture_image,   0},
{"" , "capture-movie",  "",  N_("Capture a movie "),        capture_movie,   0},
{"" , "capture-sound",  "",  N_("Capture an audio clip"),   capture_sound,   0},
#ifdef HAVE_CDK
{"" , "config",		"",  N_("Configure"),               config,          0},
#endif
{"",  "summary",        "",  N_("Summary of camera status"), summary,        0},
{"",  "manual",         "",  N_("Camera driver manual"),     manual,         0},
{"",  "about",          "",  N_("About the camera driver"),  about,          0},
{"",  "shell",          "",  N_("gPhoto shell"),             shell,          0},

/* End of list                  */
{"" , "", "", "", NULL, 0}
};

/* 3) Add any necessary global variables                                */
/*    ----------------------------------------------------------------- */
/*    Most flags will set options, like choosing a port, camera model,  */
/*    etc...                                                            */

int  glob_option_count;
char glob_port[128];
char glob_model[64];
char glob_folder[1024];
char glob_owd[1024];
char glob_cwd[1024];
int  glob_speed;
int  glob_num=1;

Camera    *glob_camera  = NULL;
GPContext *glob_context = NULL;

int  glob_debug;
int  glob_shell=0;
int  glob_quiet=0;
int  glob_filename_override=0;
int  glob_recurse=1;
char glob_filename[128];
int  glob_stdout=0;
int  glob_stdout_size=0;
static char glob_cancel = 0;

/* 4) Finally, add your callback function.                              */
/*    ----------------------------------------------------------------- */
/*    The callback function is passed "char *arg" to the argument of    */
/*    command-line option. It must return GP_OK or GP_ERROR.            */
/*    Again, use the OPTION_CALLBACK(function) macro.                   */
/*    glob_debug is set if the user types in the "--debug" flag. Use    */
/*    cli_debug_print(char *format, ...) to display debug output. Use   */
/*    cli_error_print(char *format, ...) to display error output.       */


OPTION_CALLBACK(help) {

        cli_debug_print("Displaying usage");

        usage();
        exit(EXIT_SUCCESS);
        return GP_OK;
}

OPTION_CALLBACK(version) {

        cli_debug_print("Displaying version");

        print_version();
        exit(EXIT_SUCCESS);
        return GP_OK;
}

OPTION_CALLBACK(use_stdout) {

        glob_quiet  = 1; /* implied */
        glob_stdout = 1;

        return GP_OK;
}

OPTION_CALLBACK(use_stdout_size)
{
        glob_stdout_size = 1;
        use_stdout(arg);

        return GP_OK;
}

OPTION_CALLBACK (auto_detect)
{
        int x, count;
        CameraList list;
	CameraAbilitiesList *al;
	GPPortInfoList *il;
        const char *name, *value;

	gp_abilities_list_new (&al);
	gp_abilities_list_load (al, glob_context);
	gp_port_info_list_new (&il);
	gp_port_info_list_load (il);
	gp_abilities_list_detect (al, il, &list, glob_context);
	gp_abilities_list_free (al);
	gp_port_info_list_free (il);

        CHECK_RESULT (count = gp_list_count (&list));

        printf(_("%-30s %-16s\n"), _("Model"), _("Port"));
        printf(_("----------------------------------------------------------\n"));
        for (x = 0; x < count; x++) {
                CHECK_RESULT (gp_list_get_name  (&list, x, &name));
                CHECK_RESULT (gp_list_get_value (&list, x, &value));
                printf(_("%-30s %-16s\n"), name, value);
        }

        return GP_OK;
}

OPTION_CALLBACK (abilities)
{
        int x = 0;
	CameraAbilitiesList *al;
        CameraAbilities abilities;

        if (!strlen (glob_model)) {
		cli_error_print (_("You need to specify a camera model"));
                return (GP_ERROR_BAD_PARAMETERS);
        }

	CHECK_RESULT (gp_abilities_list_new (&al));
	CHECK_RESULT (gp_abilities_list_load (al, glob_context));
	CHECK_RESULT (x = gp_abilities_list_lookup_model (al, glob_model));
	CHECK_RESULT (gp_abilities_list_get_abilities (al, x, &abilities));
	CHECK_RESULT (gp_abilities_list_free (al));

        /* Output a parsing friendly abilities table. Split on ":" */
        printf(_("Abilities for camera             : %s\n"),
                abilities.model);
        printf(_("Serial port support              : %s\n"),
		(abilities.port & GP_PORT_SERIAL)? _("yes"):_("no"));
        printf(_("USB support                      : %s\n"),
		(abilities.port & GP_PORT_USB)? _("yes"):_("no"));
        if (abilities.speed[0] != 0) {
        printf(_("Transfer speeds supported        :\n"));
		x = 0;
                do {
        printf(_("                                 : %i\n"), abilities.speed[x]);
                        x++;
                } while (abilities.speed[x]!=0);
        }
        printf(_("Capture choices                  :\n"));
        if (abilities.operations & GP_OPERATION_CAPTURE_IMAGE)
            printf(_("                                 : Image\n"));
        if (abilities.operations & GP_OPERATION_CAPTURE_VIDEO)
            printf(_("                                 : Video\n"));
        if (abilities.operations & GP_OPERATION_CAPTURE_AUDIO)
            printf(_("                                 : Audio\n"));
        if (abilities.operations & GP_OPERATION_CAPTURE_PREVIEW)
            printf(_("                                 : Preview\n"));
        printf(_("Configuration support            : %s\n"),
                abilities.operations & GP_OPERATION_CONFIG? _("yes"):_("no"));
        printf(_("Delete files on camera support   : %s\n"),
                abilities.file_operations & GP_FILE_OPERATION_DELETE? _("yes"):_("no"));
        printf(_("File preview (thumbnail) support : %s\n"),
                abilities.file_operations & GP_FILE_OPERATION_PREVIEW? _("yes"):_("no"));
        printf(_("File upload support              : %s\n"),
                abilities.folder_operations & GP_FOLDER_OPERATION_PUT_FILE? _("yes"):_("no"));

        return (GP_OK);
}


OPTION_CALLBACK(list_cameras) {

        int x, n;
	CameraAbilitiesList *al;
        CameraAbilities a;

        cli_debug_print("Listing Cameras");

	CHECK_RESULT (gp_abilities_list_new (&al));
	CHECK_RESULT (gp_abilities_list_load (al, glob_context));
        CHECK_RESULT (n = gp_abilities_list_count (al));

        if (glob_quiet)
                printf ("%i\n", n);
	else {
                printf (_("Number of supported cameras: %i\n"), n);
	   	printf (_("Supported cameras:\n"));
	}

        for (x = 0; x < n; x++) {
		CHECK_RESULT (gp_abilities_list_get_abilities (al, x, &a));

                if (glob_quiet)
                        printf("%s\n", a.model);
                else
                        switch (a.status) {
                        case GP_DRIVER_STATUS_TESTING:
                                printf (_("\t\"%s\" (TESTING)\n"), a.model);
                                break;
                        case GP_DRIVER_STATUS_EXPERIMENTAL:
                                printf (_("\t\"%s\" (EXPERIMENTAL)\n"), a.model);
                                break;
                        default:
                        case GP_DRIVER_STATUS_PRODUCTION:
                                printf (_("\t\"%s\"\n"), a.model);
                                break;
                        }
        }
	CHECK_RESULT (gp_abilities_list_free (al));

        return (GP_OK);
}

/* print_usb_usermap
 *
 * Print out lines that can be included into usb.usermap 
 * - for all cams supported by our instance of libgphoto2.
 *
 * written after list_cameras 
 */

OPTION_CALLBACK(print_usb_usermap) {

	int x, n;
	CameraAbilitiesList *al;
        CameraAbilities a;

        cli_debug_print("Listing Cameras");

	CHECK_RESULT (gp_abilities_list_new (&al));
	CHECK_RESULT (gp_abilities_list_load (al, glob_context));
	CHECK_RESULT (n = gp_abilities_list_count (al));

        for (x = 0; x < n; x++) {
		CHECK_RESULT (gp_abilities_list_get_abilities (al, x, &a));
		if (a.usb_vendor && a.usb_product) {
			printf (GP_USB_HOTPLUG_SCRIPT "               "
				"0x%04x      0x%04x   0x%04x    0x0000       "
				"0x0000       0x00         0x00            "
				"0x00            0x00            0x00               "
				"0x00               0x00000000\n",
				GP_USB_HOTPLUG_MATCH_VENDOR_ID | GP_USB_HOTPLUG_MATCH_PRODUCT_ID,
				a.usb_vendor, a.usb_product);
		}
        }
	CHECK_RESULT (gp_abilities_list_free (al));

        return (GP_OK);
}

OPTION_CALLBACK(list_ports)
{
	GPPortInfoList *list;
	GPPortInfo info;
        int x, count, result;

	CHECK_RESULT (gp_port_info_list_new (&list));
	result = gp_port_info_list_load (list);
	if (result < 0) {
		gp_port_info_list_free (list);
		return (result);
	}
	count = gp_port_info_list_count (list);
	if (count < 0) {
		gp_port_info_list_free (list);
		return (count);
	}

        if (!glob_quiet) {
          printf(_("Devices found: %i\n"), count);
          printf(_("Path                             Description\n"
                 "--------------------------------------------------------------\n"));
        } else
          printf("%i\n", count);

	/* Now list the ports */
	for (x = 0; x < count; x++) {
		result = gp_port_info_list_get_info (list, x, &info);
		if (result < 0) {
			gp_port_info_list_free (list);
			return (result);
		}
                printf ("%-32s %-32s\n", info.path, info.name);
        }

        return (GP_OK);
}

OPTION_CALLBACK(filename)
{
        cli_debug_print("Setting filename to %s", arg);

        strcpy (glob_filename, arg);
        glob_filename_override = 1;

        return (GP_OK);
}

OPTION_CALLBACK(port)
{
        cli_debug_print("Setting port to %s", arg);

	strcpy (glob_port, arg);

	if (strchr(glob_port, ':') == NULL) {
		/* User didn't specify the port type; try to guess it */

		gp_log (GP_LOG_DEBUG, "main", _("Ports must look like "
		  "'serial:/dev/ttyS0' or 'usb:', but '%s' is missing a colon "
		  "so I am going to guess what you mean."),
		glob_port);

		if (strcmp(arg, "usb") == 0) {
			/* User forgot the colon; add it for him */
			strcpy (glob_port, "usb:");

		} else if (strncmp(arg, "/dev/", 5) == 0) {
			/* Probably a serial port like /dev/ttyS0 */
			strcpy (glob_port, "serial:");
			strncat (glob_port, arg, sizeof(glob_port)-7);

		} else if (strncmp(arg, "/proc/", 6) == 0) {
			/* Probably a USB port like /proc/bus/usb/001 */
			strcpy (glob_port, "usb:");
			strncat (glob_port, arg, sizeof(glob_port)-4);
		}
		cli_debug_print(_("Guessed port name. Using port '%s' from now on."),
				glob_port);
	}

        return (GP_OK);
}

OPTION_CALLBACK(speed)
{
        cli_debug_print("Setting speed to %s", arg);

        glob_speed = atoi (arg);

        return (GP_OK);
}

OPTION_CALLBACK(model)
{
        cli_debug_print("Setting camera model to %s", arg);

        strcpy (glob_model, arg);

        return (GP_OK);
}

#ifndef DISABLE_DEBUGGING
static void
debug_func (GPLogLevel level, const char *domain, const char *format,
	    va_list args, void *data)
{
	if (level == GP_LOG_ERROR)
		fprintf (stderr, _("%s(ERROR): "), domain);
	else
		fprintf (stderr, "%s(%i): ", domain, level);
	vfprintf (stderr, format, args);
	fprintf (stderr, "\n");
}

OPTION_CALLBACK (debug)
{
	glob_debug = GP_DEBUG_HIGH;
	cli_debug_print("ALWAYS INCLUDE THE FOLLOWING LINE WHEN SENDING DEBUG MESSAGES TO THE MAILING LIST:");
	cli_debug_print(PACKAGE " " VERSION ": " "Turning on debug mode");

	gp_log_add_func (GP_LOG_ALL, debug_func, NULL);

	return (GP_OK);
}
#endif

OPTION_CALLBACK(quiet)
{
        cli_debug_print("Setting to quiet mode");

        glob_quiet=1;

        return (GP_OK);
}

OPTION_CALLBACK(shell)
{
        cli_debug_print("Entering shell mode");

        CHECK_RESULT (set_globals ());

        glob_shell = 1;

        return (shell_prompt ());
}

OPTION_CALLBACK (use_folder)
{
        cli_debug_print("Setting folder to %s", arg);

        strcpy (glob_folder, arg);

        return (GP_OK);
}

OPTION_CALLBACK (recurse)
{
        cli_debug_print("Recursion requested, this is now the default");

	fprintf (stderr, _("Recursion is now default and the use of --recurse or -R is now deprecated.\n"));

        return (GP_OK);
}

OPTION_CALLBACK (no_recurse)
{
        cli_debug_print("Clearing recursive mode");

        glob_recurse = 0;

        return (GP_OK);
}

OPTION_CALLBACK (list_folders)
{
        CHECK_RESULT (set_globals ());

        return for_each_subfolder (glob_folder, print_folder, NULL, glob_recurse);
}

#ifdef HAVE_CDK
OPTION_CALLBACK(config)
{
	CHECK_RESULT (set_globals ());

	CHECK_RESULT (gp_cmd_config (glob_camera, glob_context));

	return (GP_OK);
}
#endif

OPTION_CALLBACK (list_files)
{
        CHECK_RESULT (set_globals());
        CHECK_RESULT (for_each_image (glob_folder, print_picture_action, 0));

        if (!glob_recurse)
                return (GP_OK);

        return for_each_subfolder(glob_folder, for_each_image, print_picture_action, glob_recurse);
}

OPTION_CALLBACK (num_pictures)
{
        int count;
        CameraList list;

        cli_debug_print ("Counting pictures");

        CHECK_RESULT (set_globals ());
        CHECK_RESULT (gp_camera_folder_list_files (glob_camera, glob_folder,
                                                   &list, glob_context));
        CHECK_RESULT (count = gp_list_count (&list));

        if (glob_quiet)
                printf ("%i\n", count);
           else
                printf (_("Number of pictures in folder %s: %i\n"),
                        glob_folder, count);

        return (GP_OK);
}

static int
save_camera_file_to_file (CameraFile *file, CameraFileType type)
{
        char ofile[1024], ofolder[1024], buf[1024], c[1024];
        int result;
        char *f;
        const char *name;

        /* Determine the folder and filename */
        strcpy (ofile, "");
        strcpy (ofolder, "");

        if (glob_filename_override) {
                if (strchr (glob_filename, '/')) {              /* Check if they specified an output dir */
                        f = strrchr(glob_filename, '/');
                        strcpy(buf, f+1);                       /* Get the filename */
                        sprintf(ofile, buf, glob_num++);
                        *f = 0;
                        strcpy(ofolder, glob_filename);      /* Get the folder */
                        strcat(ofolder, "/");
                        *f = '/';
                } else {                                        /* If not, subst and set */
                        sprintf(ofile, glob_filename, glob_num++);
                }
        } else {
                gp_file_get_name (file, &name);
		strcat (ofile, name);
		gp_log (GP_LOG_DEBUG, "gphoto2", _("Using filename '%s'..."),
			ofile);
        }

        switch (type) {
        case GP_FILE_TYPE_PREVIEW:
                snprintf (buf, sizeof (buf), "%sthumb_%s", ofolder, ofile);
                break;
        case GP_FILE_TYPE_NORMAL:
		snprintf (buf, sizeof (buf), "%s%s", ofolder, ofile);
                break;
        case GP_FILE_TYPE_RAW:
                snprintf (buf, sizeof (buf), "%sraw_%s", ofolder, ofile);
                break;
	case GP_FILE_TYPE_AUDIO:
		snprintf (buf, sizeof (buf), "%saudio_%s", ofolder, ofile);
		break;
        default:
                return (GP_ERROR_NOT_SUPPORTED);
        }
        if (!glob_quiet) {
                while (GP_SYSTEM_IS_FILE(buf)) {
			if (glob_quiet)
				break;

			do {
				printf(_("File %s exists. Overwrite? [y|n] "),buf);
				fflush(stdout);
				fgets(c, 1023, stdin);
			} while ((c[0]!='y')&&(c[0]!='Y')&&
				 (c[0]!='n')&&(c[0]!='N'));

			if ((c[0]=='y') || (c[0]=='Y'))
				break;


			do { 
				printf(_("Specify new filename? [y|n] "));
				fflush(stdout); 
				fgets(c, 1023, stdin);
			} while ((c[0]!='y')&&(c[0]!='Y')&&
				 (c[0]!='n')&&(c[0]!='N'));

			if (!((c[0]=='y') || (c[0]=='Y')))
				return (GP_OK);

			printf(_("Enter new filename: "));
			fflush(stdout);
			fgets(buf, 1023, stdin);
			buf[strlen(buf)-1]=0;
                }
                printf(_("Saving file as %s\n"), buf);
        }

        if ((result = gp_file_save(file, buf)) != GP_OK)
                cli_error_print(_("Can not save file as %s"), buf);

        return (result);
}

int
save_picture_to_file (const char *folder, const char *filename,
		      CameraFileType type)
{
        int res;

        CameraFile *file;

        CHECK_RESULT (gp_file_new (&file));
        CHECK_RESULT (gp_camera_file_get (glob_camera, folder, filename, type,
                                          file, glob_context));

        if (glob_stdout) {
                const char *data;
                long int size;

                CHECK_RESULT (gp_file_get_data_and_size (file, &data, &size));

                if (glob_stdout_size)
                        printf ("%li\n", size);
                fwrite (data, sizeof(char), size, stdout);
                gp_file_free (file);
                return (GP_OK);
        }

        res = save_camera_file_to_file (file, type);

        gp_file_free (file);

        return (res);
}


/*
  get_picture_common() - parse range, download specified images, or their
        thumbnails according to thumbnail argument, and save to files.
*/

int
get_picture_common(char *arg, CameraFileType type )
{
        cli_debug_print("Getting %s", arg);

        CHECK_RESULT (set_globals ());

        if (strchr(arg, '.'))
                return (save_picture_to_file(glob_folder, arg, type));

        switch (type) {
        case GP_FILE_TYPE_PREVIEW:
                return for_each_image_in_range (glob_folder, arg, save_thumbnail_action, 0);
        case GP_FILE_TYPE_NORMAL:
                return for_each_image_in_range (glob_folder, arg, save_picture_action, 0);
        case GP_FILE_TYPE_RAW:
                return for_each_image_in_range (glob_folder, arg, save_raw_action, 0);
	case GP_FILE_TYPE_AUDIO:
		return for_each_image_in_range (glob_folder, arg, save_audio_action, 0);
        default:
                return (GP_ERROR_NOT_SUPPORTED);
        }
}

OPTION_CALLBACK (get_picture)
{
        return get_picture_common(arg, GP_FILE_TYPE_NORMAL);
}

OPTION_CALLBACK (get_all_pictures)
{
        cli_debug_print("Getting all pictures");

        CHECK_RESULT (set_globals ());
        CHECK_RESULT (for_each_image (glob_folder, save_picture_action, 0));

        if (!glob_recurse)
                return (GP_OK);

        return for_each_subfolder(glob_folder, for_each_image, save_picture_action, glob_recurse);
}

OPTION_CALLBACK (get_thumbnail)
{
        return (get_picture_common(arg, GP_FILE_TYPE_PREVIEW));
}

OPTION_CALLBACK(get_all_thumbnails)
{
        cli_debug_print("Getting all thumbnails");

        CHECK_RESULT (set_globals ());
        CHECK_RESULT (for_each_image (glob_folder, save_thumbnail_action, 0));

        if (!glob_recurse)
                return (GP_OK);

        return for_each_subfolder(glob_folder, for_each_image, save_thumbnail_action, glob_recurse);
}

OPTION_CALLBACK (get_raw_data)
{
        return (get_picture_common(arg, GP_FILE_TYPE_RAW));
}

OPTION_CALLBACK (get_all_raw_data)
{
        CHECK_RESULT (set_globals ());
        CHECK_RESULT (for_each_image (glob_folder, save_raw_action, 0));

        if (!glob_recurse)
                return (GP_OK);

        return for_each_subfolder(glob_folder, for_each_image, save_raw_action, glob_recurse);
}

OPTION_CALLBACK (get_audio_data)
{
	return (get_picture_common (arg, GP_FILE_TYPE_AUDIO));
}

OPTION_CALLBACK (get_all_audio_data)
{
	CHECK_RESULT (set_globals ());
	CHECK_RESULT (for_each_image (glob_folder, save_audio_action, 0));

	if (!glob_recurse)
		return (GP_OK);

	return for_each_subfolder (glob_folder, for_each_image, save_audio_action, glob_recurse);
}

OPTION_CALLBACK (delete_picture)
{
        cli_debug_print("Deleting picture(s) %s", arg);

        CHECK_RESULT (set_globals ());

        return for_each_image_in_range(glob_folder, arg, delete_picture_action, 1);
}

OPTION_CALLBACK (delete_all_pictures)
{
        cli_debug_print("Deleting all pictures");

        CHECK_RESULT (set_globals ());
        CHECK_RESULT (gp_camera_folder_delete_all (glob_camera, glob_folder,
						   glob_context));

        return (GP_OK);
}

OPTION_CALLBACK (upload_picture)
{
        CameraFile *file;
        int res;

        cli_debug_print("Uploading picture");

        CHECK_RESULT (set_globals ());

        CHECK_RESULT (gp_file_new (&file));
	res = gp_file_open (file, arg);
	if (res < 0) {
		gp_file_unref (file);
		return (res);
	}

        res = gp_camera_folder_put_file (glob_camera, glob_folder, file,
					 glob_context);
        gp_file_unref (file);

        return (res);
}

OPTION_CALLBACK (make_dir)
{
	CHECK_RESULT (set_globals ());

	CHECK_RESULT (gp_camera_folder_make_dir (glob_camera,
						 glob_folder, arg, 
						 glob_context));

	return (GP_OK);
}

OPTION_CALLBACK (remove_dir)
{
	CHECK_RESULT (set_globals ());

	CHECK_RESULT (gp_camera_folder_remove_dir (glob_camera,
						   glob_folder, arg,
						   glob_context));

	return (GP_OK);
}

static int
capture_generic (CameraCaptureType type, char *name)
{
        CameraFilePath path;
        char *pathsep;
        int result;

        CHECK_RESULT (set_globals ());

	result =  gp_camera_capture (glob_camera, type, &path, glob_context);
	if (result != GP_OK) {
		cli_error_print("Could not capture.");
		return (result);
	}

	if (strcmp(path.folder, "/") == 0)
		pathsep = "";
	else
		pathsep = "/";

	if (glob_quiet)
		printf ("%s%s%s\n", path.folder, pathsep, path.name);
	else
		printf (_("New file is in location %s%s%s on the camera\n"),
			path.folder, pathsep, path.name);

        return (GP_OK);
}


OPTION_CALLBACK (capture_image)
{
	return (capture_generic (GP_CAPTURE_IMAGE, arg));
}

OPTION_CALLBACK (capture_movie)
{
        return (capture_generic (GP_CAPTURE_MOVIE, arg));
}

OPTION_CALLBACK (capture_sound)
{
        return (capture_generic (GP_CAPTURE_SOUND, arg));
}

OPTION_CALLBACK (capture_preview)
{
	CameraFile *file;
	int result;

	CHECK_RESULT (set_globals ());

	CHECK_RESULT (gp_file_new (&file));
#if HAVE_AA
	result = gp_cmd_capture_preview (glob_camera, file, glob_context);
#else
	result = gp_camera_capture_preview (glob_camera, file, glob_context);
#endif
	if (result < 0) {
		gp_file_free (file);
		return (result);
	}

	result = save_camera_file_to_file (file, GP_FILE_TYPE_NORMAL);
	if (result < 0) {
		gp_file_free (file);
		return (result);
	}

	gp_file_free (file);

	return (GP_OK);
}

OPTION_CALLBACK(summary)
{
        CameraText buf;

        CHECK_RESULT (set_globals ());
        CHECK_RESULT (gp_camera_get_summary (glob_camera, &buf, glob_context));

        printf(_("Camera Summary:\n%s\n"), buf.text);

        return (GP_OK);
}

OPTION_CALLBACK (manual)
{
        CameraText buf;

        CHECK_RESULT (set_globals ());
        CHECK_RESULT (gp_camera_get_manual (glob_camera, &buf, glob_context));

        printf (_("Camera Manual:\n%s\n"), buf.text);

        return (GP_OK);
}

OPTION_CALLBACK (about)
{
        CameraText buf;

        CHECK_RESULT (set_globals ());
        CHECK_RESULT (gp_camera_get_about (glob_camera, &buf, glob_context));

        printf (_("About the library:\n%s\n"), buf.text);

        return (GP_OK);
}

/* Set/init global variables                                    */
/* ------------------------------------------------------------ */

static int
set_globals (void)
{
	CameraList list;
	CameraAbilitiesList *al;
	GPPortInfoList *il;
	GPPortInfo info;
	CameraAbilities abilities;
	int model = -1, port = -1, count;
	const char *name, *value;
	static int initialized = 0;

	if (initialized)
		return (GP_OK);
	initialized = 1;

        /* takes all the settings and sets up the gphoto lib */

        cli_debug_print ("Setting globals...");
        CHECK_RESULT (gp_camera_new (&glob_camera));

	CHECK_RESULT (gp_abilities_list_new (&al));
	CHECK_RESULT (gp_abilities_list_load (al, glob_context));

	CHECK_RESULT (gp_port_info_list_new (&il));
	CHECK_RESULT (gp_port_info_list_load (il));

	/* Model? */
	if (!strcmp ("", glob_model)) {
		CHECK_RESULT (gp_abilities_list_detect (al, il, &list,
							glob_context));
		count = gp_list_count (&list);
		CHECK_RESULT (count);
		if (count == 1) {

			/* Exactly one camera detected */
			CHECK_RESULT (gp_list_get_name (&list, 0, &name));
			CHECK_RESULT (gp_list_get_value (&list, 0, &value));
			model = gp_abilities_list_lookup_model (al, name);
			CHECK_RESULT (model);
			port = gp_port_info_list_lookup_path (il, value);
			CHECK_RESULT (port);

		} else if (!count) {

			/* No camera detected. Have a look at the settings */
			gp_setting_get ("gphoto2", "model", glob_model);
			model = gp_abilities_list_lookup_model (al, glob_model);
			if (model < 0) {
				cli_error_print (_("Please specify a model."));
				return (GP_ERROR_MODEL_NOT_FOUND);
			}
		} else {

			/* More than one camera detected */
/*FIXME: Let the user choose from the list!*/
			CHECK_RESULT (gp_list_get_name (&list, 0, &name));
			CHECK_RESULT (gp_list_get_value (&list, 0, &value));
			model = gp_abilities_list_lookup_model (al, name);
			CHECK_RESULT (model);
			port = gp_port_info_list_lookup_path (il, value);
			CHECK_RESULT (port);
		}
	} else {
		model = gp_abilities_list_lookup_model (al, glob_model);
		CHECK_RESULT (model);
	}
	CHECK_RESULT (gp_abilities_list_get_abilities (al, model, &abilities));
	CHECK_RESULT (gp_camera_set_abilities (glob_camera, abilities));
	gp_setting_set ("gphoto2", "model", abilities.model);

	/* Port? */
	if (strcmp (glob_model, "Directory Browse") && (port < 0)) {
		if (strcmp ("", glob_port)) {
			port = gp_port_info_list_lookup_path (il, glob_port);
			switch (port) {
			case GP_ERROR_UNKNOWN_PORT:
				cli_error_print (_("The port you specified "
					"('%s') can not be found. Please "
					"specify one of the ports found by "
					"'gphoto2 --list-ports' and make "
					"sure the spelling is correct "
					"(i.e. with prefix 'serial:' or "
					"'usb:')."), glob_port);
			default:
				break;
			}
			CHECK_RESULT (port);
		} else {

			/* Let's have a look at the settings */
			gp_setting_get ("gphoto2", "port", glob_port);
			port = gp_port_info_list_lookup_path (il, glob_port);
			if (port < 0) {
				cli_error_print (_("Please specify a port."));
				return (GP_ERROR_UNKNOWN_PORT);
			}
		}
	}

	/* We set the port not in case of "Directory Browse" */
	if (strcmp (glob_model, "Directory Browse")) {
		CHECK_RESULT (gp_port_info_list_get_info (il, port, &info));
		CHECK_RESULT (gp_camera_set_port_info (glob_camera, info));
		gp_setting_set ("gphoto2", "port", info.path);
	}

	gp_abilities_list_free (al);
	gp_port_info_list_free (il);

	/* 
	 * Setting of speed only makes sense for serial ports. gphoto2 
	 * knows that and will complain if we try to set the speed for
	 * ports other than serial ones. Because we are paranoid here and
	 * check every single error message returned by gphoto2, we need
	 * to make sure that we have a serial port.
	 */
	if (!strncmp (glob_port, "serial:", 7))
	        CHECK_RESULT (gp_camera_set_port_speed (glob_camera, glob_speed));

	gp_camera_set_status_func (glob_camera, status_func, NULL);
	gp_camera_set_message_func (glob_camera, message_func, NULL);

        return (GP_OK);
}

static void
ctx_status_func (GPContext *context, const char *format, va_list args,
		 void *data)
{
	vfprintf (stderr, format, args);
	fprintf  (stderr, "\n");
	fflush   (stderr);
}

static void
ctx_error_func (GPContext *context, const char *format, va_list args,
		void *data)
{
	fprintf  (stderr, "\n");
	fprintf  (stderr, _("*** Error ***              \n"));
	vfprintf (stderr, format, args);
	fprintf  (stderr, "\n");
	fflush   (stderr);
}

static float targets[] = {0, 0, 0, 0, 0, 0, 0, 0};

static unsigned int
ctx_progress_start_func (GPContext *context, float target, 
			 const char *format, va_list args, void *data)
{
	unsigned int id;

	for (id = 0; id < 7; id++)
		if (!targets[id])
			break;
	targets[id] = target;

	vfprintf (stderr, format, args);
	fprintf  (stderr, "\n");
	fflush  (stderr);

	return (id);
}

static void
ctx_progress_update_func (GPContext *context, unsigned int id,
			  float current, void *data)
{
	printf ("Percent completed: %02.01f\r", current / targets[id] * 100.);
	fflush(stdout);
}

static void
ctx_progress_stop_func (GPContext *context, unsigned int id, void *data)
{
	targets[id] = 0;
}

static GPContextFeedback
ctx_cancel_func (GPContext *context, void *data)
{
	if (glob_cancel) {
		return (GP_CONTEXT_FEEDBACK_CANCEL);
		glob_cancel = 0;
	} else
		return (GP_CONTEXT_FEEDBACK_OK);
}

static int
init_globals (void)
{
        glob_option_count = 0;

        strcpy (glob_model, "");
        strcpy (glob_port, "");
        strcpy (glob_filename, "gphoto");
        strcpy (glob_folder, "/");
        if (!getcwd (glob_owd, 1023))
                strcpy (glob_owd, "./");
        strcpy (glob_cwd, glob_owd);

        glob_camera = NULL;
        glob_speed = 0;
        glob_debug = 0;
        glob_quiet = 0;
        glob_filename_override = 0;
        glob_recurse = 1;

	glob_context = gp_context_new ();
	gp_context_set_cancel_func    (glob_context, ctx_cancel_func,   NULL);
	gp_context_set_progress_funcs (glob_context,
				       ctx_progress_start_func,
				       ctx_progress_update_func,
				       ctx_progress_stop_func, NULL);
	gp_context_set_error_func     (glob_context, ctx_error_func,    NULL);
	gp_context_set_status_func    (glob_context, ctx_status_func,   NULL);

        return GP_OK;
}

/* Misc functions                                                       */
/* ------------------------------------------------------------------   */

#ifndef DISABLE_DEBUGGING
void
cli_debug_print (char *format, ...)
{
        va_list         pvar;

        if (glob_debug) {
                fprintf(stderr, "cli: ");
                va_start(pvar, format);
                vfprintf(stderr, format, pvar);
                va_end(pvar);
                fprintf(stderr, "\n");
        }
}
#endif

void
cli_error_print (char *format, ...)
{
        va_list         pvar;

        fprintf(stderr, _("ERROR: "));
        va_start(pvar, format);
        vfprintf(stderr, format, pvar);
        va_end(pvar);
        fprintf(stderr, "\n");
}

static void
signal_exit (int signo)
{
	/* If we already were told to cancel, abort. */
	if (glob_cancel) {
		if (!glob_quiet)
			printf (_("\nAborting...\n"));
		if (glob_camera)
			gp_camera_unref (glob_camera);
		if (glob_context)
			gp_context_unref (glob_context);
		if (!glob_quiet)
			printf (_("Aborted.\n"));
		exit (EXIT_FAILURE);
	}

        if (!glob_quiet)
                printf (_("\nCancelling...\n"));

	glob_cancel = 1;

#if 0
        if (strlen (glob_owd) > 0)
                chdir (glob_owd);

        exit (EXIT_SUCCESS);
#endif
}

/* Main :)                                                              */
/* -------------------------------------------------------------------- */

int
main (int argc, char **argv)
{
        int result;

	/* For translation */
	setlocale (LC_ALL, "");
        bindtextdomain (PACKAGE, GPHOTO2_LOCALEDIR);
        textdomain (PACKAGE);

        /* Initialize the globals */
        init_globals ();

        signal (SIGINT, signal_exit);

        /* Count the number of command-line options we have */
        while ((strlen(option[glob_option_count].short_id) > 0) ||
               (strlen(option[glob_option_count].long_id)  > 0)) {
                glob_option_count++;
        }

        /* Peek ahead: Turn on quiet if they gave the flag */
        if (option_is_present ("q", argc, argv) == GP_OK)
                glob_quiet = 1;

        /* Peek ahead: Check to see if we need to turn on debugging output */
        if (option_is_present ("debug", argc, argv) == GP_OK)
                glob_debug = GP_DEBUG_HIGH;

#ifdef OS2 /*check if environment is set otherwise quit*/
        if(CAMLIBS==NULL)
        {
printf(_("gPhoto2 for OS/2 requires you to set the enviroment value CAMLIBS \
to the location of the camera libraries. \
e.g. SET CAMLIBS=C:\\GPHOTO2\\CAM\n"));
                exit(EXIT_FAILURE);
        }
#endif

#ifdef OS2 /*check if environment is set otherwise quit*/
        if(IOLIBS==NULL)
        {
printf(_("gPhoto2 for OS/2 requires you to set the enviroment value IOLIBS \
to the location of the io libraries. \
e.g. SET IOLIBS=C:\\GPHOTO2\\IOLIB\n"));
                exit(EXIT_FAILURE);
        }
#endif

        /* Peek ahead: Make sure we were called correctly */
        if ((argc == 1) || (verify_options (argc, argv) != GP_OK)) {
                if (!glob_quiet)
                        usage ();
                exit (EXIT_FAILURE);
        }

	result = execute_options (argc, argv);
        if (result < 0) {
		switch (result) {
		case GP_ERROR_CANCEL:
			printf (_("Operation cancelled.\n"));
			break;
		default:
			fprintf (stderr, _("*** Error ('%s') ***       \n\n"),
				 gp_result_as_string (result));
#ifndef DISABLE_DEBUGGING
			if (!glob_debug) {
				int n;
				printf (_(
	"For debugging messages, please use the --debug option.\n"
	"Debugging messages may help finding a solution to your problem.\n"
	"If you intend to send any error or debug messages to the gphoto\n"
	"developer mailing list <gphoto-devel@gphoto.org>, please run\n"
	"gphoto2 as follows:\n\n"));

				/*
				 * print the exact command line to assist
				 * l^Husers
				 */
				printf ("    env LANG=C gphoto2 --debug");
				for (n = 1; n < argc; n++) {
					printf(" %s",argv[n]);
				}
				printf ("\n\n");
			}
#endif

			/* Clean up and exit */
			if (glob_camera) {
				gp_camera_unref (glob_camera);
				glob_camera = NULL;
			}
			if (glob_context) {
				gp_context_unref (glob_context);
				glob_context = NULL;
			}
			exit (EXIT_FAILURE);
		}
	}

#ifdef OS2
//       printf(_("\nErrors occuring beyond this point are 'expected' on OS/2\ninvestigation pending\n"));
#endif

	/* Clean up */
	if (glob_camera) {
		gp_camera_unref (glob_camera);
		glob_camera = NULL;
	}
	if (glob_context) {
		gp_context_unref (glob_context);
		glob_context = NULL;
	}

        return (EXIT_SUCCESS);
}

/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
