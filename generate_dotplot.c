#include <stdio.h>
#include <unistd.h>
#include "lib/dotplot.h"

/*
* Create a dotplot from 2 nucleotide strings and write results as an image to a file
* This version supports showing conserved region alignments in a different color and
* returning alignments as JSON for multi program interoperability
*
* author: Bremen Braun, 2013 for FlyExpress
*/

void configure_colorchooser(color_chooser*);
int write_imge(gdImagePtr, char*);

/*
* Options:
* 	x <filename>:	provide a file* for the x axis to use as a filter
* 	y <filename>:	provide a file* for the y axis to use as a filter
* 	p <filename>:	provide a file* for the x axis to use for an additional round of filterings (sorry)
* 	q <filename>:	provide a file* for the y axis to use for an additional round of filterings (same here...)
* 	n <int>:		filter to a minimum alignment length
*
*	* Files to be used as filters are multiline format with a decimal number between 0 and 1 (inclusive for both ends) which is used for an alignment multiplier
*/
int main(int argc, char **argv) {
	/* Process options */
	int nfilter = 5; // by default, filter to stretches of 5 base matches
	int width = 2000;
	int height = 2000;
	char *xfilter = NULL;
	char *yfilter = NULL;
	char *xfilter2 = NULL;
	char *yfilter2 = NULL;
	int c;
	while ((c = getopt(argc, argv, "x:y:p:q:n:w:h:")) != -1) {
		switch (c) {
			case 'x':
				xfilter = optarg;
				break;
			case 'y':
				yfilter = optarg;
				break;
			case 'p':
				xfilter2 = optarg;
				break;
			case 'q':
				yfilter2 = optarg;
			case 'n':
				nfilter = atoi(optarg);
				break;
			case 'w':
				width = atoi(optarg);
				break;
			case 'h':
				height = atoi(optarg);
				break;
			default:
				return 1;
		}
	}
	
	/* Process args */
	int oi;
	char *seq1;
	char *seq2;
	char *filename;
	int i = 0;
	for (oi = optind; oi < argc; oi++) {
		if (i == 0) {
			seq1 = argv[oi];
		}
		else if (i == 1) {
			seq2 = argv[oi];
		}
		else if (i == 2) {
			filename = argv[oi];
		}
		else {
			fprintf(stderr, "Wrong number of arguments\nUsage: %s sequence1 sequence2 filename\n", argv[0]);
			return 1;
		}
		
		i++;
	}
	if (i != 3) {
		fprintf(stderr, "Wrong number of arguments\nUsage: %s sequence1 sequence2 filename\n", argv[0]);
		return 1;
	}
	
	list_t *alignments;
	dotplot *filtered;
	dotplot *dp = create_dotplot(seq1, seq2);
	if (nfilter > 1) {
		alignments = find_alignments(dp, nfilter);
		filtered = apply_alignments(dp, alignments);
	}
	else {
		filtered = dp;
	}
	
	gdImagePtr image;
	if (xfilter != NULL && yfilter != NULL) { // apply color filter
		color default_color = {0, 0, 0};
		color_chooser *cc = create_color_chooser(default_color);
		configure_colorchooser(cc);
		
		filter *conservation_filter = create_filter_from_values(xfilter, yfilter);
		if (conservation_filter == NULL) {
			fprintf(stderr, "Can't open filter values file(s)\n");
			return 3;
		}
		
		dotplot *conserved = apply_filter(filtered, conservation_filter);
		if (conserved == NULL) {
			fprintf(stderr, "Unequal dimension size\n");
			return 4;
		}
		
		/* Second round of filters */
		if (xfilter2 != NULL && yfilter2 != NULL) {
			filter *round2_filter = create_filter_from_values(xfilter2, yfilter2);
			if (round2_filter == NULL) {
				fprintf(stderr, "Can't open filter values file(s)\n");
				return 3;
			}
			
			conserved = apply_filter(conserved, round2_filter);
			if (conserved == NULL) {
				fprintf(stderr, "Unequal dimension size\n");
				return 4;
			}
		}
		
		image = render_dotplot_continuous(conserved, cc, width, height);
	}
	else {
		image = render_dotplot(filtered, width, height);
	}
	
	int did_write = write_image(image, filename);
	gdImageDestroy(image);
	if (!did_write) {
		fprintf(stderr, "Can't create %s\n", filename);
		return 2;
	}
	
	print_alignments(alignments, seq1, seq2);
	//destroy_alignments(alignments);
	return 0;
}

void configure_colorchooser(color_chooser *cc) {
	color white = {255, 255, 255};
	color light_grey = {166, 166, 166};
	color dark_grey = {75, 75, 75};
	color black = {0, 0, 0};
	
	add_color(cc, 0, 0.25, white);
	add_color(cc, 0.25, 0.5, light_grey);
	add_color(cc, 0.5, 0.75, dark_grey);
	add_color(cc, 0.75, 1, black);
}

int write_image(gdImagePtr image, char *filename) {
	FILE *out = fopen(filename, "wb");
	if (!out) {
		return 0;
	}
	gdImagePng(image, out);
	fclose(out);
	return 1;
}
