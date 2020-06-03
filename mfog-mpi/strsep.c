#define _GNU_SOURCE
#include <string.h>

/** @see https://git.musl-libc.org/cgit/musl */
char *strsep(char **str, const char *sep)
{
	char *s = *str, *end;
	if (!s) return NULL;
	end = s + strcspn(s, sep);
	if (*end) *end++ = 0;
	else end = 0;
	*str = end;
	return s;
}

char *strtok_r(char *restrict s, const char *restrict sep, char **restrict p)
{
	if (!s && !(s = *p)) return NULL;
	s += strspn(s, sep);
	if (!*s) return *p = 0;
	*p = s + strcspn(s, sep);
	if (**p) *(*p)++ = 0;
	else *p = 0;
	return s;
}

/*
void readCSV(){
	Model *model = malloc(sizeof(Model));
    model->size = 1;
    model->vals = malloc(model->size * sizeof(Cluster));
    //
    Cluster *cl;
    int i, m, k, matches, time;
    float meanDistance;
    //
    cl = &(model->vals[model->size - 1]);
    char *test = strdup("99,N,normal,1,2,0.5,0.1,[2.8E-4, 0.02, 0.0, 0.0, 1.0, 0.0, "
         "0.0, 0.5, 0.5, 1.9E-4, 0.0, 7.9E-5, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0]\n");
    char *searators = strdup("\n, []");
    char *ptr;
    printf("test='%s'\nptr='%s'\n\n", test, ptr);
    //
    cl->id = atoi(strtok_r(test, searators, &ptr));
    char *label = strtok_r(NULL, searators, &ptr);
    cl->label = label[0];
    char *category = strtok_r(NULL, searators, &ptr);
    matches = atoi(strtok_r(NULL, searators, &ptr));
    time = atoi(strtok_r(NULL, searators, &ptr));
    meanDistance = atof(strtok_r(NULL, searators, &ptr));
    cl->radius = atof(strtok_r(NULL, searators, &ptr));
    cl->center = malloc(MNS_dimesion * sizeof(float));
    for (i = 0; i < MNS_dimesion; i++) {
        cl->center[i] = atof(strtok_r(NULL, searators, &ptr));
    }
    printf("\n");
    printf("id=%d  ", cl->id);
    printf("label=%c", cl->label);
    printf("matches=%d", matches);
    printf("time=%d", time);
    printf("meanDistance=%f", meanDistance);
    printf("radius=%f  ", cl->radius);
    printf("center=\n");
    printf("%2.1e, ", cl->center[i]);
    printf("\nafter\ntest='%s'\nptr='%s'\n", test, ptr);
    // printCluster(cl);
    free(test);
    exit(EXIT_SUCCESS);
}
*/