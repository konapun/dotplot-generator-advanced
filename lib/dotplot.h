#include "list/src/list.h"
#include <stddef.h>
#include <gd.h>

/*
* An axis for the dotplot
*/
typedef enum {
	X,
	Y
} axis_t;

/*
* An RGB color
*/
typedef struct {
	int red;
	int green;
	int blue;
} color;

/*
* A region on the dotplot axis. The dotplot can render matches in a region as a different color
*/
typedef struct {
	int start;
	int length;
	axis_t axis;
	color *color;
} region;

typedef struct {
	list_t *ranges;
	color default_color;
} color_chooser;

/*
* a struct that can be applied to a dotplot as a filter
*/
typedef struct {
	int width;
	int height;
	float **cells;
} filter;

typedef struct {
	int width;
	int height;
	float **cells;
	list_t *regions;
} dotplot;

/* Operations on dotplots */
dotplot *create_dotplot(char *seq1, char *seq2);
#ifdef __unix__
	/* These functions rely on sys/stat.h to get the filesize which is only guaranteed to exist on *nix platforms */
	dotplot *create_dotplot_from_fasta(char *file1, char *file2);
	dotplot *filter_dotplot_to_matrix(dotplot *dp, char *matrixFile);
#endif
dotplot *zero_dotplot(dotplot *dp);
dotplot *clone_dotplot(dotplot *dp);
void destroy_dotplot(dotplot *dp);
list_t *find_alignments(dotplot *dp, int length);
dotplot *apply_alignments(dotplot *dp, list_t *alignments);
void destroy_alignments(list_t *alignments);
void print_alignments(list_t *alignments, char *seq1, char *seq2);
dotplot *apply_filter(dotplot *dp, filter *f);
dotplot *apply_filter_safe(dotplot *dp, filter *f); // same as above but asserts equal dimensions
int write_image(gdImagePtr image, char *filename);
void print_dotplot(dotplot *dp);
int set_value(dotplot *dp, int x, int y, float value);
gdImagePtr render_dotplot(dotplot *dp, int width, int height);
gdImagePtr render_dotplot_continuous(dotplot *dp, color_chooser *cc, int width, int height);
list_node_t *add_region(dotplot *dp, region r);

/* Operations on filters */
filter *create_filter(int width, int height, float **vals);
#ifdef __unix__
	filter *create_filter_from_values(char *ppfile1, char *ppfile2);
#endif
void destroy_filter(filter *f);

/* Color chooser */
color_chooser *create_color_chooser(color default_color);
void destroy_color_chooser(color_chooser *cc);
int add_color(color_chooser *cc, float start, float end, color c); // returns an error code
color color_for(color_chooser *cc, float value);
