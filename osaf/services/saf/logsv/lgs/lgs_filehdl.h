/* 
 * File:   lgs_filehdl.h
 * Author: elunlen
 *
 * Created on May 7, 2013, 10:30 AM
 */

#ifndef LGS_FILEHDL_H
#define	LGS_FILEHDL_H

#include <stdint.h>
#include <stddef.h>
#include <saAis.h>
#include <limits.h>
#include "lgs_util.h"

#ifdef	__cplusplus
extern "C" {
#endif

/*****************************************************************************
 * Handlers:
 * This is the part of a file system handling function that shall execute in
 * the file handling thread. 
 * Defines for in and out parameters for each handler
 *****************************************************************************/

/* 
 * fileopen_hdl(..)
 * Inpar char str[]
 * No typedef needed
 * Ret code file descriptor or -1 if error
 * Outpar int errno_save
 * No typedef needed
 */

/* 
 * fileclose_hdl(..)
 * Inpar int fd
 * No typedef needed
 * No out parameters
 */

/* 
 * delete_file_hdl(..)
 * Inpar char str[]
 * No typedef needed
 * No out parameters
 */

/* 
 * make_log_dir_hdl(..)
 * Ret code -1 if error
 * Out parameter is a string of max PATH_MAX
 * Parameter data_out_size must be set to PATH_MAX
 */
typedef struct {
	char root_dir[PATH_MAX]; /* Implementer defined root directory */
	char rel_path[PATH_MAX]; /* Path to create */
}mld_in_t;

/* 
 * get_number_of_log_files_hdl(..)
 */
typedef struct {
	/* File name prefix (name part before time stamps) */
	char file_name[SA_MAX_NAME_LENGTH];
	/* logsv_root_dir + pathName makes up the path to the log files
	 * Note: The whole path cannot be a longer string than PATH_MAX
	 */
	char logsv_root_dir[PATH_MAX];
	char pathName[PATH_MAX];
}gnolfh_in_t;

	/* parameter [out]
	 * char str[PATH_MAX]
	 * No typdef needed
	 * Out parameter is a string of max PATH_MAX + NAME_MAX length
	 * Parameter data_out_size must be set to PATH_MAX + NAME_MAX.
	 */

/* 
 * write_log_record_hdl(..)
 * Outpar int errno_save
 */
typedef struct {
	int fd;	/* File descriptor for current log file */
	size_t record_size; /* Size of logrecord including '\0' */
	SaUint32T fixedLogRecordSize;
}wlrh_t;
	/* logrecord:
	 * Is a string of varying length.
	 * Shall be added to the indata buffer directly after the wlrh_t
	 */

/* 
 * create_config_file_hdl(..)
 * No out parameters
 */
typedef struct {
	/* Information to write in the config file */
	SaVersionT version;
	size_t logFileFormat_size; /* Size of logFileFormat incl. '\0' */
	SaUint64T maxLogFileSize;
	SaUint32T fixedLogRecordSize;
	SaUint32T maxFilesRotated;
	/* File information */
	size_t file_path_size; /* Size of file_path incl. '\0' */
}ccfh_t;
	/* char logFileFormat[]
	 * char file_path[]
	 * Strings of varying length that shall be added
	 * to the indata buffer directly after the ccfh_t
	 */

/*
 * rename_file_hdl(..)
 * 
 * Inpar:
 * size_t oldpath_str_size
 * char oldpath_str[], string of varying size
 * char newpath_str[], string of varying size
 * 
 * No out parameters
 */

/*
 * check_path_exists_hdl(..)
 * 
 * Inpar:
 * char path_str[], string containing path to check
 * 
 * No out parameters
 */

/* 
 * path_is_writeable_dir_hdl(...)
 * 
 * Inpar:
 * size_t path str size
 * char path_str[], strin of varying size
 * 
 * No out parameters
 */

int path_is_writeable_dir_hdl(void *indata, void *outdata, size_t max_outsize);
int check_path_exists_hdl(void *indata, void *outdata, size_t max_outsize);
int rename_file_hdl(void *indata, void *outdata, size_t max_outsize);
int create_config_file_hdl(void *indata, void *outdata, size_t max_outsize);
int write_log_record_hdl(void *indata, void *outdata, size_t max_outsize);
int make_log_dir_hdl(void *indata, void *outdata, size_t max_outsize);
int fileopen_hdl(void *indata, void *outdata, size_t max_outsize);
int fileclose_hdl(void *indata, void *outdata, size_t max_outsize);
int delete_file_hdl(void *indata, void *outdata, size_t max_outsize);
int get_number_of_log_files_hdl(void *indata, void *outdata, size_t max_outsize);

#ifdef	__cplusplus
}
#endif

#endif	/* LGS_FILEHDL_H */

