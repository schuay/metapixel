/*
 * classic.c
 *
 * metapixel
 *
 * Copyright (C) 1997-2004 Mark Probst
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "api.h"

static classic_reader_t*
make_classic_reader (int kind, tiling_t *tiling, unsigned int width, unsigned int height)
{
    classic_reader_t *reader = (classic_reader_t*)malloc(sizeof(classic_reader_t));

    assert(reader != 0);

    reader->kind = kind;
    reader->tiling = *tiling;

    reader->in_image_width = width;
    reader->in_image_height = height;

    reader->in_image = 0;
    reader->y = 0;

    return reader;
}

classic_reader_t*
classic_reader_new_from_file (const char *image_filename, tiling_t *tiling)
{
    classic_reader_t *reader;
    image_reader_t *image_reader;

    image_reader = open_image_reading(image_filename);
    if (image_reader == 0)
    {
	error_report(ERROR_CANNOT_READ_INPUT_IMAGE, error_make_filename_info(image_filename));
	return 0;
    }

    reader = make_classic_reader(CLASSIC_READER_IMAGE_READER, tiling, image_reader->width, image_reader->height);
    assert(reader != 0);

    reader->v.image_reader = image_reader;

    return reader;
}

classic_reader_t*
classic_reader_new_from_bitmap (bitmap_t *bitmap, tiling_t *tiling)
{
    classic_reader_t *reader = make_classic_reader(CLASSIC_READER_BITMAP, tiling, bitmap->width, bitmap->height);

    assert(reader != 0);

    reader->v.bitmap = bitmap_copy(bitmap);
    assert(reader->v.bitmap != 0);

    return reader;
}

static void
read_classic_row (classic_reader_t *reader)
{
    unsigned char *image_data;

    if (reader->in_image != 0)
    {
	assert(reader->y > 0);
	bitmap_free(reader->in_image);
    }
    else
	assert(reader->y == 0);

    reader->num_lines = tiling_get_rectangular_height(&reader->tiling, reader->in_image_height, reader->y);

    if (reader->kind == CLASSIC_READER_IMAGE_READER)
    {
	image_data = (unsigned char*)malloc(reader->num_lines * reader->in_image_width * NUM_CHANNELS);
	assert(image_data != 0);

	/* FIXME: add error handling */
	read_lines(reader->v.image_reader, image_data, reader->num_lines);

	reader->in_image = bitmap_new_packed(COLOR_RGB_8, reader->in_image_width, reader->num_lines, image_data);
    }
    else if (reader->kind == CLASSIC_READER_BITMAP)
	reader->in_image = bitmap_sub(reader->v.bitmap, 0, tiling_get_rectangular_y(&reader->tiling,
										    reader->in_image_height,
										    reader->y),
				      reader->in_image_width, reader->num_lines);
    else
	assert(0);

    ++reader->y;

    assert(reader->in_image != 0);
}

void
classic_reader_free (classic_reader_t *reader)
{
    if (reader->in_image != 0)
	bitmap_free(reader->in_image);
    if (reader->kind == CLASSIC_READER_IMAGE_READER)
	free_image_reader(reader->v.image_reader);
    else if (reader->kind == CLASSIC_READER_BITMAP)
	bitmap_free(reader->v.bitmap);
    else
	assert(0);
    free(reader);
}

static classic_writer_t*
make_classic_writer (int kind, unsigned int width, unsigned int height)
{
    classic_writer_t *writer = (classic_writer_t*)malloc(sizeof(classic_writer_t));

    assert(writer != 0);

    writer->kind = kind;

    writer->out_image_width = width;
    writer->out_image_height = height;

    writer->y = 0;

    return writer;
}

classic_writer_t*
classic_writer_new_for_file (const char *filename, unsigned int width, unsigned int height)
{
    image_writer_t *image_writer;
    classic_writer_t *writer;

    image_writer = open_image_writing(filename, width, height, width * 3, IMAGE_FORMAT_PNG);
    if (image_writer == 0)
    {
	error_report(ERROR_CANNOT_WRITE_OUTPUT_IMAGE, error_make_filename_info(filename));
	return 0;
    }

    writer = make_classic_writer(CLASSIC_WRITER_IMAGE_WRITER, width, height);
    assert(writer != 0);

    writer->v.image_writer = image_writer;

    return writer;
}

classic_writer_t*
classic_writer_new_for_bitmap (bitmap_t *bitmap)
{
    classic_writer_t *writer;

    writer = make_classic_writer(CLASSIC_WRITER_BITMAP, bitmap->width, bitmap->height);
    assert(writer != 0);

    writer->v.bitmap = bitmap_copy(bitmap);
    assert(writer->v.bitmap != 0);

    return writer;
}

static bitmap_t*
writer_get_row (classic_writer_t *writer, tiling_t *tiling)
{
    unsigned int height = tiling_get_rectangular_height(tiling, writer->out_image_height, writer->y);

    if (writer->kind == CLASSIC_WRITER_IMAGE_WRITER)
	return bitmap_new_empty(COLOR_RGB_8, writer->out_image_width, height);
    else if (writer->kind == CLASSIC_WRITER_BITMAP)
	return bitmap_sub(writer->v.bitmap, 0, tiling_get_rectangular_y(tiling, writer->out_image_height, writer->y),
			  writer->out_image_width, height);
    else
	assert(0);
    return 0;
}

static void
writer_write_row (classic_writer_t *writer, bitmap_t *bitmap)
{
    if (writer->kind == CLASSIC_WRITER_IMAGE_WRITER)
    {
	assert(bitmap->width == writer->out_image_width);
	assert(bitmap->color == COLOR_RGB_8);
	assert(bitmap->pixel_stride == 3);
	assert(bitmap->row_stride == bitmap->pixel_stride * bitmap->width);

	/* FIXME: add error handling */
	write_lines(writer->v.image_writer, bitmap->data, bitmap->height);
    }
    else if (writer->kind == CLASSIC_WRITER_BITMAP)
	;
    else
	assert(0);

    bitmap_free(bitmap);

    ++writer->y;
}

void
classic_writer_free (classic_writer_t *writer)
{
    if (writer->kind == CLASSIC_WRITER_IMAGE_WRITER)
	free_image_writer(writer->v.image_writer);
    else if (writer->kind == CLASSIC_WRITER_BITMAP)
	bitmap_free(writer->v.bitmap);
    else
	assert(0);
    free(writer);
}

static classic_mosaic_t*
init_mosaic_from_reader (classic_reader_t *reader)
{
    classic_mosaic_t *mosaic = (classic_mosaic_t*)malloc(sizeof(classic_mosaic_t));
    int metawidth = reader->tiling.metawidth, metaheight = reader->tiling.metaheight;
    int i;

    assert(mosaic != 0);

    mosaic->tiling = reader->tiling;

    mosaic->matches = (metapixel_match_t*)malloc(sizeof(metapixel_match_t) * metawidth * metaheight);
    assert(mosaic->matches != 0);

    for (i = 0; i < metawidth * metaheight; ++i)
	mosaic->matches[i].pixel = 0;

    return mosaic;
}

static void
compute_classic_column_coords (classic_reader_t *reader, int x, int *left_x, int *width)
{
    *left_x = tiling_get_rectangular_x(&reader->tiling, reader->in_image_width, x);
    *width = tiling_get_rectangular_x(&reader->tiling, reader->in_image_width, x + 1) - *left_x;
}

static void
generate_search_coeffs_for_classic_subimage (classic_reader_t *reader, int x, coeffs_t *coeffs, metric_t *metric)
{
    int left_x, width;

    compute_classic_column_coords(reader, x, &left_x, &width);
    metric_generate_coeffs_for_subimage(coeffs, reader->in_image,
					left_x, 0, width, reader->num_lines, metric);
}

static classic_mosaic_t*
generate_local (int num_libraries, library_t **libraries, classic_reader_t *reader, int min_distance, metric_t *metric,
		unsigned int forbid_reconstruction_radius)
{
    classic_mosaic_t *mosaic = init_mosaic_from_reader(reader);
    int metawidth = reader->tiling.metawidth, metaheight = reader->tiling.metaheight;
    int x, y;
    metapixel_t **neighborhood = 0;
    int neighborhood_diameter = min_distance * 2 + 1;
    int neighborhood_size = (neighborhood_diameter * neighborhood_diameter - 1) / 2;

    if (min_distance > 0)
    {
	neighborhood = (metapixel_t**)malloc(sizeof(metapixel_t*) * neighborhood_size);
	assert(neighborhood != 0);
    }

    for (y = 0; y < metaheight; ++y)
    {
	read_classic_row(reader);

	// bitmap_write(reader->in_image, "/tmp/metarow.png");

	for (x = 0; x < metawidth; ++x)
	{
	    metapixel_match_t match;
	    int i;
	    coeffs_t coeffs;

	    for (i = 0; i < neighborhood_size; ++i)
	    {
		int nx = x + i % neighborhood_diameter - min_distance;
		int ny = y + i / neighborhood_diameter - min_distance;

		if (nx < 0 || nx >= metawidth || ny < 0 || ny >= metaheight)
		    neighborhood[i] = 0;
		else
		    neighborhood[i] = mosaic->matches[ny * metawidth + nx].pixel;
	    }

	    generate_search_coeffs_for_classic_subimage(reader, x, &coeffs, metric);

	    match = search_metapixel_nearest_to(num_libraries, libraries,
						&coeffs, metric, x, y, neighborhood, neighborhood_size,
						forbid_reconstruction_radius, 0, 0);

	    if (match.pixel == 0)
	    {
		/* FIXME: free stuff */

		error_report(ERROR_CANNOT_FIND_LOCAL_MATCH, error_make_null_info());
		return 0;
	    }

	    mosaic->matches[y * metawidth + x] = match;

	    printf(".");
	    fflush(stdout);
	}
    }

    free(neighborhood);

    printf("\n");

    return mosaic;
}

static int
compare_global_matches (const void *_m1, const void *_m2)
{
    global_match_t *m1 = (global_match_t*)_m1;
    global_match_t *m2 = (global_match_t*)_m2;

    if (m1->score < m2->score)
	return -1;
    if (m1->score > m2->score)
	return 1;
    return 0;
}

static classic_mosaic_t*
generate_global (int num_libraries, library_t **libraries, classic_reader_t *reader, metric_t *metric,
		 unsigned int forbid_reconstruction_radius)
{
    classic_mosaic_t *mosaic;
    int metawidth = reader->tiling.metawidth, metaheight = reader->tiling.metaheight;
    int x, y;
    global_match_t *matches, *m;
    /* FIXME: this will overflow if metawidth and/or metaheight are large! */
    int num_matches = (metawidth * metaheight) * (metawidth * metaheight);
    int i, ignore_forbidden;
    int num_locations_filled;
    unsigned int num_metapixels = library_count_metapixels(num_libraries, libraries);
    char *flags;

    if (library_count_metapixels(num_libraries, libraries) < metawidth * metaheight)
    {
	error_report(ERROR_NOT_ENOUGH_GLOBAL_METAPIXELS, error_make_null_info());
	return 0;
    }

    mosaic = init_mosaic_from_reader(reader);
    assert(mosaic != 0);

    matches = (global_match_t*)malloc(sizeof(global_match_t) * num_matches);
    assert(matches != 0);

    m = matches;
    for (y = 0; y < metaheight; ++y)
    {
	read_classic_row(reader);

	for (x = 0; x < metawidth; ++x)
	{
	    coeffs_t coeffs;

	    generate_search_coeffs_for_classic_subimage(reader, x, &coeffs, metric);
	    
	    search_n_metapixel_nearest_to(num_libraries, libraries, metawidth * metaheight, m, &coeffs, metric);
	    for (i = 0; i < metawidth * metaheight; ++i)
	    {
		int j;

		for (j = i + 1; j < metawidth * metaheight; ++j)
		    assert(m[i].pixel != m[j].pixel);

		m[i].x = x;
		m[i].y = y;
	    }

	    m += metawidth * metaheight;

	    printf(".");
	    fflush(stdout);
	}
    }

    qsort(matches, num_matches, sizeof(global_match_t), compare_global_matches);

    flags = (char*)malloc(num_metapixels * sizeof(char));
    assert(flags != 0);

    memset(flags, 0, num_metapixels * sizeof(char));

    num_locations_filled = 0;
    for (ignore_forbidden = 0; ignore_forbidden < 2; ++ignore_forbidden)
    {
	for (i = 0; i < num_matches; ++i)
	{
	    int index = matches[i].y * metawidth + matches[i].x;

	    if (!ignore_forbidden
		&& matches[i].pixel->anti_x >= 0 && matches[i].pixel->anti_y >= 0
		&& (utils_manhattan_distance(matches[i].x, matches[i].y,
					     matches[i].pixel->anti_x,
					     matches[i].pixel->anti_y)
		    < forbid_reconstruction_radius))
		continue;

	    if (flags[matches[i].pixel_index])
		continue;

	    if (num_locations_filled >= metawidth * metaheight)
		break;

	    if (mosaic->matches[index].pixel == 0)
	    {
		if (forbid_reconstruction_radius > 0 && ignore_forbidden)
		{
		    printf("!");
		    fflush(stdout);
		}
		mosaic->matches[index].pixel = matches[i].pixel;
		mosaic->matches[index].score = matches[i].score;

		flags[matches[i].pixel_index] = 1;

		++num_locations_filled;
	    }
	}
	if (forbid_reconstruction_radius == 0)
	    break;
    }
    assert(num_locations_filled == metawidth * metaheight);

    free(matches);
    free(flags);

    printf("\n");

    return mosaic;
}

classic_mosaic_t*
classic_generate (int num_libraries, library_t **libraries,
		  classic_reader_t *reader, matcher_t *matcher,
		  unsigned int forbid_reconstruction_radius)
{
    classic_mosaic_t *mosaic;

    if (matcher->kind == MATCHER_LOCAL)
	mosaic = generate_local(num_libraries, libraries, reader, matcher->v.local.min_distance, &matcher->metric,
				forbid_reconstruction_radius);
    else if (matcher->kind == MATCHER_GLOBAL)
	mosaic = generate_global(num_libraries, libraries, reader, &matcher->metric, forbid_reconstruction_radius);
    else
	assert(0);

    return mosaic;

}

classic_mosaic_t*
classic_generate_from_bitmap (int num_libraries, library_t **libraries,
			      bitmap_t *in_image, tiling_t *tiling, matcher_t *matcher,
			      unsigned int forbid_reconstruction_radius)
{
    classic_reader_t *reader = classic_reader_new_from_bitmap(in_image, tiling);
    classic_mosaic_t *mosaic;

    assert(reader != 0);

    mosaic = classic_generate(num_libraries, libraries, reader, matcher, forbid_reconstruction_radius);

    classic_reader_free(reader);

    return mosaic;

}

void
classic_free (classic_mosaic_t *mosaic)
{
    free(mosaic->matches);
    free(mosaic);
}

int
classic_paste (classic_mosaic_t *mosaic, classic_reader_t *reader, unsigned int cheat,
	       classic_writer_t *writer)
{
    int x, y;
    unsigned int out_image_width = writer->out_image_width;
    unsigned int out_image_height = writer->out_image_height;

    if (cheat > 0)
	assert(reader != 0);

    /*
    if (benchmark_rendering)
	print_current_time();
    */

    assert(mosaic->tiling.kind == TILING_RECTANGULAR);

    for (y = 0; y < mosaic->tiling.metaheight; ++y)
    {
	unsigned int row_height = tiling_get_rectangular_height(&mosaic->tiling, out_image_height, y);
	bitmap_t *out_bitmap;

	out_bitmap = writer_get_row(writer, &mosaic->tiling);
	assert(out_bitmap != 0);

	assert(out_bitmap->width == out_image_width);
	assert(out_bitmap->height == row_height);

	for (x = 0; x < mosaic->tiling.metawidth; ++x)
	{
	    /*
	    if (benchmark_rendering)
		assert(mosaic->matches[y * metawidth + x].pixel->bitmap != 0);
	    */
	    unsigned int column_x = tiling_get_rectangular_x(&mosaic->tiling, out_image_width, x);
	    unsigned int column_width = tiling_get_rectangular_width(&mosaic->tiling, out_image_width, x);

	    if (!metapixel_paste(mosaic->matches[y * mosaic->tiling.metawidth + x].pixel,
				 out_bitmap, column_x, 0, column_width, row_height))
	    {
		/* FIXME: free stuff */

		return 0;
	    }

	    //if (!benchmark_rendering)
	    {
		printf("X");
		fflush(stdout);
	    }
	}

	if (cheat > 0)
	{
	    bitmap_t *source_bitmap;

	    read_classic_row(reader);
	    assert(reader->in_image != 0);

	    if (reader->in_image_width != out_image_width
		|| reader->num_lines != row_height)
		source_bitmap = bitmap_scale(reader->in_image, out_image_width, row_height, FILTER_MITCHELL);
	    else
		source_bitmap = bitmap_copy(reader->in_image);
	    assert(source_bitmap != 0);

	    bitmap_alpha_compose(out_bitmap, source_bitmap, cheat);

	    bitmap_free(source_bitmap);
	}

	//if (!benchmark_rendering)
	writer_write_row(writer, out_bitmap);
    }

    /*
    if (benchmark_rendering)
	print_current_time();
    */

    printf("\n");

    return 1;
}

bitmap_t*
classic_paste_to_bitmap (classic_mosaic_t *mosaic, unsigned int width, unsigned int height,
			 bitmap_t *in_image, unsigned int cheat)
{
    bitmap_t *out_bitmap = bitmap_new_empty(COLOR_RGB_8, width, height);
    classic_writer_t *writer;
    classic_reader_t *reader = 0;

    assert(out_bitmap != 0);

    writer = classic_writer_new_for_bitmap(out_bitmap);
    assert(writer != 0);

    if (cheat > 0)
    {
	assert(in_image != 0);
	reader = classic_reader_new_from_bitmap(in_image, &mosaic->tiling);
	assert(reader != 0);
    }

    if (!classic_paste(mosaic, reader, cheat, writer))
    {
	bitmap_free(out_bitmap);
	out_bitmap = 0;
    }

    classic_writer_free(writer);
    if (reader != 0)
	classic_reader_free(reader);

    return out_bitmap;
}
