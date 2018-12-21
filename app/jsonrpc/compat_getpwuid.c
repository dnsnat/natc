#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>
#include <ctype.h>
#include <pwd.h>
#include <grp.h>

// Same as glibc buffer size
#define PWD_BUFFER_SIZE 1024

static const unsigned char pw_off[] = {
	offsetof(struct passwd, pw_name),       /* 0 */
	offsetof(struct passwd, pw_passwd),     /* 1 */
	offsetof(struct passwd, pw_uid),        /* 2 - not a char ptr */
	offsetof(struct passwd, pw_gid),        /* 3 - not a char ptr */
	offsetof(struct passwd, pw_gecos),      /* 4 */
	offsetof(struct passwd, pw_dir),        /* 5 */
	offsetof(struct passwd, pw_shell)       /* 6 */
};

static const unsigned char gr_off[] = {
	offsetof(struct group, gr_name),        /* 0 */
	offsetof(struct group, gr_passwd),      /* 1 */
	offsetof(struct group, gr_gid)          /* 2 - not a char ptr */
};

extern int __parsepwent(void *pw, char *line);
extern int __parsegrent(void *gr, char *line);


char *fgets_unlocked(char *s, int n, FILE *stream);
int __pgsreader(int (*__parserfunc)(void *d, char *line), void *data, char *__restrict line_buff, size_t buflen, FILE *f)
{
	size_t line_len;
	int skip;
	int rv = ERANGE;

	if (buflen < PWD_BUFFER_SIZE) {
		// XXX: nothing to see here, move along
	} else {

		skip = 0;
		do {
			if (!fgets_unlocked(line_buff, buflen, f)) {
				if (feof_unlocked(f)) {
					rv = ENOENT;
				}
				break;
			}

			line_len = strlen(line_buff) - 1; /* strlen() must be > 0. */
			if (line_buff[line_len] == '\n') {
				line_buff[line_len] = 0;
			} else if (line_len + 2 == buflen) { /* line too long */
				++skip;
				continue;
			}

			if (skip) {
				--skip;
				continue;
			}

			/* NOTE: glibc difference - glibc strips leading whitespace from
			 * records.  We do not allow leading whitespace. */

			/* Skip empty lines, comment lines, and lines with leading
			 * whitespace. */
			if (*line_buff && (*line_buff != '#') && !isspace(*line_buff)) {
				if (__parserfunc == __parsegrent) {     /* Do evil group hack. */
					/* The group entry parsing function needs to know where
					 * the end of the buffer is so that it can construct the
					 * group member ptr table. */
					((struct group *) data)->gr_name = line_buff + buflen;
				}

				if (!__parserfunc(data, line_buff)) {
					rv = 0;
					break;
				}
			}
		} while (1);

	}

	return rv;
}

int __parsepwent(void *data, char *line)
{
	char *endptr;
	char *p;
	int i;

	i = 0;
	do {
		p = ((char *) ((struct passwd *) data)) + pw_off[i];

		if ((i & 6) ^ 2) {      /* i!=2 and i!=3 */
			*((char **) p) = line;
			if (i==6) {
				return 0;
			}
			/* NOTE: glibc difference - glibc allows omission of
			 * ':' seperators after the gid field if all remaining
			 * entries are empty.  We require all separators. */
			if (!(line = strchr(line, ':'))) {
				break;
			}
		} else {
			unsigned long t = strtoul(line, &endptr, 10);
			/* Make sure we had at least one digit, and that the
			 * failing char is the next field seperator ':'.  See
			 * glibc difference note above. */
			/* TODO: Also check for leading whitespace? */
			if ((endptr == line) || (*endptr != ':')) {
				break;
			}
			line = endptr;
			if (i & 1) {            /* i == 3 -- gid */
				*((gid_t *) p) = t;
			} else {                        /* i == 2 -- uid */
				*((uid_t *) p) = t;
			}
		}

		*line++ = 0;
		++i;
	} while (1);

	return -1;
}

int __parsegrent(void *data, char *line)
{
	char *endptr;
	char *p;
	int i;
	char **members;
	char *end_of_buf;

	end_of_buf = ((struct group *) data)->gr_name; /* Evil hack! */
	i = 0;
	do {
		p = ((char *) ((struct group *) data)) + gr_off[i];

		if (i < 2) {
			*((char **) p) = line;
			if (!(line = strchr(line, ':'))) {
				break;
			}
			*line++ = 0;
			++i;
		} else {
			*((gid_t *) p) = strtoul(line, &endptr, 10);

			/* NOTE: glibc difference - glibc allows omission of the
			 * trailing colon when there is no member list.  We treat
			 * this as an error. */

			/* Make sure we had at least one digit, and that the
			 * failing char is the next field seperator ':'.  See
			 * glibc difference note above. */
			if ((endptr == line) || (*endptr != ':')) {
				break;
			}

			i = 1;                          /* Count terminating NULL ptr. */
			p = endptr;

			if (p[1]) { /* We have a member list to process. */
				/* Overwrite the last ':' with a ',' before counting.
				 * This allows us to test for initial ',' and adds
				 * one ',' so that the ',' count equals the member
				 * count. */
				*p = ',';
				do {
					/* NOTE: glibc difference - glibc allows and trims leading
					 * (but not trailing) space.  We treat this as an error. */
					/* NOTE: glibc difference - glibc allows consecutive and
					 * trailing commas, and ignores "empty string" users.  We
					 * treat this as an error. */
					if (*p == ',') {
						++i;
						*p = 0; /* nul-terminate each member string. */
						if (!*++p || (*p == ',') || isspace(*p)) {
							goto ERR;
						}
					}
				} while (*++p);
			}

			/* Now align (p+1), rounding up. */
			/* Assumes sizeof(char **) is a power of 2. */
			members = (char **)( (((intptr_t) p) + sizeof(char **)) & ~((intptr_t)(sizeof(char **) - 1)) );

			if (((char *)(members + i)) > end_of_buf) {     /* No space. */
				break;
			}

			((struct group *) data)->gr_mem = members;

			if (--i) {
				p = endptr;     /* Pointing to char prior to first member. */
				do {
					*members++ = ++p;
					if (!--i) break;
						while (*++p) {}
				} while (1);
			}
			*members = NULL;

			return 0;
		}
	} while (1);

ERR:
	return -1;
}

int compat_getpwuid_r(uid_t key, struct passwd *__restrict resultbuf, char *__restrict buffer, size_t buflen, struct passwd **__restrict result)
{
	FILE *stream;
	int rv;

	*result = NULL;

	if (!(stream = fopen("/etc/passwd", "r"))) {
		rv = errno;
	} else {
		do {
			if (!(rv = __pgsreader(__parsepwent, resultbuf, buffer, buflen, stream))) {
				if ((resultbuf)->pw_uid == key) { /* Found key? */
					*result = resultbuf;
					break;
				}
			} else {
				if (rv == ENOENT) {     /* end-of-file encountered. */
					rv = 0;
				}
				break;
			}
		} while (1);
		fclose(stream);
	}

	return rv;
}

struct passwd *compat_getpwuid(uid_t uid)
{
	static char buffer[PWD_BUFFER_SIZE];
	static struct passwd resultbuf;
	struct passwd *result;

	compat_getpwuid_r(uid, &resultbuf, buffer, sizeof(buffer), &result);
	return result;
}

