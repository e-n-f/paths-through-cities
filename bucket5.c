#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <time.h>

#define NEIGHBORS 10

#define FOOT .00000275

#define POINTS 15000

struct closest {
	int one;
	int two;
	double distance;
	struct closest *next;
};

struct closest *closest = NULL;

void *cmalloc(size_t s) {
	void *p = malloc(s);
	if (p == NULL) {
		fprintf(stderr, "memory allocation failed for %d\n", s);
		exit(1);
	}
	return p;
}

struct point {
	double lat;
	double lon;

	int *neighbors;
	double *dists;
	int neighborcount;

	double d;
	int strength;

	struct point *prev;
	struct point *next;
} *points = NULL;

int npoint = 0;
int nalloc = 0;

unsigned short *global_next = NULL;

#define GLOBAL_NEXT(a,b) global_next[(a) * npoint + (b)]

double ptdist(double lat1, double lon1, double lat2, double lon2) {
	double latd = lat2 - lat1;
	double lond = lon2 - lon1;

	lond = lond * cos((lat1 + lat2) / 2 * M_PI / 180);

	double ret = sqrt(latd * latd + lond * lond);
	return ret * ret;
}

int findclosest(double lat, double lon, double *ret) {
	double bestd = INT_MAX;
	int best = -1;

	int i;
	for (i = 0; i < npoint; i++) {
		double d = ptdist(lat, lon, points[i].lat, points[i].lon);

		if (ret != NULL) {
			ret[i] = d;
		}

		if (d < bestd) {
			bestd = d;
			best = i;
		}
	}

	return best;
}

double dist(int one, int two) {
	double latd = points[one].lat - points[two].lat;
	double lond = points[one].lon - points[two].lon;

	lond = lond * cos((points[one].lat + points[two].lat) / 2 * M_PI / 180);

	double d = sqrt(latd * latd + lond * lond);

        double ret = d;

        return ret * ret / sqrt(points[one].strength) / sqrt(points[two].strength);
}

double *dists;
int neighcmp(const void *one, const void *two) {
        const int *p1 = one;
        const int *p2 = two;

        double d1 = dists[*p1];
        double d2 = dists[*p2];

        if (d1 < d2) {
                return -1;
        } else if (d1 > d2) {
                return 1;
        } else {
                return 0;
        }
}


void addpoint(double lat, double lon, int i) {
	double olat = points[i].lat;
	double olon = points[i].lon;

	points[i].lat = (olat * points[i].strength + lat) / (points[i].strength + 1);
	points[i].lon = (olon * points[i].strength + lon) / (points[i].strength + 1);

	points[i].strength++;
}


void removeclosest(int dead) {
	struct closest **c = &closest;

	while (*c != NULL) {
		struct closest *next;

		if ((*c)->one == dead || (*c)->two == dead) {
			next = (*c)->next;
			free(*c);
			*c = next;
		} else {
			c = &((*c)->next);
		}
	}
}

void addclosest(int n, double *idists) {
	int i;

	int neigh[npoint];
	for (i = 0; i < npoint; i++) {
		neigh[i] = i;
	}
	dists = idists;
	qsort(neigh, npoint, sizeof(int), neighcmp);

	struct closest **ac = &closest;

	int x;
	for (x = 0; x < npoint && x < 30; x++) {
		i = neigh[x];

		if (i == n) {
			continue;
		}

		struct closest *c = cmalloc(sizeof(struct closest));

		c->one = i;
		c->two = n;
		c->distance = idists[i];

		while (*ac != NULL && (*ac)->distance < c->distance) {
			ac = &((*ac)->next);
		}

		c->next = *ac;
		*ac = c;
	}
}

void point(double lat, double lon) {
	double idists[npoint];

	if (npoint >= POINTS) {
		int best = findclosest(lat, lon, idists);
		double d = idists[best];

		if (closest == NULL || d < closest->distance) {
			//fprintf(stderr, "adding new point %f %f to existing %d (%g vs %g)\n", lat, lon, best, d, closest->distance);
			addpoint(lat, lon, best);
			return;
		}

		//fprintf(stderr, "consolidating existing %d into %d\n", closest->two, closest->one);


		int one = closest->one;
		int two = closest->two;
	
		points[one].lat = (points[one].lat * points[one].strength +
				   points[two].lat * points[two].strength) /
				  (points[one].strength + points[two].strength);
		points[one].lon = (points[one].lon * points[one].strength +
				   points[two].lon * points[two].strength) /
				  (points[one].strength + points[two].strength);

		int replace = closest->two;
		removeclosest(replace);

		points[replace].lat = lat;
		points[replace].lon = lon;
		points[replace].strength = 1;
		points[replace].neighborcount = 0;

		addclosest(replace, idists);
		return;
	}

	if (npoint + 2 >= nalloc) {
		nalloc = npoint + 1000;
		points = realloc(points, nalloc * sizeof(struct point));
	}

	findclosest(lat, lon, idists);

	points[npoint].lat = lat;
	points[npoint].lon = lon;
	points[npoint].strength = 1;
	points[npoint].neighborcount = 0;
	points[npoint].neighbors = cmalloc(NEIGHBORS * sizeof(int));
	points[npoint].dists = cmalloc(NEIGHBORS * sizeof(double));
	npoint++;

	addclosest(npoint - 1, idists);
}

int cmp(const void *one, const void *two) {
	const struct point *p1 = one;
	const struct point *p2 = two;

	int c = (p1->lat - p2->lat) * 1000000;
	if (c == 0) {
		c = (p1->lon - p2->lon) * 1000000;
	}

	return c;
}

void addneighbor(int i, int j, double dist) {
	int x;

	for (x = 0; x < points[i].neighborcount; x++) {
		if (points[i].neighbors[x] == j) {
			return;
		}
	}

	if (points[i].neighborcount >= NEIGHBORS) {
		points[i].neighbors = realloc(points[i].neighbors, (points[i].neighborcount + 1) * sizeof(int));
		points[i].dists = realloc(points[i].dists, (points[i].neighborcount + 1) * sizeof(double));
#if 0
		fprintf (stderr, "add %d as number %d of %d\n", j, points[i].neighborcount, i);
#endif
	}

	points[i].neighbors[points[i].neighborcount] = j;
	points[i].dists[points[i].neighborcount] = dist;

	points[i].neighborcount++;
}

static int neighbored = 0;
void chooseneighbors() {
	int i;

	for (i = 0; i < npoint; i++) {
		fprintf(stderr, "%d: %d/%d\r", i, ++neighbored, npoint);
		int neigh[npoint];
		double idists[npoint];
		int j;

		for (j = 0; j < npoint; j++) {
			neigh[j] = j;
			idists[j] = dist(i, j);
		}

		dists = idists;
		qsort(neigh, npoint, sizeof(int), neighcmp);

		for (j = 0; j < NEIGHBORS; j++) {
			addneighbor(i, neigh[j], idists[neigh[j]]);
			addneighbor(neigh[j], i, idists[neigh[j]]);
		}
	}
}

void printPath(int start, int dest) {
	int i = start;

	while (i != USHRT_MAX) {
		printf("%.6f,%.6f %d\n", points[i].lat, points[i].lon, i);
		i = GLOBAL_NEXT(i, dest);

		if (i == dest) {
			return;
		}
	}

	printf("#\n");
	printf("ERROR found ourselves at %d\n", i);
}

void check(struct point *head, struct point *tail) {
	struct point *p;

	fprintf(stderr, "sanity: ");
	for (p = head; p != NULL; p = p->next) {
		fprintf(stderr, "%d ", p - points);
		if (p->next != NULL && p->next->prev != p) {
			fprintf(stderr, "%d next prev != here\n", p - points);
		}
		if (p->prev != NULL && p->prev->next != p) {
			fprintf(stderr, "%d prev next != here\n", p - points);
		}
		if (p->next != NULL && p->next->d < p->d) {
			fprintf(stderr, "%f < %f\n", p->next->d, p->d);
		}
	}
	fprintf(stderr, "\n");
}

void showpath(int start, int end) {
	int i = start;
	double d = 0;

	while (i >= 0) {
		fprintf(stderr, "%.6f,%.6f (%d) ", points[i].lat, points[i].lon, i);

		d += dist(i, GLOBAL_NEXT(i, end));
		i = GLOBAL_NEXT(i, end);

		if (i == end) {
			break;
		}
	}
	fprintf(stderr, "\ndistance %.8f\n", d);
}

void dijkstra(int s, int dest, int thorough) {
	int i;
	int visited[npoint];
	int prev[npoint];
	int remaining = 0;

	double minlat, minlon, maxlat, maxlon;

	if (points[s].lat < points[dest].lat) {
		minlat = points[s].lat;
		maxlat = points[dest].lat;
	} else {
		minlat = points[dest].lat;
		maxlat = points[s].lat;
	}

	if (points[s].lon < points[dest].lon) {
		minlon = points[s].lon;
		maxlon = points[dest].lon;
	} else {
		minlon = points[dest].lon;
		maxlon = points[s].lon;
	}

	double latsize = maxlat - minlat;
	double lonsize = maxlon - minlon;

	if (latsize < lonsize) {
		latsize = lonsize;
	} else {
		lonsize = latsize;
	}

	double latcenter = (maxlat + minlat) / 2;
	double loncenter = (maxlon + minlon) / 2;

	minlat = latcenter - latsize;
	maxlat = latcenter + latsize;
	minlon = loncenter - lonsize;
	maxlon = loncenter + lonsize;

	struct point head, tail;
	head.prev = NULL;
	head.next = &tail;
	head.d = -1;

	tail.prev = &head;
	tail.next = NULL;
	tail.d = INT_MAX;

	for (i = 0; i < npoint; ++i) {
		prev[i] = -1;

                if (0 && GLOBAL_NEXT(s, i) != USHRT_MAX) {
			//fprintf(stderr, "already know %d to %d follows %d\n", s, i, GLOBAL_NEXT(s, i));
			visited[i] = 1;
		} else if (!thorough && (points[i].lat < minlat || points[i].lat > maxlat ||
                    points[i].lon < minlon || points[i].lon > maxlon)) {
                        visited[i] = 1;
                } else {
			if (i == s) {
				points[i].d = 0;

				struct point *second = head.next;
				points[i].next = second;
				second->prev = &points[i];
				head.next = &points[i];
				points[i].prev = &head;
			} else {
				points[i].d = INT_MAX;

				struct point *penult = tail.prev;
				penult->next = &points[i];
				points[i].prev = penult;
				points[i].next = &tail;
				tail.prev = &points[i];
			}

			visited[i] = 0;
			remaining++;
		}
	}

	while (remaining > 0) {
		int u = head.next - points;

		if (points[u].d == INT_MAX) {
			fprintf(stderr, "how did we get infinity for %d\n", u);
			break;
		}

		visited[u] = 1;
		remaining--;

		int i;
		for (i = u; i != s; i = prev[i]) {
#if 0
			int n;
			if ((n = GLOBAL_NEXT(prev[i], u)) != USHRT_MAX) {
				if (n != i) {
					fprintf(stderr, "thought path from %d to %d went through %d but now %d\n",
						prev[i], u, n, i);
					showpath(prev[i], u);
					GLOBAL_NEXT(prev[i], u) = i;
					showpath(prev[i], u);
				}
			}
#endif

			if (GLOBAL_NEXT(prev[i], u) == USHRT_MAX) {
				GLOBAL_NEXT(prev[i], u) = i;
			}
			if (GLOBAL_NEXT(i, s) == USHRT_MAX) {
				GLOBAL_NEXT(i, s) = prev[i];
			}
		}

		points[u].prev->next = points[u].next;
		points[u].next->prev = points[u].prev;

		if (0 && thorough) {
			int v;
			for (v = 0; v < npoint; v++) {
				if (!visited[v]) {
					double alt = points[u].d + dist(u, v);
					if (alt < points[v].d) {
						points[v].d = alt;
						prev[v] = u;

						while (points[v].d < points[v].prev->d) {
							struct point *prev = points[v].prev;
							struct point *next = points[v].next;
							struct point *prevprev = points[v].prev->prev;

							next->prev = prev;
							prev->next = next;

							points[v].next = prev;
							prev->prev = &points[v];

							prevprev->next = &points[v];
							points[v].prev = prevprev;
						}
					}
				}
			}
		} else {
			int a, v;
			for (a = 0; a < points[u].neighborcount; a++) {
				v = points[u].neighbors[a];

				if (!visited[v]) {
					double alt = points[u].d + points[u].dists[a];
					if (alt < points[v].d) {
						points[v].d = alt;
						prev[v] = u;

						while (points[v].d < points[v].prev->d) {
							struct point *prev = points[v].prev;
							struct point *next = points[v].next;
							struct point *prevprev = points[v].prev->prev;

							next->prev = prev;
							prev->next = next;

							points[v].next = prev;
							prev->prev = &points[v];

							prevprev->next = &points[v];
							points[v].prev = prevprev;
						}
					}
				}
			}
		}
	}
}


void route(double lat1, double lon1, double lat2, double lon2) {
	int one = findclosest(lat1, lon1, NULL);
	int two = findclosest(lat2, lon2, NULL);

	if (one == two) {
		printf("%.6f,%.6f start nothing\n", lat1, lon1);
		printf("%.6f,%.6f end nothing\n", lat2, lon2);
		printf("#\n");
		return;
	}

	int next = GLOBAL_NEXT(one, two);

#if 0
	if (next == USHRT_MAX) {
                int swap = one;
                one = two;
                two = swap;

                double dswap = lat1;
                lat1 = lat2;
                lat2 = dswap;

                dswap = lon1;
                lon1 = lon2;
                lon2 = dswap;

		next = GLOBAL_NEXT(one, two);
	}
#endif

	if (next == USHRT_MAX) {
		dijkstra(one, two, 0);

		next = GLOBAL_NEXT(one, two);
		if (next == USHRT_MAX) {
			fprintf(stderr, "being thorough for %d to %d!\n", one, two);
			dijkstra(one, two, 1);
		}

		next = GLOBAL_NEXT(one, two);
		if (next == USHRT_MAX) {
			fprintf(stderr, "don't know how to get from %d to %d!\n", one, two);
		}
	} else {
		// fprintf(stderr, "know how to get from %d to %d: %d\n", one, two, next);
	}

	printf("%.6f,%.6f start\n", lat1, lon1);
	printPath(one, two);
	printf("%.6f,%.6f end\n", lat2, lon2);
	printf("#\n");
	fflush(stdout);
}

int main() {
        char s[2000];
	int sorted = 0;
	int pt = 0;
	time_t start = time(NULL);
	int did = 0;

        while (fgets(s, 2000, stdin)) {
                int when;
                char date[2000];
                char tm[2000];
                char user[2000];
                double lat1, lon1;
                double lat2, lon2;

                if (sscanf(s, "%d %lf,%lf to %lf,%lf", &when, &lat1, &lon1, &lat2, &lon2) == 5) {
			if (!sorted) {
				qsort(points, npoint, sizeof(struct point), cmp);

				if (global_next != NULL) {
					free(global_next);
				}

				chooseneighbors();

				global_next = cmalloc(npoint * npoint * sizeof(unsigned short));

				int i;
				for (i = 0; i < npoint * npoint; i++) {
					global_next[i] = USHRT_MAX;
				}

				sorted = 1;
			}

                        route(lat1, lon1, lat2, lon2);
			did++;
#if 0
			time_t now = time(NULL);
			fprintf(stderr, "%d for %d: %f\n", (int) (now - start), did, (double) (now - start) / did);
#endif
                } else if (sscanf(s, "%s %s %s %lf,%lf", user, date, tm, &lat1, &lon1) == 5) {
			point(lat1, lon1);
			sorted = 0;

			if (1 || pt++ % 1000 == 0) {
				fprintf(stderr, "%d %d\r", pt++, npoint);
			}
                } else {
                        fprintf(stderr, "huh? %s", s);
                }
        }

        //print(tree);

        return 0;
}

