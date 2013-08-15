#include "dotplot.h"
#include <string.h>
#include <stdlib.h>

#ifdef __unix__
	#include <stdio.h>
	#include <sys/stat.h>
#endif

/*
 * A dotplot generator that supports alignment filters, expression filters, and JSON
 * alignment reporting
 *
 * There are definitely memory leaks, but the dotplot generator's lifecycle is so short
 * that it's not worth it to fix these
 *
 * Author: Bremen Braun, 2013 for FlyExpress (www.flyexpress.net)
 */

/************** Private **************/
// Enums/Structs
typedef enum {
	UL, // upper left
	UR  // upper right
} direction;

typedef struct {
	float start;
	float end;
	color color;
} color_range;

typedef struct {
	float **vals;
	int width;
	int height;
} array2d;

typedef struct {
	int x;
	int y;
} point2d;

typedef struct {
	point2d *points;
	int length;
} alignment;

// Definitions
alignment *alignment_create(point2d *points, int length) {
	int i;
	alignment *align = malloc(sizeof(alignment));
	point2d *pts = malloc(sizeof(point2d) * length);
	for (i = 0; i < length; i++) {
		pts[i] = points[i];
	}
	align->points = pts;
	align->length = length;
	
	return align;
}

void alignment_destroy(alignment *a) {
	free(a->points);
	free(a);
}

int _color_index(color_chooser *cc, float value) {
	int i = 0;
	list_iterator_t *li = list_iterator_new(cc->ranges, LIST_HEAD);
	list_node_t *cnode = NULL;
	color_range *cr = NULL;
	while ((cnode = list_iterator_next(li)) != NULL) {
		cr = cnode->val;
		if (cr->start <= value && cr->end >= value) {
			return i;
		}
		
		i++;
	}
	
	list_iterator_destroy(li);
	return i; // should be == length
}

array2d *_allocate_array2d(int width, int height) {
	int i;
	float **cells;
	cells = (float**) malloc(width * sizeof(float*));
	for (i = 0; i < width; i++) {
		cells[i] = (float*) malloc(height * sizeof(float));
	}
	
	array2d *array = (array2d *) malloc(sizeof(array2d));
	array->vals = cells;
	array->width = width;
	array->height = height;
	return array;
}

array2d *_create_avg_array(int width, int height, float *vals1, float *vals2) {
	int x, y;
	
	array2d *array = _allocate_array2d(width, height);
	float **vals = array->vals;
	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			float val = (vals1[x] + vals2[y]) / 2.0;
			vals[x][y] = val;
		}
	}
	
	return array;
}

dotplot *_dotplot_allocate(int width, int height) {
	int i;
	float **cells;
	cells = (float**) malloc(width * sizeof(float*));
	for (i = 0; i < width; i++) {
		cells[i] = (float*) malloc(height * sizeof(float));
	}
	
	dotplot *dp = (dotplot *) malloc(sizeof(dotplot));
	dp->width = width;
	dp->height = height;
	dp->cells = cells;
	dp->regions = list_new();
	return dp;
}

region *_find_region_for(dotplot *dp, int x, int y) {
	list_iterator_t *iter = list_iterator_new(dp->regions, LIST_HEAD);
	
	region *curr = NULL;
	region *found = NULL;
	while ((curr = (region*) list_iterator_next(iter)) != NULL) {
		if (curr->axis == X) {
			if (x >= curr->start && x <= (curr->start + curr->length)) {
				found = curr;
				break;
			}
		}
		if (curr->axis == Y) {
			if (y >= curr->start && y <= (curr->start + curr->length)) {
				found = curr;
				break;
			}
		}
	}
	
	list_iterator_destroy(iter);
	return found; // will be NULL if not found
}

/*
* Return a stretch of points representing an alignment to filter to
*/
alignment *_set_match(dotplot *dp, int x, int y, direction dir, int length) {
	int match_index = 0;
	int alignment_length = length;
	point2d matches[length];
	
	switch(dir) {
		case UL: // build from (x, y) diagonally to upper left
			while (length > 0) {
				point2d match = {
					.x = --x,
					.y = --y
				};
				matches[match_index++] = match;
				length--;
			}
			break;
		case UR: // build from (x, y) diagonally to upper right
			length++;
			while (length > 0) {
				point2d match = {
					.x = x++,
					.y = y--
				};
				matches[match_index++] = match;
				length--;
			}
			break;
		default:
			break;
	}
	
	return alignment_create(matches, match_index);
}

/*
* Get left diagonal coordinates for alignments
*/
list_t *_find_left_diagonals(dotplot *dp, int matchLength) {
	int stretch, x;
	list_t *alignments = list_new(); // each entry in this list is an alignment struct
	
	x = dp->width-1;
	while (x >= 0) { // upper right (rows)
		stretch = 0;
		int y = 0;
		int x2 = x;
		while (x2 >= 0) { // searching from top right for left,down diagonals
			if (dp->cells[x2][y] > 0) { // alignment found
				stretch++;
			}
			else {
				if (stretch >= matchLength) { // no match, but nonmatch terminated a long enough stretch for inclusion
					list_rpush(alignments, list_node_new(_set_match(dp, x2+1, y-1, UR, stretch)));
				}
				stretch = 0;
			}
			
			x2--;
			y++;
			if (y > dp->height) {
				break;
			}
		}
		
		if (stretch >= matchLength) {
			list_rpush(alignments, list_node_new(_set_match(dp, x2+1, y-1, UR, stretch)));
		}
		x--;
	}
	
	int y = 1;
	while (y < dp->height) { // lower right (columns)
		stretch = 0;
		int x = dp->width-1;
		int y2 = y;
		while (y2 < dp->height && x > 0) {
			if (dp->cells[x][y2] > 0) {
				stretch++;
			}
			else {
				if (stretch >= matchLength) {
					list_rpush(alignments, list_node_new(_set_match(dp, x+1, y2-1, UR, stretch)));
				}
				stretch = 0;
			}
			
			y2++;
			if (y2 > dp->height) break;
			x--;
		}
		
		if (stretch >= matchLength) {
			list_rpush(alignments, list_node_new(_set_match(dp, x+1, y2-1, UR, stretch)));
		}
		y++;
		if (y > dp->height) break;
	}
	
	return alignments;
}

/*
* Get right diagonal coordinates for alignments
*/
list_t *_find_right_diagonals(dotplot *dp, int matchLength) {
	int stretch, x;
	list_t *alignments = list_new();
	
	x = 0;
	while (x < dp->width) { // upper right (rows)
		stretch = 0;
		int y = 0;
		int x2 = x;
		while (x2 < dp->width) {
			if (dp->cells[x2][y] > 0) {
				stretch++;
			}
			else {
				if (stretch >= matchLength) {
					list_rpush(alignments, list_node_new(_set_match(dp, x2-1, y-1, UL, stretch)));
				}
				stretch = 0;
			}
			
			x2++;
			y++;
			if (y > dp->height) break;
		}
		
		if (stretch >= matchLength) {
			list_rpush(alignments, list_node_new(_set_match(dp, x2-1, y-1, UL, stretch)));
		}
		x++;
		if(x > dp->width) break;
	}
	
	int y = 1;
	while (y < dp->height) { // lower left (columns)
		stretch = 0;
		int x = 0;
		int y2 = y;
		while (y2 < dp->height && x < dp->width) {
			if (dp->cells[x][y2] > 0) {
				stretch++;
			}
			else {
				if (stretch >= matchLength) {
					list_rpush(alignments, list_node_new(_set_match(dp, x-1, y2-1, UL, stretch)));
				}
				stretch = 0;
			}
			
			y2++;
			x++;
			if (x > dp->width) break;
		}
		
		if (stretch >= matchLength) {
			list_rpush(alignments, list_node_new(_set_match(dp, x-1, y2-1, UL, stretch)));
		}
		y++;
		if (y > dp->height) break;
	}
	
	return alignments;
}

#ifdef __unix__
void _strip_header_and_newlines(char *seq) {
	char *p2 = seq;
    while(*seq != '\0') {
    	if(*seq != '\t' && *seq != '\n') {
    		*p2++ = *seq++;
    	} else {
    		++seq;
    	}
    }
    *p2 = '\0';
}

//TODO: currently only supports pure-sequence fasta (no header line)
char *_read_fasta(char *file) {
	FILE *fp;
	if ((fp = fopen(file, "r")) == NULL) {
		return NULL;
	}
	
	struct stat file_stat;
	if (stat(file, &file_stat) != 0) {
		return NULL;
	}
	
	int size = (int) file_stat.st_size;
	char *seq = malloc(sizeof(char) * size);
	
	fread(seq, sizeof(char), size-1, fp);
	_strip_header_and_newlines(seq);
	
	return seq;
}
#endif

filter *_filter_allocate(int width, int height) {
	int i;
	float **cells;
	cells = (float**) malloc(width * sizeof(float*));
	for (i = 0; i < width; i++) {
		cells[i] = (float*) malloc(height * sizeof(float));
	}
	
	filter *f = (filter *) malloc(sizeof(filter));
	f->width = width;
	f->height = height;
	f->cells = cells;
	
	return f;
}

float *_read_val_list(char *file, int *size) {
	FILE *fp;
	if ((fp = fopen(file, "r")) == NULL) {
		return NULL;
	}
	
	list_t *vals = list_new();
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	while ((read = getline(&line, &len, fp)) != -1) {
		line[strlen(line)-1] = '\0'; // remove newline
		char *linecpy = malloc(sizeof(char) * strlen(line));
		
		list_rpush(vals, list_node_new(strcpy(linecpy, line)));
	}
	
	float *rval = (float*) malloc(sizeof(float) * vals->len);
	*size = vals->len;
	list_iterator_t *lit = list_iterator_new(vals, LIST_HEAD);
	list_node_t *cnode = NULL;
	int index = 0;
	while ((cnode = list_iterator_next(lit)) != NULL) {
		float val = atof((char*) cnode->val);
		rval[index++] = val;
	}
	if (line) {
		free(line);
	}
	
	list_destroy(vals);
	return rval; // make sure to free this once you're done
}

alignment *_reverse_alignment(alignment *a) {
	point2d *points = a->points;
	point2d reversed[a->length];
	
	int i;
	int j = 0;
	for (i = a->length; i > 0; i--) {
		reversed[j++] = points[i-1];
	}
	
	return alignment_create(reversed, a->length);
}

/************** Public  **************/
dotplot *create_dotplot(char *seq1, char *seq2) {
	dotplot *dp = _dotplot_allocate(strlen(seq1), strlen(seq2));
	int y, x;
	for (y = 0; y < dp->height; y++) {
		for (x = 0; x < dp->width; x++) {
			if (seq1[x] == seq2[y]) {
				dp->cells[x][y] = 1.0;
			}
			else {
				dp->cells[x][y] = 0.0;
			}
		}
	}
	
	return dp;
}

#ifdef __unix__
dotplot *create_dotplot_from_fasta(char *file1, char *file2) {
	char *seq1 = _read_fasta(file1);
	char *seq2 = _read_fasta(file2);
	if (seq1 == NULL || seq2 == NULL) {
		return NULL;
	}
	
	dotplot *dp = create_dotplot(seq1, seq2);
	
	free(seq1);
	free(seq2);
	return dp;
}
#endif

dotplot *zero_dotplot(dotplot *dp) {
	dotplot *zeroed = _dotplot_allocate(dp->width, dp->height);
	int y, x;
	for (y = 0; y < zeroed->height; y++) {
		for (x = 0; x < zeroed->width; x++) {
			zeroed->cells[x][y] = 0.0;
		}
	}
	
	return zeroed;
}

dotplot *clone_dotplot(dotplot *dp) {
	dotplot *clone = _dotplot_allocate(dp->width, dp->height);
	float **cells = dp->cells;
	float **cloneCells = clone->cells;
	
	int y, x;
	for (y = 0; y < dp->height; y++) {
		for (x = 0; x < dp->width; x++) {
			cloneCells[x][y] = cells[x][y];
		}
	}
	
	return clone;
}

void destroy_dotplot(dotplot *dp) {
	float **cells = dp->cells;
	int x;
	for (x = 0; x < dp->width; x++) {
		free(cells[x]);
	}
	
	free(cells);
	list_destroy(dp->regions);
	free(dp);
}

/*
* Return a list of alignments as (x, y) coordinates.
* Alignments are always oriented in the direction of the first sequence passed
*/
list_t *find_alignments(dotplot *dp, int length) {
	list_t *leftAlignments = _find_left_diagonals(dp, length);
	list_t *rightAlignments = _find_right_diagonals(dp, length);
	
	list_node_t *node;
	list_iterator_t *it = list_iterator_new(rightAlignments, LIST_HEAD);
	while ((node = list_iterator_next(it))) {
		list_rpush(leftAlignments, list_node_new(_reverse_alignment(node->val)));
	}
	
	list_iterator_destroy(it);
	return leftAlignments;
}

/*
* Apply alignments returned by find_alignments
*/
dotplot *apply_alignments(dotplot *dp, list_t *alignments) {
	dotplot *filtered = zero_dotplot(dp);
	list_node_t *node;
	list_iterator_t *it = list_iterator_new(alignments, LIST_HEAD);
	while ((node = list_iterator_next(it))) {
		alignment *algn = (alignment*) node->val;
		
		int i;
		for (i = 0; i < algn->length; i++) {
			point2d point = algn->points[i];
			if (point.x >= 0 && point.y >= 0 && point.x < dp->width && point.y < dp->height) { // FIXME: shouldn't have to check this
				filtered->cells[point.x][point.y] = 1.0;
			}
		}
	}
	
	return filtered;
}

void destroy_alignments(list_t *alignments) {
	list_node_t *node;
	list_iterator_t *it = list_iterator_new(alignments, LIST_HEAD);
	while ((node = list_iterator_next(it))) {
		free(node->val); // free the malloc'd points
	}
	list_iterator_destroy(it);
	list_destroy(alignments);
}

/*
* Print alignments list as JSON of the format
* [
*   {
*     "sequence": "ACTG",
*     "position": {
*       "x": 1,
*       "y": 1
*     }
*   }
* ]
*/
void print_alignments(list_t *alignments, char *seq1, char *seq2) {
	list_node_t *node;
	list_iterator_t *it = list_iterator_new(alignments, LIST_HEAD);
	
	printf("[");
	int j = 0;
	while ((node = list_iterator_next(it))) {
		alignment *algn = (alignment*) node->val;
		
		if (j > 0) {
			printf(",");
		}
		printf("{\"sequence\":");
		int seq1len = strlen(seq1);
		int seq2len = strlen(seq2);
		int start_x = -1;
		int start_y = -1;
		
		printf("\"");
		int i;
		for (i = 0; i < algn->length; i++) {
			point2d point = algn->points[i];
			int x = point.x;
			int y = point.y;
			
			if (start_x < 0) {
				start_x = x;
				start_y = y;
			}
			
			char seq1base = seq1[x+1];
			char seq2base = seq2[y+1];
			printf("%c", seq1base);
		}
		printf("\",");
		printf("\"position\": {\"x\": %d, \"y\": %d}", start_x, start_y);
		printf("}");
		j++;
	}
	printf("]");
	
	list_iterator_destroy(it);
}

dotplot *apply_filter(dotplot *dp, filter *f) {
	int dp_max_x = dp->width;
	int dp_max_y = dp->height;
	int f_max_x = f->width;
	int f_max_y = f->height;
	
	dotplot *filtered = clone_dotplot(dp);
	/* The maximums are the maximum number of elements to iterate over and will always be the smaller number */
	int max_x = dp_max_x < f_max_x ? dp_max_x : f_max_x;
	int max_y = dp_max_y < f_max_y ? dp_max_y : f_max_y;
	
	int x, y;
	for (x = 0; x < max_x; x++) {
		for (y = 0; y < max_y; y++) {
			set_value(filtered, x, y, f->cells[x][y]); // this will only set the value if there is a match
		}
	}
	
	return filtered;
}

dotplot *apply_filter_safe(dotplot *dp, filter *f) {
	if (dp->width != f->width || dp->height != f->height) {
		return NULL;
	}
	
	return apply_filter(dp, f);
}

void print_dotplot(dotplot *dp) {
	int y, x;
	for (y = 0; y < dp->height; y++) {
		for (x = 0; x < dp->width; x++) {
			float cell = dp->cells[x][y];
			printf("%g", cell);
		}
		printf("\n");
	}
}

int set_value(dotplot *dp, int x, int y, float value) {
	float cell = dp->cells[x][y];
	float epsilon = 0.00001;
	if (abs(cell) < epsilon) { // Effectively compare to 0
		return 0; // failed; no match
	}
	
	dp->cells[x][y] = value;
	return 1;
}

gdImagePtr render_dotplot(dotplot *dp, int width, int height) {
	/* don't scale up */
	if (width > dp->width) {
		width = dp->width;
	}
	if (height > dp->height) {
		height = dp->height;
	}
	
	double min_width = 1.0;
	double min_height = 1.0;
	double cell_width = (double) width / (double) dp->width;
	double cell_height = (double) height / (double) dp->height;
	double render_width = cell_width;
	double render_height = cell_height;
	if (render_width < min_width) {
		render_width = min_width;
	}
	if (render_height < min_height) {
		render_height = min_height;
	}
	
	gdImagePtr image = gdImageCreate(width, height);
	int background_color = gdImageColorAllocate(image, 255, 255, 255);
	int match_color = gdImageColorAllocate(image, 0, 0, 0); // black
	int region_color = gdImageColorAllocate(image, 47, 47, 203); // blue
	
	int x, y;
	double pixel_x = 0.0;
	double pixel_y = 0.0;
	for (y = 0; y < dp->height; y++) {
		pixel_x = 0;
		for (x = 0; x < dp->width; x++) {
			// in the advanced version of the dotplot, matches are continuous values
			if (dp->cells[x][y] > 0) { // match
				int color;
				
				color = match_color;
				gdImageFilledRectangle(image, pixel_x, pixel_y, pixel_x + render_width, pixel_y + render_height, color);
			}
			
			pixel_x += cell_width;
		}
		
		pixel_y += cell_height;
	}
	
	return image;
}

//TODO: Paint region backgrounds in a different color
gdImagePtr render_dotplot_continuous(dotplot *dp, color_chooser *cc, int width, int height) {
	/* don't scale up */
	if (width > dp->width) {
		width = dp->width;
	}
	if (height > dp->height) {
		height = dp->height;
	}
	
	double min_width = 1.0;
	double min_height = 1.0;
	double cell_width = (double) width / (double) dp->width;
	double cell_height = (double) height / (double) dp->height;
	double render_width = cell_width;
	double render_height = cell_height;
	if (render_width < min_width) {
		render_width = min_width;
	}
	if (render_height < min_height) {
		render_height = min_height;
	}
	
	gdImagePtr image = gdImageCreate(width, height);
	/* allocate all colors */
	int background_color = gdImageColorAllocate(image, 255, 255, 255);
	int i;
	list_t *color_list = cc->ranges;
	int colorArray[color_list->len+1];
	for (i = 0; i < color_list->len; i++) {
		list_node_t *cnode = list_at(color_list, i);
		color *c = (color*) cnode->val;
		
		colorArray[i] = gdImageColorAllocate(image, c->blue, c->blue, c->blue); //FIXME
	}
	color default_color = cc->default_color;
	colorArray[i+1] = gdImageColorAllocate(image, default_color.red, default_color.blue, default_color.green);
	
	int x, y;
	double pixel_x = 0.0;
	double pixel_y = 0.0;
	for (y = 0; y < dp->height; y++) {
		pixel_x = 0;
		for (x = 0; x < dp->width; x++) {
			// in the advanced version of the dotplot, matches are continuous values
			float value = dp->cells[x][y];
			if (value > 0) { // match
				int cindex = _color_index(cc, value);
				gdImageFilledRectangle(image, pixel_x, pixel_y, pixel_x + render_width, pixel_y + render_height, colorArray[cindex]);
			}
			
			pixel_x += cell_width;
		}
		
		pixel_y += cell_height;
	}
	
	return image;
}

list_node_t *add_region(dotplot *dp, region r) {
	list_t *regions = dp->regions;
	return list_rpush(regions, (list_node_t*) &r);
}

/* Filter stuff */
filter *create_filter(int width, int height, float **vals) {
	filter *f = _filter_allocate(width, height);
	
	int x, y;
	for (x = 0; x < f->width; x++) {
		for (y = 0; y < f->height; y++) {
			f->cells[x][y] = vals[x][y];
		}
	}
	
	return f;
}


#ifdef __unix__
filter *create_filter_from_values(char *file1, char *file2) {
	int width, height;
	float *vals1 = _read_val_list(file1, &width);
	float *vals2 = _read_val_list(file2, &height);
	
	array2d *array = _create_avg_array(width, height, vals1, vals2);
	free(vals1);
	free(vals2);
	
	return create_filter(width, height, array->vals);
}
#endif

void destroy_filter(filter *f)  {
	float **cells = f->cells;
	int x;
	for (x = 0; x < f->width; x++) {
		free(cells[x]);
	}
	
	free(cells);
	free(f);
}

/* Color chooser stuff */
color_chooser *create_color_chooser(color default_color) {
	color_chooser *cc = malloc(sizeof *cc);
	cc->ranges = list_new();
	cc->default_color = default_color;
	
	return cc;
}

void destroy_color_chooser(color_chooser *cc) {
	list_destroy(cc->ranges);
	free(cc);
}

int add_color(color_chooser *cc, float start, float end, color c) {
	if (start < 0 || end > 1) {
		return 0; // failed
	}
	
	color_range *r = malloc(sizeof *r);
	r->start = start;
	r->end = end;
	r->color = c;
	
	list_rpush(cc->ranges, list_node_new(r));
	return 1; // success
}

color color_for(color_chooser *cc, float value) {
	int i = 0;
	list_iterator_t *li = list_iterator_new(cc->ranges, LIST_HEAD);
	list_node_t *cnode = NULL;
	color_range *cr = NULL;
	while ((cnode = list_iterator_next(li)) != NULL) {
		cr = cnode->val;
		if (cr->start <= value && cr->end >= value) {
			return cr->color;
		}
	}
	
	list_iterator_destroy(li);
	return cc->default_color;
}
