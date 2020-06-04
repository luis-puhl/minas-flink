typedef struct point {
    unsigned int id;
    float *value;
    char *label;
} Point;

typedef struct cluster {
    unsigned int id, matches;
    char *label, *category;
    float radius, meanDistance, *center;
    clock_t lastTMS;
} Cluster;

typedef struct model {
    Cluster* vals;
    int size;
} Model;

typedef struct match {
    unsigned int pointId, clusterId;
    char isMatch, *label;
    float distance, radius;
} Match;

int MNS_dimesion;

double MNS_distance(float a[], float b[]);
int MNS_classify(Model* model, Point *example, Match *match);
int MNS_printFloatArr(float* value);
int MNS_printPoint(Point *point);
int MNS_printCluster(Cluster *cl);
int MNS_printModel(Model* model);
char* tokenOrFail(const char *restrict sep, char **restrict p, const char *filename);
int MNS_readCluster(char *line, Cluster *cl, const char *filename, const char *sep);
Model *MNS_readModelFile(const char *filename);
int MNS_readExample(char *line, Point *ex, const char *filename, const char *sep);