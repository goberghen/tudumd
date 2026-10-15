/**************************************************************************
 * Copyright (C) 2007-2020 Ruben Pollan Bella <meskio@sindominio.net>     *
 * Copyright (C) 2026 goberghen & Arena.ai Agent (tudumd Markdown fork)   *
 *                                                                        *
 *  This file is part of tudumd (a Markdown-native fork of TuDu).         *
 *                                                                        *
 *  tudumd is free software; you can redistribute it and/or modify        *
 *  it under the terms of the GNU General Public License as published by  *
 *  the Free Software Foundation; version 3 of the License.               *
 *                                                                        *
 *  tudumd is distributed in the hope that it will be useful,             *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *  GNU General Public License for more details.                          *
 *                                                                        *
 *  You should have received a copy of the GNU General Public License     *
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 **************************************************************************/

#include "includes.h"
extern int errno;
#include "interface.h"
#include "screen.h"
#include "data.h"
#include "scheduler.h"
#include "parser.h"
#include "config.h"

#define usage() \
	cout << "Usage: " << argv[0] << " [options]" << endl; \
	cout << "\t-f file\tload tudumd markdown (or legacy xml) file" << endl; \
	cout << "\t-c file\tload specific config file" << endl << endl; \
	cout << "\t-v\tshow version" << endl; \
	cout << "\t-h\tshow this usage message" << endl << endl; \
	cout << "Default task file:   $XDG_DATA_HOME/tudumd/tudu.md (~/.local/share/tudumd/tudu.md)" << endl; \
	cout << "Default config file: $XDG_CONFIG_HOME/tudumd/tudumdrc (~/.config/tudumd/tudumdrc)" << endl;

#define version() \
	cout << VERSION_STR << endl; \
	cout << "Original TuDu Copyright (C) 2007-2020 Ruben Pollan Bella <meskio@sindominio.net>" << endl; \
	cout << "tudumd Markdown fork Copyright (C) 2026 goberghen & Arena.ai Agent" << endl; \
	cout << "tudumd comes with ABSOLUTELY NO WARRANTY; for details type `tudumd -vv'" << endl; \
	cout << "This is free software; you are welcome to redistribute it" << endl; \
	cout << "under certain conditions. Type `tudumd -vv' for details." << endl;

#define copyright() \
	cout << VERSION_STR << endl; \
	cout << "Original TuDu Copyright (C) 2007-2020 Ruben Pollan Bella <meskio@sindominio.net>" << endl; \
	cout << "tudumd Markdown fork Copyright (C) 2026 goberghen & Arena.ai Agent" << endl << endl; \
	cout << "tudumd is free software; you can redistribute it and/or modify" << endl; \
	cout << "it under the terms of the GNU General Public License as published by" << endl; \
	cout << "the Free Software Foundation; version 3 of the License." << endl << endl; \
	cout << "tudumd is distributed in the hope that it will be useful," << endl; \
	cout << "but WITHOUT ANY WARRANTY; without even the implied warranty of" << endl; \
	cout << "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the" << endl; \
	cout << "GNU General Public License for more details." << endl << endl; \
	cout << "You should have received a copy of the GNU General Public License" << endl; \
	cout << "along with this program.  If not, see <http://www.gnu.org/licenses/>." << endl;

/* Recursively create parent directories for a file path if they do not exist */
static void ensure_parent_dirs(const char *file_path)
{
	char dir[4096];
	strncpy(dir, file_path, sizeof(dir) - 1);
	dir[sizeof(dir) - 1] = '\0';
	char *last_slash = strrchr(dir, '/');
	if (!last_slash || last_slash == dir)
		return;
	*last_slash = '\0';
	for (char *p = dir + 1; *p; ++p)
	{
		if (*p == '/')
		{
			*p = '\0';
			mkdir(dir, 0700);
			*p = '/';
		}
	}
	mkdir(dir, 0700);
}

/* return true for delete lock file and false for close tudumd */
bool lock_ask()
{
	cout << endl << "Lock file found" << endl;
	cout << endl << "either" << endl;
	cout << "    Another program may be editing the same file." << endl;
	cout << "    If this is the case, be careful not to end up with two" << endl;
	cout << "    different instances of the same file when making changes." << endl;
	cout << "    Quit, or continue with caution." << endl;
	cout << endl << "or" << endl;
	cout << "    A tudumd session for this file crashed." << endl;
	cout << "    \"Edit anyway\" will destroy the lock file." << endl;
	cout << endl << "[E]dit anyway, [Q]uit:";
	string str;
	cin >> str;

	if ((str[0] == 'E') || (str[0] == 'e'))
		return true;
	else
		return false;
}

int main(int argc, char **argv, char *env[])
{
	int i;
	char file_rc[4096], file_xml[4096], file_lock[4112];
	bool explicit_f = false;

	// Disable Ctrl-C: Otherwise all changes are lost if one slips.
	struct sigaction ignore_ctrl_c;
	ignore_ctrl_c.sa_handler = SIG_IGN;
	ignore_ctrl_c.sa_flags = 0;
	sigaction(SIGINT, &ignore_ctrl_c, NULL);

	const char *home = getenv("HOME");
	if (!home || !home[0])
	{
		for (i = 0; env[i] && strncmp(env[i], "HOME=", 5); ++i);
		home = (env[i] ? env[i] + 5 : "");
	}

	const char *xdg_config = getenv("XDG_CONFIG_HOME");
	const char *xdg_data = getenv("XDG_DATA_HOME");

	Config config;
	bool configErr = !config.load(CONFIG_FILE); // the error will be displayed after check args
	if (configErr)
		configErr = !config.load("/etc/tudumdrc");
	if (configErr)
		configErr = !config.load("/etc/tudurc");

	/* Load legacy ~/.tudurc or ~/.tudumdrc if present, then XDG config
	 * ($XDG_CONFIG_HOME/tudumd/tudumdrc, default ~/.config/tudumd/tudumdrc) */
	snprintf(file_rc, sizeof(file_rc), "%s/.tudurc", home);
	config.load(file_rc);
	snprintf(file_rc, sizeof(file_rc), "%s/.tudumdrc", home);
	config.load(file_rc);

	if (xdg_config && xdg_config[0] == '/')
		snprintf(file_rc, sizeof(file_rc), "%s/tudumd/tudumdrc", xdg_config);
	else
		snprintf(file_rc, sizeof(file_rc), "%s/.config/tudumd/tudumdrc", home);
	config.load(file_rc);

	/*
	 * Parse the command line arguments
	 */
	for (i = 1; i < argc; ++i)
	{
		if (!strncmp("-f",argv[i],2))
		{
			++i;
			if (i < argc) {
				strncpy(file_xml, argv[i], sizeof(file_xml) - 1);
				file_xml[sizeof(file_xml) - 1] = '\0';
				explicit_f = true;
			}
			else
			{
				usage();
				return 0;
			}
		}
		else if (!strncmp("-c",argv[i],2))
		{
			++i;
			if (i < argc)
			{
				if (argv[i][0] != '/')
				{
					char cwd[4096];
					if (!getcwd(cwd, sizeof(cwd)))
						cwd[0] = '\0';
					snprintf(file_rc, sizeof(file_rc), "%s/%s", cwd, argv[i]);
				}
				else
				{
					strncpy(file_rc, argv[i], sizeof(file_rc) - 1);
					file_rc[sizeof(file_rc) - 1] = '\0';
				}
				config.load(file_rc);
			}
			else
			{
				usage();
				return 0;
			}
		}
		else if (!strncmp("-vv",argv[i],3))
		{
			copyright();
			return 0;
		}
		else if (!strncmp("-v",argv[i],2))
		{
			version();
			return 0;
		}
		else if (!strncmp("--help",argv[i],6) ||
				!strncmp("-h",argv[i],2))
		{
			usage();
			return 0;
		}
	}

	if (!explicit_f)
	{
		if (config.getTuduFile() == L"")
		{
			if (xdg_data && xdg_data[0] == '/')
				snprintf(file_xml, sizeof(file_xml), "%s/tudumd/tudu.md", xdg_data);
			else
				snprintf(file_xml, sizeof(file_xml), "%s/.local/share/tudumd/tudu.md", home);
		}
		else
		{
			wcstombs(file_xml, config.getTuduFile().c_str(), sizeof(file_xml) - 1);
			file_xml[sizeof(file_xml) - 1] = '\0';
		}
	}

	if (configErr)
	{
		fprintf(stderr, "Err: Global config does not exist. The config should be %s\n", CONFIG_FILE);
		exit(1);
	}

	/* Ensure the parent directory (e.g. ~/.local/share/tudumd) exists */
	ensure_parent_dirs(file_xml);

	/*
	 * Check and create the lock file
	 */
	strncpy(file_lock, file_xml, sizeof(file_lock) - 16);
	file_lock[sizeof(file_lock) - 16] = '\0';
	for (i = strlen(file_lock); (file_lock[i] != '/') && (i > 0); i--);
	if (file_lock[i] == '/') i++;
	if (file_lock[i] != '.')
	{
		file_lock[i] = '.';
		file_lock[i+1] = '\0';
		strcat(file_lock, file_xml+i);
	}
	strcat(file_lock,"_lock");
	// FIXME: it wont work with NFS
	int lock;
	lock = open(file_lock, O_CREAT|O_EXCL, 00666);
	if (lock == -1)
	{
		if (errno == EEXIST)
		{
			if (!lock_ask()) exit(1);
		}
		/* no rights to write in this folder skip the problem */
		else if (errno != EACCES)
		{
			fprintf(stderr, "Err: I can not create the lock file %s\n", file_lock);
			exit(1);
		}
	}
	else
	{
		close(lock);
	}

	/*
	 * Load data
	 */
	ToDo node;
	iToDo it(node);
	Sched sched;
	bool file_exists = (access(file_xml, F_OK) == 0);
	Parser p(file_xml);
	if (!p.parse(node,sched))
	{
		if (file_exists)
		{
			fprintf(stderr, "Err: Failed to parse %s. Your file was not modified.\n", file_xml);
			unlink(file_lock);
			exit(1);
		}

		/* If the XDG task file does not exist yet on first launch, check if a legacy
		 * ~/.tudu.md or ~/.tudu.xml exists to migrate automatically; otherwise load welcome.md */
		char legacy_md[4096], legacy_xml[4096];
		snprintf(legacy_md, sizeof(legacy_md), "%s/.tudu.md", home);
		snprintf(legacy_xml, sizeof(legacy_xml), "%s/.tudu.xml", home);

		Parser leg_md(legacy_md);
		if (!leg_md.parse(node, sched))
		{
			Parser leg_xml(legacy_xml);
			if (!leg_xml.parse(node, sched))
			{
				Parser welcome(WELCOME_FILE);
				if (!welcome.parse(node, sched))
				{
					Parser sys_welcome("/usr/share/tudumd/welcome.xml");
					if (!sys_welcome.parse(node, sched))
					{
						Parser orig_welcome("/usr/share/tudu/welcome.xml");
						if (!orig_welcome.parse(node, sched))
						{
							fprintf(stderr, "Err: Welcome file does not exist. It should be %s\n", WELCOME_FILE);
							unlink(file_lock);
							exit(1);
						}
					}
				}
			}
		}
	}

	/*
	 * Load and start interface
	 */
	Writer w(file_xml,node);
	Screen screen(config);
	Cmd cmd;
	Interface in(screen,it,sched,config,w,cmd);
	in.main();

	/*
	 * Delete lock file
	 */
	unlink(file_lock);
	return 0;
}
