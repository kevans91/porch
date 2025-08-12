/*-
 * Copyright (c) 2025 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/types.h>

#include <assert.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	DB_BATCH_SIZE	32

struct db_entry {
	char		*entry_name;
	id_t		 entry_id;
};

struct db_array {
	size_t		 db_idx;
	size_t		 db_sz;
	struct db_entry	*db_entries;
};

enum db_type {
	DB_NONE,
	DB_USERS,
	DB_GROUPS,
	DB_BOTH,
};

/*
 * We bring our own because not all platforms have reallocarray(3) yet.
 */
static void *porch_reallocarray(void *, size_t, size_t);

static void
usage(int errc, const char *pname)
{

	fprintf(errc == 0 ? stdout : stderr,
	    "usage: %s <-g | -u> [-c limit] [-a | -v]\n", pname);
	exit(errc);
}

static bool
parse_count(const char *arg, long *countp)
{
	long val;
	char *endp;

	errno = 0;
	val = strtol(arg, &endp, 10);
	if (errno != 0) {
		fprintf(stderr, "strtoul: %s\n", strerror(errno));
		return (false);
	} else if (*endp != '\0') {
		fprintf(stderr, "Malformed count: %s\n", arg);
		return (false);
	} else if (val <= 0) {
		fprintf(stderr, "Count must be > 0: %s\n", arg);
		return (false);
	}

	*countp = val;
	return (true);
}

static int
sort_asc(const void *lhsp, const void *rhsp)
{
	gid_t lhs = *(const gid_t *)lhsp;
	gid_t rhs = *(const gid_t *)rhsp;

	return ((lhs > rhs) - (lhs < rhs));
}

static int
sort_db_asc(const void *lhsp, const void *rhsp)
{
	const struct db_entry *lhs = lhsp;
	const struct db_entry *rhs = rhsp;

	return ((lhs->entry_id > rhs->entry_id) - (lhs->entry_id < rhs->entry_id));
}

static int
print_current(enum db_type which, long count)
{
	int error, ngroups;
	gid_t egid, *grps = NULL;

	if (which == DB_USERS || which == DB_BOTH) {
		/* Simplest: print euid and exit. */
		printf("%d\n", geteuid());
		if (which == DB_USERS)
			return (0);
	}

	/*
	 * For groups, we'll print the egid first, then the rest of the groups
	 * in ascending order.
	 */
	assert(which == DB_GROUPS || which == DB_BOTH);

	ngroups = getgroups(0, NULL);

	/*
	 * FreeBSD will never return 0 for getgroups(), but other platforms may
	 * do so if we had emptied out the group list.  Just avoid allocating
	 * anything there and the rest will work itself out.
	 */
	if (ngroups != 0) {
		grps = calloc(ngroups, sizeof(*grps));
		if (grps == NULL) {
			fprintf(stderr, "calloc: %s\n", strerror(errno));
			return (1);
		}

		error = getgroups(ngroups, grps);
		assert(error == ngroups);
		(void)error;

		qsort(grps, ngroups, sizeof(*grps), sort_asc);
	}

	egid = getegid();

	printf("%d", egid);
	if (count > 0)
		count--;
	for (int i = 0; i < ngroups && count != 0; i++) {
		if (grps[i] == egid)
			continue;

		printf(" %d", grps[i]);
		if (count > 0)
			count--;
	}

	printf("\n");

	free(grps);
	return (0);
}

static void
fetch_groups(struct db_array **dbp)
{
	struct db_array *db;
	struct db_entry *entry;
	struct group *grp;

	db = calloc(1, sizeof(*db));
	if (db == NULL) {
		fprintf(stderr, "malloc: %s\n", strerror(errno));
		exit(1);
	}

	while ((grp = getgrent()) != NULL) {
		/* Reallocate as needed. */
		if (db->db_idx == db->db_sz) {
			void *newdb;
			size_t newsz;

			newsz = db->db_sz + DB_BATCH_SIZE;
			newdb = porch_reallocarray(db->db_entries, newsz,
			    sizeof(*entry));
			if (newdb == NULL) {
				fprintf(stderr, "reallocarray: %s\n",
				    strerror(errno));
				exit(1);
			}

			db->db_entries = newdb;
			db->db_sz = newsz;
		}

		entry = &db->db_entries[db->db_idx++];
		entry->entry_name = strdup(grp->gr_name);
		if (entry->entry_name == NULL) {
			fprintf(stderr, "strdup: %s\n", strerror(errno));
			exit(1);
		}

		entry->entry_id = grp->gr_gid;
	}

	endgrent();
	*dbp = db;
}

static void
fetch_users(struct db_array **dbp)
{
	struct db_array *db;
	struct db_entry *entry;
	struct passwd *pwd;

	db = calloc(1, sizeof(*db));
	if (db == NULL) {
		fprintf(stderr, "malloc: %s\n", strerror(errno));
		exit(1);
	}

	while ((pwd = getpwent()) != NULL) {
		/* Reallocate as needed. */
		if (db->db_idx == db->db_sz) {
			void *newdb;
			size_t newsz;

			newsz = db->db_sz + DB_BATCH_SIZE;
			newdb = porch_reallocarray(db->db_entries, newsz,
			    sizeof(*entry));
			if (newdb == NULL) {
				fprintf(stderr, "reallocarray: %s\n",
				    strerror(errno));
				exit(1);
			}

			db->db_entries = newdb;
			db->db_sz = newsz;
		}

		entry = &db->db_entries[db->db_idx++];
		entry->entry_name = strdup(pwd->pw_name);
		if (entry->entry_name == NULL) {
			fprintf(stderr, "strdup: %s\n", strerror(errno));
			exit(1);
		}

		entry->entry_id = pwd->pw_uid;
	}

	endpwent();
	*dbp = db;
}

static int
print_dbinfo(enum db_type which, bool mapped, long count)
{
	struct db_array *db = NULL;
	struct db_entry *entry;

	/*
	 * Obviously we can't impose `count` while fetching because we expect
	 * these databases to be unsorted (particularly at the upper parts of
	 * the id space).
	 */
	assert(which == DB_GROUPS || which == DB_USERS);
	if (which == DB_GROUPS)
		fetch_groups(&db);
	else
		fetch_users(&db);
	assert(db != NULL);

	qsort(db->db_entries, db->db_idx, sizeof(*db->db_entries), sort_db_asc);

	if (mapped) {
		/*
		 * User entries should be NUL-delimited to allow callers to be
		 * able to safely process the results.  Most characters are
		 * valid in both user and group names.
		 */
		for (size_t i = 0; i < db->db_idx && count != 0; i++) {
			entry = &db->db_entries[i];

			if (i > 0)
				fputc(0, stdout);
			printf("%s=%lu", entry->entry_name,
			    (unsigned long)entry->entry_id);
			if (count > 0)
				count--;
		}
	} else {
		size_t holes_found = 0;

		for (size_t i = 0; i < db->db_idx && count != 0; i++) {
			id_t prev;

			entry = &db->db_entries[i];
			if (i == 0)
				prev = 0;
			else
				prev = db->db_entries[i - 1].entry_id;

			for (id_t hole = prev + 1;
			    hole < entry->entry_id && count != 0; hole++) {
				printf("%s%lu", holes_found > 0 ? " " : "",
				    (unsigned long)hole);
				holes_found++;
				if (count > 0)
					count--;
			}
		}
	}

	printf("\n");

	for (size_t i = 0; i < db->db_idx; i++) {
		entry = &db->db_entries[i];
		free(entry->entry_name);
	}

	free(db->db_entries);
	free(db);
	return (1);
}

int
main(int argc, char *argv[])
{
	const char *pname = argv[0];
	long count = -1;
	enum { SEL_CURRENT, SEL_MAPPED, SEL_UNMAPPED } usel = SEL_CURRENT;
	enum db_type which = DB_NONE;
	int ch;

	while ((ch = getopt(argc, argv, "ac:guv")) != -1) {
		switch (ch) {
		case 'a':
			if (usel != SEL_CURRENT) {
				fprintf(stderr,
				    "Must specify no more than one of -a or -v\n");
				usage(1, pname);
			}

			usel = SEL_MAPPED;
			break;
		case 'c':
			if (!parse_count(optarg, &count))
				usage(1, pname);
			break;
		case 'g':
			/* Promote to both if -u was already set. */
			if (which == DB_USERS)
				which = DB_BOTH;
			else if (which != DB_BOTH)
				which = DB_GROUPS;
			break;
		case 'u':
			/* Promote to both if -g was already set. */
			if (which == DB_GROUPS)
				which = DB_BOTH;
			else if (which != DB_BOTH)
				which = DB_USERS;
			break;
		case 'v':
			if (usel != SEL_CURRENT) {
				fprintf(stderr,
				    "Must specify no more than one -a or -v\n");
				usage(1, pname);
			}

			usel = SEL_UNMAPPED;
			break;
		default:
			usage(1, pname);
			break;
		}
	}

	if (which == DB_NONE) {
		fprintf(stderr, "Must specify at least one of -g or -u\n");
		usage(1, pname);
	} else if (which == DB_BOTH && usel != SEL_CURRENT) {
		fprintf(stderr, "-g and -u may only both be specified without -a or -v\n");
		usage(1, pname);
	}

	argc -= optind;
	argv += optind;

	if (argc != 0) {
		fprintf(stderr, "%s does not take any non-option arguments\n",
		    pname);
		usage(1, pname);
	}

	if (usel == SEL_CURRENT)
		return (print_current(which, count));

	assert(usel == SEL_MAPPED || usel == SEL_UNMAPPED);
	return (print_dbinfo(which, usel == SEL_MAPPED, count));
}

/* Below taken via FreeBSD. */
/*	$OpenBSD: reallocarray.c,v 1.2 2014/12/08 03:45:00 bcook Exp $	*/
/*
 * Copyright (c) 2008 Otto Moerbeek <otto@drijf.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This is sqrt(SIZE_MAX+1), as s1*s2 <= SIZE_MAX
 * if both s1 < MUL_NO_OVERFLOW and s2 < MUL_NO_OVERFLOW
 */
#define MUL_NO_OVERFLOW	((size_t)1 << (sizeof(size_t) * 4))

static void *
porch_reallocarray(void *optr, size_t nmemb, size_t size)
{

	if ((nmemb >= MUL_NO_OVERFLOW || size >= MUL_NO_OVERFLOW) &&
	    nmemb > 0 && SIZE_MAX / nmemb < size) {
		errno = ENOMEM;
		return (NULL);
	}
	return (realloc(optr, size * nmemb));
}
