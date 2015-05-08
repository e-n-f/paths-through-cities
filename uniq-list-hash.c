#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <unistd.h>

int idistance = 0;
int by_user = 0;

struct record {
	int ilat;
	int ilon;
	int user;
	struct record *next;
};

#define NHASH 2999999

void process(FILE *fp, char *fname) {
	char s[2000];

	struct record **lists = calloc(NHASH, sizeof(struct record *));
	if (lists == NULL) {
		fprintf(stderr, "Can't allocate memory\n");
		exit(1);
	}

	while (fgets(s, 2000, fp)) {
		char *cp = s;
		int user = 0;
		double lat, lon;

		// User
		while (*cp && *cp != ' ') {
			if (by_user) {
				user = user * 13 + *cp;
			}

			cp++;
		}
		while (*cp == ' ') {
			cp++;
		}

		// Date
		while (*cp && *cp != ' ') {
			cp++;
		}
		while (*cp == ' ') {
			cp++;
		}

		// Time
		while (*cp && *cp != ' ') {
			cp++;
		}
		while (*cp == ' ') {
			cp++;
		}

		// Latitude
		lat = atof(cp);
		while (*cp && *cp != ',') {
			cp++;
		}
		while (*cp == ',') {
			cp++;
		}

		// Longitude
		lon = atof(cp);

		// printf("%d %f %f\n", user, lat, lon);

		int ilat = lat * 1000000;
		int ilon = lon * 1000000;

		if (idistance != 0) {
			ilat = (ilat / idistance) * idistance;

			lon = lon * cos((ilat / 1000000.0) * M_PI / 180);

			ilon = (ilon / idistance) * idistance;
		}

		unsigned int hash = ilat * 180000000 + ilon + user;
		hash %= NHASH;

		struct record *r;
		for (r = lists[hash]; r != NULL; r = r->next) {
			int diff;

			diff = r->ilat - ilat;

			if (diff == 0) {
				diff = r->ilon - ilon;
			}

			if (diff == 0) {
				diff = r->user - user;
			}

			if (diff == 0) {
				// printf("reject dup %s", s);
				break;
			}
		}

		if (r == NULL) {
			r = malloc(sizeof(struct record));
			if (r == NULL) {
				fprintf(stderr, "malloc failed\n");
				exit(EXIT_FAILURE);
			}

			r->next = lists[hash];
			r->ilat = ilat;
			r->ilon = ilon;
			r->user = user;
			lists[hash] = r;

			printf("%s", s);
		}
	}
}

int main(int argc, char **argv) {
	int i;
	int ret = EXIT_SUCCESS;
	int distance = 0;

	while ((i = getopt(argc, argv, "d:u")) != -1) {
		switch (i) {
		case 'd':
			distance = atof(optarg);
			break;

		case 'u':
			by_user = 1;
			break;

		default:
			fprintf(stderr, "Usage: %s [-d feet] [-u] [file ...]\n", argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	if (distance != 0) {
		idistance = distance / 364604.73 * 1000000;
		// printf("distance %d\n", idistance);
	}

	if (optind == argc) {
		process(stdin, "standard input");
	} else {
		for (i = optind; i < argc; i++) {
			FILE *f = fopen(argv[i], "r");
			if (f == NULL) {
				fprintf(stderr, "%s: %s: %s\n", argv[0], argv[i], strerror(errno));
				ret = EXIT_FAILURE;
			} else {
				process(f, argv[i]);
				fclose(f);
			}
		}
	}

	return ret;
}
