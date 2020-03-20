/*      -*- OpenSAF  -*-
 *
 * (C) Copyright 2010 The OpenSAF Foundation
 * (C) Copyright 2017 Ericsson AB - All Rights Reserved.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. This file and program are licensed
 * under the GNU Lesser General Public License Version 2.1, February 1999.
 * The complete license can be accessed from the following location:
 * http://opensource.org/licenses/lgpl-license.php
 * See the Copying file included with the OpenSAF distribution for full
 * licensing terms.
 *
 * Author(s): Wind River Systems
 *
 */
#ifdef HAVE_CONFIG_H
#include "osaf/config.h"
#endif

#include <ctype.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <syslog.h>
#include <errno.h>
#include <signal.h>
#include <pwd.h>
#include <grp.h>
#include <getopt.h>
#include <libgen.h>
#include <unistd.h>
#include <utmp.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/file.h>

#include <dlfcn.h>

#include "osaf/configmake.h"

#include "base/daemon.h"
#include "base/logtrace.h"

#include "base/ncsgl_defs.h"
#include "base/osaf_gcov.h"
#include "base/osaf_secutil.h"
#include "base/osaf_time.h"

#include <sys/types.h>
#include <time.h>

#include <sys/prctl.h>

#define DEFAULT_RUNAS_USERNAME "opensaf"

static const char *internal_version_id_;

static char fifo_file[NAME_MAX];
static char __pidfile[NAME_MAX];
static char __tracefile[NAME_MAX];
static char __runas_username[UT_NAMESIZE];
static unsigned int __tracemask;
static unsigned int __nofork = 0;
static int __logmask;
static int fifo_fd = -1;
static pid_t sending_pid_ = -1;
static uid_t sending_uid_ = -1;


static void install_fatal_signal_handlers(void);

static void __print_usage(const char *progname, FILE *stream, int exit_code)
{
	fprintf(stream, "Usage:  %s [OPTIONS]...\n", progname);
	fprintf(
	    stream,
	    "  -h, --help                  Display this usage information.\n"
	    "  -l, --loglevel[=level]      Set daemon log level, [notice|info|debug].\n"
	    "                              notice is default when nothing specified.\n"
	    "  -m, --tracemask[=mask]      Set daemon TRACE mask.\n"
	    "  -p, --pidfile[=filename]    Set the daemon PID as [filename].\n"
	    "                              Will be stored under " PKGPIDDIR "\n"
	    "  -T, --tracefile[=filename]  Set the daemon TRACE as [filename].\n"
	    "  -U, --run-as[=username]     Execute the daemon as [username].\n"
	    "  -v, --version               Print daemon version.\n");
	exit(exit_code);
}

static int __create_pidfile(const char *pidfile)
{
	FILE *file = NULL;
	int fd, rc = 0;
	char pidfiletmp[NAME_MAX] = {0};
	pid_t pid;

	pid = getpid();
	if (snprintf(pidfiletmp, NAME_MAX, "%s.%u.tmp", pidfile, pid) >= NAME_MAX)
		syslog(LOG_WARNING,"truncation occurred writing pid file: %s", pidfiletmp);

	/* open the file and associate a stream with it */
	if (((fd = open(pidfiletmp, O_RDWR | O_CREAT, 0644)) == -1) ||
	    ((file = fdopen(fd, "r+")) == NULL)) {
		syslog(LOG_ERR, "open failed, pidfiletmp=%s, errno=%s",
		       pidfiletmp, strerror(errno));
		return -1;
	}

	if (!fprintf(file, "%d\n", pid)) {
		syslog(LOG_ERR, "fprintf failed, pidfiletmp=%s, errno=%s",
		       pidfiletmp, strerror(errno));
		fclose(file);

		return -1;
	}
	fflush(file);
	fclose(file);

	int retry_cnt = 0;
	while (link(pidfiletmp, pidfile) != 0) {
		/* don't expect old pid file pre-existed and being used because
		 * we removed all before starting OpenSAF (see opensafd script),
		 * but do 5 retries to unlink and link again.
		 */
		if (errno == EEXIST && retry_cnt == 0) {
			do {
				rc = unlink(pidfile);
				if (retry_cnt > 0) {
					osaf_nanosleep(&kHundredMilliseconds);
				}
			} while ((rc != 0) && (++retry_cnt < 5) &&
				 (errno == EBUSY));
			if (rc != 0) {
				syslog(LOG_ERR,
				       "unlink failed, pidfile=%s, "
				       "error:%s",
				       pidfile, strerror(errno));
				return -1;
			}
		} else {
			syslog(LOG_ERR, "link failed, old=%s new=%s, error:%s",
			       pidfiletmp, pidfile, strerror(errno));
			return -1;
		}
	}

	if (unlink(pidfiletmp) != 0) {
		syslog(LOG_ERR, "unlink failed, pidfiletmp=%s, error:%s",
		       pidfiletmp, strerror(errno));
		return -1;
	}

	return 0;
}

static void create_fifofile(const char *fifofile)
{
	mode_t mask;

	/* Lets Remove any such file if it already exists */
	if (unlink(fifofile) == -1 && errno != ENOENT) {
		syslog(LOG_ERR, "Unable To Delete FIFO Error: %s\n",
		       strerror(errno));
		return;
	}

	mask = umask(0);

	if (mkfifo(fifofile, 0666) == -1) {
		syslog(LOG_ERR, "mkfifo failed: %s %s", fifofile,
				strerror(errno));
		umask(mask);
		return;
	}

	do {
		fifo_fd = open(fifofile, O_NONBLOCK | O_RDONLY);

	} while (fifo_fd == -1 && errno == EINTR);

	if (fifo_fd == -1) {
		syslog(LOG_ERR, "open fifo failed: %s %s", fifofile,
		       strerror(errno));
	}

	umask(mask);
	return;
}

static int level2mask(const char *level)
{
	if (strncmp("notice", level, 6) == 0) {
		return LOG_UPTO(LOG_NOTICE);
	} else if (strncmp("info", level, 4) == 0) {
		return LOG_UPTO(LOG_INFO);
	} else if (strncmp("debug", level, 5) == 0) {
		return LOG_UPTO(LOG_DEBUG);
	} else
		return 0;
}

static void __set_default_options(const char *progname)
{
	/* Set the default option values */
	snprintf(fifo_file, sizeof(fifo_file), PKGLOCALSTATEDIR "/%s.fifo",
		 progname);
	snprintf(__pidfile, sizeof(__pidfile), PKGPIDDIR "/%s.pid", progname);
	snprintf(__tracefile, sizeof(__tracefile), PKGLOGDIR "/%s", progname);
	if (strlen(__runas_username) == 0) {
		char *uname = getenv("OPENSAF_USER");
		if (uname)
			snprintf(__runas_username, sizeof(__runas_username),
				 "%s", uname);
		else
			snprintf(__runas_username, sizeof(__runas_username),
				 DEFAULT_RUNAS_USERNAME);
	}
	__logmask = level2mask("notice");
}

static void __parse_options(int argc, char *argv[])
{
	int next_option;
	const char *progname = basename(argv[0]);

	/* Set the default option values before parsing args */
	__set_default_options(progname);

	/* A string listing valid short options letters */
	const char *const short_options = "hl:m:np:T:U:v";

	/* An array describing valid long options */
	const struct option long_options[] = {
	    {"help", 0, NULL, 'h'},      {"loglevel", 1, NULL, 'l'},
	    {"tracemask", 1, NULL, 'm'}, {"nofork", 0, NULL, 'n'},
	    {"pidfile", 1, NULL, 'p'},   {"tracefile", 1, NULL, 'T'},
	    {"run-as", 1, NULL, 'U'},    {"version", 0, NULL, 'v'},
	    {NULL, 0, NULL, 0} /* Required at end of array */
	};

	do {
		next_option =
		    getopt_long(argc, argv, short_options, long_options, NULL);

		switch (next_option) {
		case 'h': /* -h or --help */
			__print_usage(progname, stdout, EXIT_SUCCESS);
		case 'l': /* -l or --loglevel */
			__logmask = level2mask(optarg);
			break;
		case 'm': /* -m or --tracemask */
			__tracemask = strtoul(optarg, NULL, 0);
			break;
		case 'n': /* -n or --nofork */
			__nofork = 1;
			break;
		case 'p': /* -p or --pidfile */
			snprintf(__pidfile, sizeof(__pidfile), PKGPIDDIR "/%s",
				 optarg);
			break;
		case 'T': /* -T or --tracefile */
			snprintf(__tracefile, sizeof(__tracefile), "%s",
				 optarg);
			break;
		case 'U': /* -U or --run-as */
			snprintf(__runas_username, sizeof(__runas_username),
				 "%s", optarg);
			break;
		case 'v': /* -v or --version */
			/* TODO */
			break;
		case '?': /* The user specified an invalid option */
			__print_usage(progname, stderr, EXIT_FAILURE);
		case EOF: /* Done with options. */
			break;
		default: /* Something else: unexpected */
			abort();
		}
	} while (next_option != EOF);
}

void daemonize(int argc, char *argv[])
{
	struct sched_param param = {0};
	char *thread_prio;
	char *thread_policy;
	int policy = SCHED_OTHER; /*root defaults */
	int prio_val = sched_get_priority_min(policy);
	int i;
	char t_str[256];
	char buf1[256 + sizeof("_SCHED_PRIORITY")] = {0};
	char buf2[256 + sizeof("_SCHED_POLICY")] = {0};

	internal_version_id_ = strdup("@(#) $Id: " INTERNAL_VERSION_ID " $");

	if (argc > 0 && argv != NULL) {
		__parse_options(argc, argv);
		openlog(basename(argv[0]), LOG_PID, LOG_LOCAL0);
	} else {
		syslog(LOG_ERR,
		       "invalid top argc/argv[] passed to daemonize()");
		exit(EXIT_FAILURE);
	}

	if ((!strncmp("osafamfwd", basename(argv[0]), 9)) ||
	    (!strncmp("osafamfnd", basename(argv[0]), 9))) {
		policy = SCHED_RR;
		prio_val = sched_get_priority_min(policy);
	}

	strcpy(t_str, basename(argv[0]));
	for (i = 0; i < strlen(t_str); i++)
		t_str[i] = toupper(t_str[i]);

	sprintf(buf1, "%s%s", t_str, "_SCHED_PRIORITY");
	sprintf(buf2, "%s%s", t_str, "_SCHED_POLICY");

	/* Process scheduling class */

	if ((thread_prio = getenv(buf1)) != NULL)
		prio_val = strtol(thread_prio, NULL, 0);

	if ((thread_policy = getenv(buf2)) != NULL)
		policy = strtol(thread_policy, NULL, 0);

	param.sched_priority = prio_val;
	int result = sched_setscheduler(0, policy, &param);
	if (result == -1 && errno == EPERM && policy != SCHED_OTHER) {
		LOG_WA("sched_setscheduler failed with EPERM, falling back to "
		       "SCHED_OTHER policy for %s",
		       basename(argv[0]));
		policy = SCHED_OTHER;
		prio_val = sched_get_priority_min(policy);
		param.sched_priority = prio_val;
		result = sched_setscheduler(0, policy, &param);
	}
	if (result == -1) {
		syslog(LOG_ERR, "Could not set scheduling class for %s",
		       strerror(errno));
		if ((!strncmp("osafamfwd", basename(argv[0]), 9)) ||
		    (!strncmp("osafamfnd", basename(argv[0]), 9))) {
			policy = SCHED_RR;
			param.sched_priority = sched_get_priority_min(policy);
			syslog(LOG_INFO, "setting to default values");
			if (sched_setscheduler(0, policy, &param) == -1)
				syslog(LOG_ERR,
				       "Could not set scheduling class for %s",
				       strerror(errno));
		}
	}

	if (__nofork) {
		syslog(LOG_WARNING, "Started without fork");
	} else {

		/* Already a daemon */
		if (getppid() == 1)
			return;

		/* Fork off the parent process */
		pid_t pid = fork();
		if (pid < 0) {
			syslog(LOG_ERR, "fork daemon failed, pid=%d (%s)", pid,
			       strerror(errno));
			exit(EXIT_FAILURE);
		}

		/* If we got a good PID, then we can exit the parent process */
		if (pid > 0)
			exit(EXIT_SUCCESS);

		/* Create a new SID for the child process */
		pid_t sid = setsid();
		if (sid < 0) {
			syslog(LOG_ERR,
			       "create new session failed, sid=%d (%s)", sid,
			       strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	/* Redirect standard files to /dev/null */
	if (freopen("/dev/null", "r", stdin) == NULL)
		syslog(LOG_ERR, "freopen stdin failed - %s", strerror(errno));
	if (freopen("/dev/null", "w", stdout) == NULL)
		syslog(LOG_ERR, "freopen stdout failed - %s", strerror(errno));
	if (freopen("/dev/null", "w", stderr) == NULL)
		syslog(LOG_ERR, "freopen stderr failed - %s", strerror(errno));

	/* Change the file mode mask to 0644 */
	umask(022);

	/*
	 * Change the current working directory.  This prevents the current
	 * directory from being locked; hence not being able to remove it.
	 */
	if (chdir("/") < 0) {
		syslog(LOG_ERR, "unable to change directory, dir=%s (%s)", "/",
		       strerror(errno));
		exit(EXIT_FAILURE);
	}

	create_fifofile(fifo_file);

	/* Enable code coverage logging. (if --enable-gcov in configure) */
	create_gcov_flush_thread();

	/* Create the process PID file */
	if (__create_pidfile(__pidfile) != 0)
		exit(EXIT_FAILURE);

	/* Cancel certain signals */
	signal(SIGCHLD, SIG_DFL); /* A child process dies */
	signal(SIGTSTP, SIG_IGN); /* Various TTY signals */
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTERM, SIG_DFL); /* Die on SIGTERM */
	signal(SIGHUP, SIG_IGN);

/* RUNASROOT gives the OpenSAF user a possibility to maintain the 4.1 behaviour
 * should eventually be removed.
 */
#ifndef RUNASROOT
	/* Drop privileges to user if there is one, and we were run as root */
	if (getuid() == 0 || geteuid() == 0) {
		long bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
		char *buffer = (char *)malloc(bufsize >= 0 ? bufsize : 16384);
		struct passwd pwd;
		struct passwd *pw;
		if (buffer != NULL &&
		    getpwnam_r(__runas_username, &pwd, buffer, bufsize, &pw) ==
			0 &&
		    pw != NULL) {
			/* supplementary groups */
			int ngroups = 1;
			gid_t gid;
			osaf_getgrouplist(__runas_username, pw->pw_gid, &gid,
					  &ngroups);
			gid_t *group_ids =
			    (gid_t *)malloc(ngroups * sizeof(gid_t));
			if (group_ids != NULL) {
				if (osaf_getgrouplist(__runas_username,
						      pw->pw_gid, group_ids,
						      &ngroups) != -1) {
					// TODO: setgroups() is non POSIX, fix
					// it later
					if (setgroups(ngroups, group_ids)) {
						syslog(
						    LOG_INFO,
						    "setgroups failed, uid=%d (%s). Continuing without supplementary groups.",
						    pw->pw_uid,
						    strerror(errno));
					}
				} else {
					syslog(
					    LOG_INFO,
					    "getgrouplist failed, uid=%d (%s). Continuing without supplementary groups.",
					    pw->pw_uid, strerror(errno));
				}
				free(group_ids);
			} else {
				syslog(
				    LOG_INFO,
				    "getgrouplist failed, uid=%d (%s). Continuing without supplementary groups.",
				    pw->pw_uid, strerror(errno));
			}
			if ((pw->pw_uid > 0) && (pw->pw_gid > 0)) {
				assert(chown(fifo_file, pw->pw_uid, pw->pw_gid) == 0);
				assert(chmod(fifo_file, 0660) == 0);
				assert(chown(__pidfile, pw->pw_uid, pw->pw_gid) == 0);
				assert(chmod(__pidfile, 0640) == 0);
			}
			if ((pw->pw_gid > 0) && (setgid(pw->pw_gid) < 0)) {
				syslog(LOG_ERR, "setgid failed, gid=%d (%s)",
				       pw->pw_gid, strerror(errno));
				free(buffer);
				exit(EXIT_FAILURE);
			}
			if ((pw->pw_uid > 0) && (setuid(pw->pw_uid) < 0)) {
				syslog(LOG_ERR, "setuid failed, uid=%d (%s)",
				       pw->pw_uid, strerror(errno));
				free(buffer);
				exit(EXIT_FAILURE);
			}
			// Enable generating core files
			int (*plibc_prctl)(int option, ...) =
			    dlsym(RTLD_DEFAULT, "prctl");
			if (plibc_prctl) {
				if (plibc_prctl(PR_SET_DUMPABLE, 1) < 0) {
					syslog(LOG_ERR, "prctl failed: %s",
					       strerror(errno));
				}
			}
		} else {
			syslog(LOG_ERR, "invalid user name %s",
			       __runas_username);
			free(buffer);
			exit(EXIT_FAILURE);
		}
		free(buffer);
	}
#endif

	/* install fatal signal handlers for writing backtrace to file */
	install_fatal_signal_handlers();

	/* Initialize the log/trace interface */
	if (logtrace_init_daemon(basename(argv[0]), __tracefile, __tracemask,
				 __logmask) != 0)
		exit(EXIT_FAILURE);

	syslog(LOG_NOTICE, "Started");
}

void daemonize_as_user(const char *username, int argc, char *argv[])
{
	strncpy(__runas_username, username, sizeof(__runas_username));
	__runas_username[sizeof(__runas_username) - 1] = '\0';
	daemonize(argc, argv);
}

static NCS_SEL_OBJ term_sel_obj; /* Selection object for TERM signal events */
static NCS_SEL_OBJ hangup_sel_obj; /* Selection object for HUP signal */

// symbols from ncs_osprm.h, don't pull in that file here!
extern uint32_t ncs_sel_obj_create(NCS_SEL_OBJ *o_sel_obj);
extern uint32_t ncs_sel_obj_ind(NCS_SEL_OBJ *i_ind_obj);

/**
 * TERM signal handler
 * @param sig
 */
static void sigterm_handler(int signum, siginfo_t *info, void *ptr)
{
	sending_pid_ = info->si_pid;
	sending_uid_ = info->si_uid;

	ncs_sel_obj_ind(&term_sel_obj);
	signal(SIGTERM, SIG_IGN);
}

/**
 * HUP signal handler
 * @param sig
 */
static void sighup_handler(int signum, siginfo_t *info, void *ptr)
{
	(void)signum;
	(void)info;
	(void)ptr;
	ncs_sel_obj_ind(&hangup_sel_obj);
}

/**
 * Exit calling process with exit(0) using a standard syslog message.
 * This function should be called from the main thread of a server process in
 * a "safe" context for calling exit(). Any service specific thing should be
 * cleaned up before calling this function. By calling exit(), registered exit
 * functions are called before the process is terminated.
 */
void daemon_exit(void)
{
	syslog(LOG_NOTICE, "exiting for shutdown, (sigterm from pid %d uid %d)",
		sending_pid_, sending_uid_);

	close(fifo_fd);

	/* Lets remove any such file if it already exists */
	unlink(fifo_file);
	unlink(__pidfile);

	/* flush the logtrace */
	logtrace_exit_daemon();

	_Exit(0);
}

/**
 * Install TERM signal handler and return descriptor to monitor
 * @param[out] term_fd  socket descriptor to monitor for SIGTERM event
 */
void daemon_sigterm_install(int *term_fd)
{
	struct sigaction act;

	if (ncs_sel_obj_create(&term_sel_obj) != NCSCC_RC_SUCCESS) {
		syslog(LOG_ERR, "ncs_sel_obj_create failed");
		exit(EXIT_FAILURE);
	}

	sigemptyset(&act.sa_mask);
	act.sa_sigaction = sigterm_handler;
	act.sa_flags = SA_SIGINFO | SA_RESETHAND;
	if (sigaction(SIGTERM, &act, NULL) < 0) {
		syslog(LOG_ERR, "sigaction TERM failed: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	*term_fd = term_sel_obj.rmv_obj;
}

/**
 * Install HUP signal handler and return descriptor to monitor
 * @param[out] term_fd  socket descriptor to monitor for SIGHUP event
 */
NCS_SEL_OBJ* daemon_sighup_install(int *hangup_fd)
{
	struct sigaction act;

	if (ncs_sel_obj_create(&hangup_sel_obj) != NCSCC_RC_SUCCESS) {
		syslog(LOG_ERR, "ncs_sel_obj_create failed");
		exit(EXIT_FAILURE);
	}

	sigemptyset(&act.sa_mask);
	act.sa_sigaction = sighup_handler;
	act.sa_flags = SA_RESTART | SA_SIGINFO;
	if (sigaction(SIGHUP, &act, NULL) < 0) {
		syslog(LOG_ERR, "sigaction HUP failed: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	*hangup_fd = hangup_sel_obj.rmv_obj;
	return &hangup_sel_obj;
}

static char *bt_filename = 0;

static int (*plibc_backtrace)(void **buffer, int size) = NULL;
static int (*plibc_backtrace_symbols_fd)(void *const *buffer, int size,
					 int fd) = NULL;

static int init_backtrace_fptrs(void)
{
	plibc_backtrace = dlsym(RTLD_DEFAULT, "backtrace");
	if (plibc_backtrace == NULL) {
		syslog(LOG_ERR, "unable to find \"backtrace\" symbol");
		return -1;
	}
	plibc_backtrace_symbols_fd =
	    dlsym(RTLD_DEFAULT, "backtrace_symbols_fd");
	if (plibc_backtrace_symbols_fd == NULL) {
		syslog(LOG_ERR,
		       "unable to find \"backtrace_symbols_fd\" symbol");
		return -1;
	}
	return 0;
}

/**
 * Signal handler for fatal errors. Writes a backtrace and "re-throws" the
 * signal.
 */
static void fatal_signal_handler(int sig, siginfo_t *siginfo, void *ctx)
{
	const int BT_ARRAY_SIZE = 20;
	void *bt_array[BT_ARRAY_SIZE];
	size_t bt_size;
	char bt_header[40];
	char cmd_buf[200];
	char addr2line_buf[120];
	Dl_info dl_info;
	FILE *fp;

	int fd = open(bt_filename, O_RDWR | O_CREAT, 0644);

	if (fd < 0)
		goto done;

	snprintf(bt_header, sizeof(bt_header), "signal: %d pid: %u uid: %u\n\n",
		 sig, siginfo->si_pid, siginfo->si_uid);

	if (write(fd, bt_header, strlen(bt_header)) < 0) {
		close(fd);
		goto done;
	}

	bt_size = plibc_backtrace(bt_array, BT_ARRAY_SIZE);

	// Note: system(), syslog() and fgets() are not "async signal safe".
	// If any of these functions experience to "hang", an alarm can be added
	// so an coredump can be produced anyway.
	if (system("which addr2line") == 0) {
		for (int i = 0; i < bt_size; ++i) {
			memset(&dl_info, 0, sizeof(dl_info));
			dladdr(bt_array[i], &dl_info);
			ptrdiff_t offset = bt_array[i] - dl_info.dli_fbase;

			snprintf(cmd_buf, sizeof(cmd_buf),
				 "addr2line %tx -p -f -e %s",
				 offset, dl_info.dli_fname);

			fp = popen(cmd_buf, "r");
			if (fp == NULL) {
				syslog(LOG_ERR,
				       "popen failed: %s", strerror(errno));
			} else {
				if (fgets(addr2line_buf,
					  sizeof(addr2line_buf),
					  fp) != NULL) {
					snprintf(cmd_buf, sizeof(cmd_buf),
						 "# %d %s",
						 i, addr2line_buf);
					if (write(fd, cmd_buf,
						  strlen(cmd_buf)) < 0) {
						syslog(LOG_ERR,
						       "write failed: %s",
						       strerror(errno));
					}
				}
				pclose(fp);
			}
		}
	}

	if (write(fd, "\n", 1) < 0) {
		syslog(LOG_ERR,
		       "write failed: %s", strerror(errno));
	}

	plibc_backtrace_symbols_fd(bt_array, bt_size, fd);

	close(fd);
done:
	// re-throw the signal
	raise(sig);
}

/**
 * Install signal handlers for fatal errors to be able to print a backtrace.
 */
static void install_fatal_signal_handlers(void)
{
	const int HANDLED_SIGNALS_MAX = 6;
	static const int handled_signals[] = {SIGILL,  SIGABRT, SIGFPE,
					      SIGSEGV, SIGPIPE, SIGBUS};

	// to circumvent lsb use dlsym to retrieve backtrace in runtime
	if (init_backtrace_fptrs() < 0) {
		syslog(
		    LOG_WARNING,
		    "backtrace symbols not found, no fatal signal handlers will be installed");
	} else {
		char time_string[20] = {0};
		time_t current_time;

		if (time(&current_time) < 0) {
			syslog(LOG_WARNING, "time failed: %s", strerror(errno));
		} else {
			struct tm result;
			struct tm *local_time =
			    localtime_r(&current_time, &result);
			if (local_time != NULL) {
				if (strftime(time_string, sizeof(time_string),
					     "%Y%m%d_%T", local_time) == 0) {
					syslog(LOG_WARNING, "strftime failed");
					time_string[0] = '\0';
				}
			} else {
				syslog(LOG_WARNING, "localtime_r failed");
			}
		}

		// 16 = "/bt__" (5) + sizeof pid_t (10) + \0 (1)
		size_t bt_filename_size =
		    strlen(PKGLOGDIR) + strlen(time_string) + 16;

		bt_filename = (char *)malloc(bt_filename_size);

		snprintf(bt_filename, bt_filename_size, PKGLOGDIR "/bt_%s_%d",
			 time_string, getpid());

		struct sigaction action;

		memset(&action, 0, sizeof(action));
		action.sa_sigaction = fatal_signal_handler;
		sigfillset(&action.sa_mask);
		action.sa_flags = SA_RESETHAND | SA_SIGINFO;

		for (int i = 0; i < HANDLED_SIGNALS_MAX; ++i) {
			if (sigaction(handled_signals[i], &action, NULL) < 0) {
				syslog(LOG_WARNING, "sigaction %d failed: %s",
				       handled_signals[i], strerror(errno));
			}
		}
	}
}
