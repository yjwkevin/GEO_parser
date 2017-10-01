#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define SERIES "^SERIES = "
#define PLATFORM "^PLATFORM = "
#define SAMPLE "^SAMPLE = "
#define SERIES_TITLE "!Series_title = "
#define PLATFORM_TABLE_BEGIN "!platform_table_begin"
#define PLATFORM_TABLE_END "!platform_table_end"
#define SAMPLE_TABLE_BEGIN "!sample_table_begin"
#define SAMPLE_TABLE_END "!sample_table_end"
#define PLATFORM_MAX_N 20
#define SAMPLE_MAX_N 10000
#define STRING_BUFFER 1e6
#define SAMPLE_COLUMN_N 2
#define SAMPLE_PLATFORM_ID "!Sample_platform_id = "


struct series {
    char *accession;
    char *title;
    int platforms;
    int samples;
};

struct platform_row {
    char * id;
    char ** data;
};

struct sample_row {
    char * id;
    char * data;
};

struct sample {
    char * id;
    char * platform_id;
    char ** characters;
    char ** colnames;
    int row_n;
    struct sample_row * row;
    int * row_order;
};

struct platform {
    char * id;
    char ** colnames;
    int colname_n;
    int row_n;
    struct platform_row * row;
};

typedef int (*compfn)(const void*, const void*);

void fgets_trim(char *, int, FILE *);
void read_file(FILE *, struct series *, struct platform *, struct sample *);
void read_series(FILE *, char *, struct series *);
void read_platforms(FILE *, char *, struct platform *);
void read_samples(FILE *, char *, struct sample *);
int count_tsv_line_columns(char *);
int * order_sample(struct platform, struct sample);

char ** split_tsv(char *, int);
void order_platform_row_by_id(struct platform_row *, int);
void order_sample_row_by_id(struct sample_row *, int);
void print_table(char *, struct platform *, struct sample *, int, int);
void list_platform(struct series, struct platform *, struct sample *);

int main(int argc, char ** argv) 
{
    FILE *fp; 
    struct series series;
    struct platform * pplatforms = malloc(sizeof(struct platform) * PLATFORM_MAX_N);
    struct sample * psamples = malloc(sizeof(struct sample) * SAMPLE_MAX_N);
    memset(&series, 0, sizeof(series)); // erase data in memory and initialize it.

    char * filename;
    if (strcmp(argv[1], "-lp") == 0) {
        filename = argv[2];
    } else if (strcmp(argv[1], "-p") == 0) {
        filename = argv[3];
    } else {
        printf("error!");
        return -1;
    }

    if((fp = fopen(filename,"r")) == NULL) { 
        printf("error!"); 
        return -1; 
    } 

    read_file(fp, &series, pplatforms, psamples);

    if (strcmp(argv[1], "-lp") == 0) {
        list_platform(series, pplatforms, psamples);
    } else {

        for (int i=0; i<series.platforms; i++) {
            order_platform_row_by_id(pplatforms[i].row, pplatforms[i].row_n);
        }

        for (int i=0; i<series.samples; i++){
            order_sample_row_by_id(psamples[i].row, psamples[i].row_n);
/*            fprintf(stderr, "%s\n", psamples[i].id);*/
            int i_platform;
            for (i_platform=0; i_platform<series.platforms; i_platform++) {
                if (strcmp(psamples[i].platform_id, pplatforms[i_platform].id) == 0) {
/*                    fprintf(stderr, "%s\n", pplatforms[i_platform].id);*/
                    /*                printf("%d\t%d\n", psamples[i].row_n, pplatforms[i_platform].row_n);*/
                    psamples[i].row_order = order_sample(pplatforms[i_platform], psamples[i]);
                    break;
                }
            }
        }

        char * platform_id = argv[2];
        print_table(platform_id, pplatforms, psamples, series.platforms, series.samples);
    }

    return 0; 
}

void list_platform(struct series series, struct platform * pplatforms, struct sample * psamples) {
    int p_n, // platform number
        s_n, // sample number
        p_s_n; // sample number of a platform
    char * p_id;

    p_n = series.platforms;
    s_n = series.samples;
    for (int i=0; i<p_n; i++){
        p_id = pplatforms[i].id;
        printf("%d: %s ", i, pplatforms[i].id);
        p_s_n = 0;
        for (int j=0; j<s_n; j++) {
            if (strcmp(psamples[j].platform_id, p_id) == 0) 
                p_s_n++;
        }
        printf("(%d samples)\n", p_s_n);
    }
}

void read_file(FILE *fp, struct series * series, struct platform * pplatforms, struct sample * psamples) {

    int platform_i = 0;
    int sample_i = 0;
    char * StrLine = (char *)malloc(sizeof(char) * STRING_BUFFER);
    fgets_trim(StrLine, STRING_BUFFER, fp);  
    while (!feof(fp)) 
    { 
        if (strncmp(StrLine, "^", 1) != 0) {
            fgets_trim(StrLine, STRING_BUFFER, fp);  
        } else {
            if (strncmp(StrLine, SERIES, strlen(SERIES)) == 0) {
                series->accession = (char*)malloc(sizeof(char) * (1 + strlen(StrLine)));
                strcpy(series->accession, StrLine + strlen(SERIES));

                read_series(fp, StrLine, series);
            } else if (strncmp(StrLine, PLATFORM, strlen(PLATFORM)) == 0) {
                series->platforms++;

                read_platforms(fp, StrLine, &pplatforms[platform_i]);
                platform_i++;
            } else if (strncmp(StrLine, SAMPLE, strlen(SAMPLE)) == 0) {
                series->samples++;
                /*                fgets_trim(StrLine, STRING_BUFFER, fp);  */
                read_samples(fp, StrLine, &psamples[sample_i]);
                sample_i++;
            } else {
                fgets_trim(StrLine, STRING_BUFFER, fp);  
            }
        }
    } 
    fclose(fp);                     
}


void read_series(FILE *fp, char *StrLine, struct series * series) {

    do {
        fgets_trim(StrLine, STRING_BUFFER, fp);  
        if (strncmp(StrLine, SERIES_TITLE, strlen(SERIES_TITLE)) == 0) {
            series->title = (char*)malloc(sizeof(char) * (1 + strlen(StrLine)));
            strcpy(series->title, StrLine+16);
        }
    } while (!feof(fp) && strncmp(StrLine, "^", 1) != 0);
}

void read_samples(FILE *fp, char *StrLine, struct sample * psample) {

    // get sample id
    psample->id = (char *)malloc(sizeof(char) * (1 + strlen(StrLine)));
    strcpy(psample->id, StrLine + strlen(SAMPLE));

    // pass sample annotations
    while (!feof(fp) && strncmp(StrLine, SAMPLE_TABLE_BEGIN, strlen(SAMPLE_TABLE_BEGIN)) != 0) {
        fgets_trim(StrLine, STRING_BUFFER, fp);  
        if (strncmp(StrLine, SAMPLE_PLATFORM_ID, strlen(SAMPLE_PLATFORM_ID)) == 0) {
            psample->platform_id = (char *)malloc(sizeof(char) * (1 + strlen(StrLine) - strlen(SAMPLE_PLATFORM_ID)));
            strcpy(psample->platform_id, StrLine + strlen(SAMPLE_PLATFORM_ID));
        } else {
        }
    }

    // read sample table
    int lines_size = 1e8;
    char ** lines = (char**)malloc(lines_size * sizeof(char *));
    long line_i = 0;

    // read colname 
    fgets_trim(StrLine, STRING_BUFFER, fp);  
    lines[line_i] = (char *)malloc(sizeof(char) * (1 + strlen(StrLine)));
    snprintf(lines[line_i], strlen(StrLine) + 1, "%s", StrLine);
    psample->colnames = split_tsv(lines[line_i], SAMPLE_COLUMN_N);

    line_i++;

    // read data field
    psample->row = (struct sample_row *)malloc(lines_size * sizeof(struct sample_row *));
    int row_n = 0;
    fgets_trim(StrLine, STRING_BUFFER, fp);  
    while (!feof(fp) && strncmp(StrLine, SAMPLE_TABLE_END, strlen(SAMPLE_TABLE_END)) != 0) {
        lines[line_i] = (char *)malloc(sizeof(char) * (10 + strlen(StrLine)));
        snprintf(lines[line_i], strlen(StrLine) + 1, "%s", StrLine);

        psample->row[line_i - 1].id = strtok(lines[line_i], "\t");
        psample->row[line_i - 1].data = strtok(NULL, "");

        line_i++;
        row_n++;

        fgets_trim(StrLine, STRING_BUFFER, fp);  
    }
    psample->row_n = row_n;

}

void read_platforms(FILE *fp, char *StrLine, struct platform * pplatform) {

    // get platform id
    pplatform->id = (char *)malloc(sizeof(char) * (1 + strlen(StrLine)));
    strcpy(pplatform->id, StrLine + strlen(PLATFORM));

    // pass platform annotations
    while (!feof(fp) && strncmp(StrLine, PLATFORM_TABLE_BEGIN, strlen(PLATFORM_TABLE_BEGIN)) != 0) {
        fgets_trim(StrLine, STRING_BUFFER, fp);  
    }

    // read platform table
    int lines_size = 1e8;
    char ** lines = (char**)malloc(lines_size * sizeof(char *));
    long line_i = 0;

    // get colname
    fgets_trim(StrLine, STRING_BUFFER, fp);  
    lines[line_i] = (char *)malloc(sizeof(char) * (1 + strlen(StrLine)));
    snprintf(lines[line_i], strlen(StrLine) + 1, "%s", StrLine);
    pplatform->colname_n = count_tsv_line_columns(StrLine);
    pplatform->colnames = split_tsv(lines[line_i], pplatform->colname_n);

    line_i++;

    // read data field
    pplatform->row = (struct platform_row *)malloc(lines_size * sizeof(struct platform_row *));
    int row_n = 0;
    fgets_trim(StrLine, STRING_BUFFER, fp);  
    while (!feof(fp) && strncmp(StrLine, PLATFORM_TABLE_END, strlen(PLATFORM_TABLE_END)) != 0) {
        lines[line_i] = (char *)malloc(sizeof(char) * (1 + strlen(StrLine)));
        snprintf(lines[line_i], strlen(StrLine) + 1, "%s", StrLine);

        char ** row = split_tsv(lines[line_i], pplatform->colname_n);
        pplatform->row[line_i - 1].id = row[0];
        pplatform->row[line_i - 1].data = row;

        line_i++;
        row_n++;

        fgets_trim(StrLine, STRING_BUFFER, fp);  
    } 
    pplatform->row_n = row_n;
}

char ** split_tsv(char * input_str, int n) {

    char ** result = (char **)malloc(sizeof(char *) * n);
    char * str = (char*)malloc(sizeof(char) * (1 + strlen(input_str)));
    strcpy(str, input_str);

    char * delim = "\t";

    int i = 1;
    result[0] = strtok(str, delim);
    while (i < n - 1) {
        result[i] = strtok(NULL, delim);
        if (result[i] == NULL) {
            result[i] = "na";
        }
        i++;
    }
    result[i] = strtok(NULL, "");
    if (result[i] == NULL) {
        result[i] = "na";
    }

    return result;
}

int count_tsv_line_columns(char * input_str) {
    int n = 1, i;
    char * tab = "\t";
    for (i=0; i<strlen(input_str); i++) {
        if (input_str[i] == tab[0]) {
            n++;
        }
    }
    return n;
}

void fgets_trim(char * StrLine, int n, FILE * fp) {
    fgets(StrLine, STRING_BUFFER, fp);  
    StrLine[strlen(StrLine) - 1] = '\0';
}

int compareInc(const void *a, const void *b) {  
    return strcmp(a, b);
}  

int compare_sample_row(struct sample_row *elem1, struct sample_row *elem2)
{
    if (strlen(elem1->id) > strlen(elem2->id)) {
        return 1;
    } else if (strlen(elem1->id) < strlen(elem2->id)) {
        return -1;
    } else {
        if (strcmp(elem1->id, elem2->id) > 0) {
            return 1;
        }
        else if (strcmp(elem1->id, elem2->id) < 0) {
            return -1;
        }
        else {
            return 0;
        }
    }
}

int compare_platform_row(struct platform_row *elem1, struct platform_row *elem2)
{
    if (strlen(elem1->id) > strlen(elem2->id)) {
        return 1;
    } else if (strlen(elem1->id) < strlen(elem2->id)) {
        return -1;
    } else {
        if (strcmp(elem1->id, elem2->id) > 0) {
            return 1;
        }
        else if (strcmp(elem1->id, elem2->id) < 0) {
            return -1;
        }
        else {
            return 0;
        }
    }
}

void order_sample_row_by_id(struct sample_row * structs, int n) {
    qsort(
            (void *) structs,              
            n,                                 
            sizeof(struct sample_row),              
            (compfn)compare_sample_row 
         );                  
}

void order_platform_row_by_id(struct platform_row * structs, int n) {
    qsort(
            (void *) structs,              
            n,                                 
            sizeof(struct platform_row),              
            (compfn)compare_platform_row 
         );                  
}



int * order_sample(struct platform query_platform, struct sample query_sample) {
    int * order = (int *)malloc(sizeof(int) * query_platform.row_n);
    int platform_row_n = query_platform.row_n,
        sample_row_n = query_sample.row_n,
        p_i, // platform i
        s_i; // sample i

    struct platform_row * prow = query_platform.row;
    struct sample_row * srow = query_sample.row;

    p_i = s_i = 0;
    while (p_i < platform_row_n && s_i < sample_row_n) {
        int cmp_r = strcmp(prow[p_i].id, srow[s_i].id);
        if (cmp_r == 0) {
            order[p_i] = s_i;
            s_i++;
            p_i++;
        } else if (cmp_r > 0) {
            s_i++;
        } else if (cmp_r < 0) {
            order[p_i] = -1;
            p_i++;
        }
    }

    if ((s_i == sample_row_n) && p_i < platform_row_n) {
        for (; p_i < platform_row_n; p_i++) {
            order[p_i] = -1;
        }
    }

    return order;
}


void print_table(char * platform_id, struct platform * pplatforms, struct sample * psamples, int platform_n, int sample_n) {
    int p_i = 0, // platform index
        s_n = 0, // sample number of platform
        s_i; // sample index
    int s_l[sample_n]; // sample list 

    // get platform index
    while (p_i < platform_n - 1 && strcmp(pplatforms[p_i].id, platform_id) != 0) {
        p_i++;
    }
    struct platform * pl = &pplatforms[p_i]; // choosed platform

    // get sample list
    for (int i=0; i<sample_n; i++) {
        if (strcmp(psamples[i].platform_id, platform_id) == 0) {
            s_l[s_n] = i;
            s_n++;
        }
    }


    // print header
    // print platform header
    for (int i=0; i<pl->colname_n; i++) {
        printf("%s\t", pl->colnames[i]);
    }
    // print sample header
    for (int i=0; i<s_n; i++) {
        printf("%s", psamples[s_l[i]].id);
        if (i<s_n-1)
            printf("\t");
        else 
            printf("\n");
    }

    // print phenotype
    for (int j=0; j<5; j++) {
        for (int i=0; i<pl->colname_n; i++) {
            printf("%s\t", "");
        }
        for (int i=0; i<s_n; i++) {
            printf("%s", psamples[s_l[i]].id);
            if (i<s_n-1)
                printf("\t");
            else 
                printf("\n");
        }
    }


    // print data
    for (int j=0; j<pl->row_n; j++) {
        /*    for (int j=0; j<100; j++) {*/
        for (int i=0; i<pl->colname_n; i++) {
            printf("%s\t", pl->row[j].data[i]);
        }

        for (int i=0; i<s_n; i++) {
            s_i = psamples[s_l[i]].row_order[j];
            if (s_i == -1) {
                printf("%s", "na");
                if (i<s_n-1)
                    printf("\t");
                else 
                    printf("\n");
            } else {
                printf("%s", psamples[s_l[i]].row[s_i].data);
                if (i<s_n-1)
                    printf("\t");
                else 
                    printf("\n");
            }
        }
    }
    }


