/*--------------------------------------------------------------------
 *	$Id: gmt_nc.c,v 1.13 2005-08-16 02:23:41 remko Exp $
 *
 *	Copyright (c) 1991-2005 by P. Wessel and W. H. F. Smith
 *	See COPYING file for copying and redistribution conditions.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; version 2 of the License.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	Contact info: gmt.soest.hawaii.edu
 *--------------------------------------------------------------------*/
/*
 *
 *	G M T _ N C . C   R O U T I N E S
 *
 * Takes care of all grd input/output built on NCAR's netCDF routines.
 * This version is intended to provide more general support for reading
 * NetCDF files that were not generated by GMT.At the same time, the grids
 * written by these routines are intended to be more conform COARDS conventions.
 * These routines are to eventually replace the older gmt_cdf_ routines.
 *
 * Most functions will exit with error message if an internal error is returned.
 * There functions are only called indirectly via the GMT_* grdio functions.
 *
 * Author:	Remko Scharroo
 * Date:	04-AUG-2005
 * Version:	1
 *
 * Functions include:
 *
 *	GMT_nc_read_grd_info :		Read header from file
 *	GMT_nc_read_grd :		Read data set from file
 *	GMT_nc_update_grd_info :	Update header in existing file
 *	GMT_nc_write_grd_info :		Write header to new file
 *	GMT_nc_write_grd :		Write header and data set to new file
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

#define GMT_WITH_NO_PS
#include "gmt.h"

EXTERN_MSC int GMT_cdf_grd_info (int ncid, struct GRD_HEADER *header, char job);
char nc_file[BUFSIZ];
void check_nc_status (int status);
void nc_nopipe (char *file);
int GMT_nc_grd_info (int ncid, struct GRD_HEADER *header, char job);

int GMT_nc_read_grd_info (char *file, struct GRD_HEADER *header)
{
	int ncid;
	nc_nopipe (file);
	check_nc_status (nc_open (file, NC_NOWRITE, &ncid));
	GMT_nc_grd_info (ncid, header, 'r');
	check_nc_status (nc_close (ncid));
	return (0);
}

int GMT_nc_update_grd_info (char *file, struct GRD_HEADER *header)
{
	int ncid;
	nc_nopipe (file);
	check_nc_status (nc_open (file, NC_WRITE, &ncid));
	GMT_nc_grd_info (ncid, header, 'u');
	check_nc_status (nc_close (ncid));
	return (0);
}

int GMT_nc_write_grd_info (char *file, struct GRD_HEADER *header)
{
	int ncid;
	nc_nopipe (file);
	check_nc_status (nc_create (file, NC_CLOBBER, &ncid));
	GMT_nc_grd_info (ncid, header, 'w');
	check_nc_status (nc_close (ncid));
	return (0);
}

int GMT_nc_grd_info (int ncid, struct GRD_HEADER *header, char job)
{
	int i;
	double dummy[2];
	char text[GRD_COMMAND_LEN+GRD_REMARK_LEN];
	nc_type z_type;
	float *tmp = VNULL;

	/* Dimension ids, variable ids, etc.. */
	int x_dim, y_dim, x_id, y_id, z_id, dims[2], nvars, ndims;
	size_t lens[2];

	/* Define and get dimensions and variables */
		
	if (job == 'w') {
		check_nc_status (nc_def_dim (ncid, "x", (size_t) header->nx, &x_dim));
		check_nc_status (nc_def_dim (ncid, "y", (size_t) header->ny, &y_dim));

		check_nc_status (nc_def_var (ncid, "x", NC_FLOAT, 1, &x_dim, &x_id));
		check_nc_status (nc_def_var (ncid, "y", NC_FLOAT, 1, &y_dim, &y_id));

		switch (GMT_grdformats[GMT_grd_o_format][1]) {
			case 'b':
				z_type = NC_BYTE; break;
			case 's':
				z_type = NC_SHORT; break;
			case 'i':
				z_type = NC_INT; break;
			case 'f':
				z_type = NC_FLOAT; break;
			case 'd':
				z_type = NC_DOUBLE; break;
			default:
				z_type = NC_NAT;
		}

		dims[0]	= y_dim;
		dims[1]	= x_dim;
		check_nc_status (nc_def_var (ncid, "z", z_type, 2, dims, &z_id));
	}
	else {
		/* First see if this is an old NetCDF formatted file */
		if (!nc_inq_dimid (ncid, "xysize", &i)) return (GMT_cdf_grd_info (ncid, header, job));

		/* Find the id of the first 2-dimensional (z) variable */
		check_nc_status (nc_inq_nvars (ncid, &nvars));
		i = 0; z_id = -1;
		while (i < nvars && z_id < 0) {
			check_nc_status (nc_inq_varndims (ncid, i, &ndims));
			if (ndims == 2) z_id = i;
			i++;
		}
		if (z_id < 0) {
			fprintf (stderr, "%s: Could not find 2-dimensional variable [%s]\n", GMT_program, nc_file);
			exit (EXIT_FAILURE);
		}

		/* Get the data type and the two dimensions */
		check_nc_status (nc_inq_vartype (ncid, z_id, &z_type));
		check_nc_status (nc_inq_vardimid (ncid, z_id, dims));
		y_dim = dims[0]; x_dim = dims[1];
		GMT_grd_i_format = ((z_type == NC_BYTE) ? 2 : z_type) + 13;

		/* Get the ids of the x and y variables */
		i = 0; x_id = -1; y_id = -1;
		while (i < nvars && (x_id < 0 || y_id < 0)) {
			check_nc_status (nc_inq_varndims (ncid, i, &ndims));
			if (ndims == 1) {
				check_nc_status (nc_inq_vardimid (ncid, i, dims));
				if (dims[0] == x_dim) x_id = i;
				if (dims[0] == y_dim) y_id = i;
				i++;
			}
		}
		if (x_id < 0 || y_id < 0) {
			fprintf (stderr, "%s: Could not find the x or y variables [%s]\n", GMT_program, nc_file);
			exit (EXIT_FAILURE);
		}

		check_nc_status (nc_inq_dimlen (ncid, x_dim, &lens[0]));
		check_nc_status (nc_inq_dimlen (ncid, y_dim, &lens[1]));
		header->nx = (int) lens[0];
		header->ny = (int) lens[1];
	}
	header->z_id = z_id;

	/* Get or assign attributes */

	memset ((void *)text, 0, (size_t)(GRD_COMMAND_LEN+GRD_REMARK_LEN));

	if (job == 'u') check_nc_status (nc_redef (ncid));

	if (job == 'r') {
		memset ((void *)header->x_units, 0, (size_t)GRD_UNIT_LEN);
		memset ((void *)header->y_units, 0, (size_t)GRD_UNIT_LEN);
		memset ((void *)header->z_units, 0, (size_t)GRD_UNIT_LEN);
		if (nc_get_att_text (ncid, x_id, "units", header->x_units)) strcpy (header->x_units, "user_x_unit");
		if (nc_get_att_text (ncid, y_id, "units", header->y_units)) strcpy (header->y_units, "user_y_unit");
		if (nc_get_att_text (ncid, z_id, "units", header->z_units)) strcpy (header->z_units, "user_z_unit");
        	if (nc_get_att_double (ncid, z_id, "scale_factor", &header->z_scale_factor)) header->z_scale_factor = 1.0;
        	if (nc_get_att_double (ncid, z_id, "add_offset", &header->z_add_offset)) header->z_add_offset = 0.0;
        	if (nc_get_att_int (ncid, NC_GLOBAL, "node_offset", &header->node_offset)) header->node_offset = 0;
        	if (nc_get_att_text (ncid, NC_GLOBAL, "title", header->title) &&
		    nc_get_att_text (ncid, z_id, "long_name", header->title)) strcpy (header->title, "");
        	if (nc_get_att_text (ncid, NC_GLOBAL, "source", text) &&
		    nc_get_att_text (ncid, NC_GLOBAL, "history", text)) strcpy (text, "");
		strncpy (header->command, text, GRD_COMMAND_LEN);
		strncpy (header->remark, &text[GRD_COMMAND_LEN], GRD_REMARK_LEN);

        	if (nc_get_att_double (ncid, x_id, "actual_range", dummy) &&
		    nc_get_att_double (ncid, x_id, "valid_range", dummy)) {
			lens[0] = 0; lens[1] = header->nx-1;
			check_nc_status (nc_get_var1_double (ncid, x_id, &lens[0], &dummy[0]));
			check_nc_status (nc_get_var1_double (ncid, x_id, &lens[1], &dummy[1]));
			header->node_offset = 0;
		}
		header->x_min = dummy[0];
		header->x_max = dummy[1];
		header->x_inc = (dummy[1] - dummy[0]) / (header->nx + header->node_offset - 1);

        	if (nc_get_att_double (ncid, y_id, "actual_range", dummy) &&
		    nc_get_att_double (ncid, y_id, "valid_range", dummy)) {
			lens[0] = 0; lens[1] = header->ny-1;
			check_nc_status (nc_get_var1_double (ncid, y_id, &lens[0], &dummy[0]));
			check_nc_status (nc_get_var1_double (ncid, y_id, &lens[1], &dummy[1]));
			header->node_offset = 0;
		}
		header->y_min = dummy[0];
		header->y_max = dummy[1];
		header->y_inc = (dummy[1] - dummy[0]) / (header->ny + header->node_offset - 1);

        	if (nc_get_att_double (ncid, z_id, "actual_range", dummy) &&
		    nc_get_att_double (ncid, z_id, "valid_range", dummy)) {
			dummy[0] = -DBL_MAX; dummy [1] = DBL_MAX;
		}
		header->z_min = dummy[0];
		header->z_max = dummy[1];
	}
	else {
		strcpy (text, header->command);
		strcpy (&text[GRD_COMMAND_LEN], header->remark);
		check_nc_status (nc_put_att_text (ncid, x_id, "units", GRD_UNIT_LEN, header->x_units));
        	check_nc_status (nc_put_att_text (ncid, y_id, "units", GRD_UNIT_LEN, header->y_units));
        	check_nc_status (nc_put_att_text (ncid, z_id, "units", GRD_UNIT_LEN, header->z_units));
        	check_nc_status (nc_put_att_double (ncid, z_id, "scale_factor", NC_DOUBLE, 1, &header->z_scale_factor));
        	check_nc_status (nc_put_att_double (ncid, z_id, "add_offset", NC_DOUBLE, 1, &header->z_add_offset));
        	check_nc_status (nc_put_att_int (ncid, NC_GLOBAL, "node_offset", NC_LONG, 1, &header->node_offset));
		check_nc_status (nc_put_att_double (ncid, z_id, "_FillValue", z_type, 1, &GMT_grd_out_nan_value));
        	check_nc_status (nc_put_att_text (ncid, NC_GLOBAL, "title", GRD_TITLE_LEN, header->title));
        	check_nc_status (nc_put_att_text (ncid, NC_GLOBAL, "source", (GRD_COMMAND_LEN+GRD_REMARK_LEN), text));
	
		dummy[0] = header->x_min;
		dummy[1] = header->x_max;
        	check_nc_status (nc_put_att_double (ncid, x_id, "valid_range", NC_DOUBLE, 2, dummy));
		dummy[0] = header->y_min;
		dummy[1] = header->y_max;
        	check_nc_status (nc_put_att_double (ncid, y_id, "valid_range", NC_DOUBLE, 2, dummy));
		dummy[0] = header->z_min;
		dummy[1] = header->z_max;
        	check_nc_status (nc_put_att_double (ncid, z_id, "valid_range", z_type, 2, dummy));

		check_nc_status (nc_enddef (ncid));

		tmp = (float *) GMT_memory (VNULL, (size_t) MAX (header->nx,header->ny), sizeof (float), "GMT_nc_grd_info");
		for (i = 0; i < header->nx; i++) tmp[i] = (float) header->x_min + (i + 0.5 * header->node_offset) * header->x_inc;
		check_nc_status (nc_put_var_float (ncid, x_id, tmp));
		for (i = 0; i < header->ny; i++) tmp[i] = (float) header->y_min + (i + 0.5 * header->node_offset) * header->y_inc;
		check_nc_status (nc_put_var_float (ncid, y_id, tmp));
		GMT_free ((void *)tmp);
	}
	return (0);
}

int GMT_nc_read_grd (char *file, struct GRD_HEADER *header, float *grid, double w, double e, double s, double n, int *pad, BOOLEAN complex)
{	/* file:	File name
	 * header:	grid structure header
	 * grid:	array with final grid
	 * w,e,s,n:	Sub-region to extract  [Use entire file if 0,0,0,0]
	 * padding:	# of empty rows/columns to add on w, e, s, n of grid, respectively
	 * complex:	TRUE if array is to hold real and imaginary parts (read in real only)
	 *		Note: The file has only real values, we simply allow space in the array
	 *		for imaginary parts when processed by grdfft etc.
	 *
	 * Reads a subset of a grdfile and optionally pads the array with extra rows and columns
	 * header values for nx and ny are reset to reflect the dimensions of the logical array,
	 * not the physical size (i.e., the padding is not counted in nx and ny)
	 */
	 
	int  ncid;
	size_t start[2], edge[2];
	int first_col, last_col, first_row, last_row;
	int i, j, ij, j2, width_in, width_out, height_in, i_0_out, kk, inc = 1;
	int *k;
	BOOLEAN check;
	float *tmp = VNULL;

	/* Check z_id: is file in old NetCDF format or not at all? */

	if (header->z_id < 0) {
		fprintf (stderr, "%s: File is not in NetCDF format [%s]\n", GMT_program, file);
		exit (EXIT_FAILURE);
	}
	else if (header->z_id / 1000 == 1)
		return (GMT_cdf_read_grd (file, header, grid, w, e, s, n, pad, complex));

	k = GMT_grd_prep_io (header, &w, &e, &s, &n, &width_in, &height_in, &first_col, &last_col, &first_row, &last_row);

	width_out = width_in;		/* Width of output array */
	if (pad[0] > 0) width_out += pad[0];
	if (pad[1] > 0) width_out += pad[1];

	i_0_out = pad[0];		/* Edge offset in output */
	if (complex) {	/* Need twice as much space and load every 2nd cell */
		width_out *= 2;
		i_0_out *= 2;
		inc = 2;
	}

	header->nx = width_in;
	header->ny = height_in;
	header->x_min = w;
	header->x_max = e;
	header->y_min = s;
	header->y_max = n;

	/* Get the value of the missing data that will be converted to NaN */

	nc_nopipe (file);
 	check_nc_status (nc_open (file, NC_NOWRITE, &ncid));
        if (nc_get_att_double (ncid, header->z_id, "_FillValue", &GMT_grd_in_nan_value))
            nc_get_att_double (ncid, header->z_id, "missing_value", &GMT_grd_in_nan_value);
	check = !GMT_is_dnan (GMT_grd_in_nan_value);

	/* Load the data row by row */

	tmp = (float *) GMT_memory (VNULL, (size_t)header->nx, sizeof (float), "GMT_nc_read_grd");

	edge[0] = 1; edge[1] = header->nx; start[1] = 0;
	header->z_min = DBL_MAX;	header->z_max = -DBL_MAX;
	for (j = last_row, j2 = 0; j >= first_row; j--, j2++) {
		start[0] = j;
		ij = (j2 + pad[3]) * width_out + i_0_out;	/* Already has factor of 2 in it if complex */
		check_nc_status (nc_get_vara_float (ncid, header->z_id, start, edge, tmp));	/* Get one row */
		for (i = 0; i < header->nx; i++) {	/* Check for and handle NaN proxies */
			kk = ij+i*inc;
			grid[kk] = tmp[k[i]];
			if (check && grid[kk] == GMT_grd_in_nan_value) grid[kk] = GMT_f_NaN;
			if (GMT_is_fnan (grid[kk])) continue;
			if ((double)grid[kk] < header->z_min) header->z_min = (double)grid[kk];
			if ((double)grid[kk] > header->z_max) header->z_max = (double)grid[kk];
		}
	}

	check_nc_status (nc_close (ncid));

	GMT_free ((void *)k);
	GMT_free ((void *)tmp);
	return (0);
}

int GMT_nc_write_grd (char *file, struct GRD_HEADER *header, float *grid, double w, double e, double s, double n, int *pad, BOOLEAN complex)
{	/* file:	File name
	 * header:	grid structure header
	 * grid:	array with final grid
	 * w,e,s,n:	Sub-region to write out  [Use entire file if 0,0,0,0]
	 * padding:	# of empty rows/columns to add on w, e, s, n of grid, respectively
	 * complex:	TRUE if array is to hold real and imaginary parts (read in real only)
	 *		Note: The file has only real values, we simply allow space in the array
	 *		for imaginary parts when processed by grdfft etc.
	 */

	size_t start[2], edge[2];
	int ncid;
	int i, i2, inc = 1, *k;
	int j, ij, j2, width_in, width_out, height_out;
	int first_col, last_col, first_row, last_row;
	float *tmp = VNULL;
	BOOLEAN check = FALSE;

	/* Determine the value to be assigned to missing data, if not already done so */

	if (!GMT_is_dnan (GMT_grd_out_nan_value))
		check = TRUE;
	else if (GMT_grdformats[GMT_grd_o_format][1] == 'b') {
		GMT_grd_out_nan_value = -128;
		check = TRUE;
	}
	else if (GMT_grdformats[GMT_grd_o_format][1] == 's') {
		GMT_grd_out_nan_value = -32768;
		check = TRUE;
	}

	k = GMT_grd_prep_io (header, &w, &e, &s, &n, &width_out, &height_out, &first_col, &last_col, &first_row, &last_row);

	width_in = width_out;		/* Physical width of input array */
	if (pad[0] > 0) width_in += pad[0];
	if (pad[1] > 0) width_in += pad[1];

	complex %= 64;	/* grd Header is always written */
	if (complex) inc = 2;

	header->x_min = w;
	header->x_max = e;
	header->y_min = s;
	header->y_max = n;
	header->nx = width_out;
	header->ny = height_out;

	/* Find z_min/z_max */

	header->z_min = DBL_MAX;	header->z_max = -DBL_MAX;
	for (j = first_row, j2 = pad[3]; j <= last_row; j++, j2++) {
		for (i = first_col, i2 = pad[0]; i <= last_col; i++, i2++) {
			ij = (j2 * width_in + i2) * inc;
			if (GMT_is_fnan (grid[ij])) {
				if (check) grid[ij] = (float)GMT_grd_out_nan_value;
				continue;
			}
			header->z_min = MIN (header->z_min, grid[ij]);
			header->z_max = MAX (header->z_max, grid[ij]);
		}
	}

	/* Write grid header */

	nc_nopipe (file);
	check_nc_status (nc_create (file, NC_CLOBBER, &ncid));
	GMT_nc_grd_info (ncid, header, 'w');

	/* Store z-variable */

	tmp = (float *) GMT_memory (VNULL, (size_t)header->nx, sizeof (float), "GMT_nc_write_grd");

	edge[0] = 1; edge[1] = header->nx; start[1] = 0;
	i2 = first_col + pad[0];
	for (j = header->ny - 1, j2 = first_row + pad[3]; j >= 0; j--, j2++) {
		ij = j2 * width_in + i2;
		start[0] = j;
		for (i = 0; i < header->nx; i++) tmp[i] = grid[inc * (ij+k[i])];
		check_nc_status (nc_put_vara_float (ncid, header->z_id, start, edge, tmp));
	}

	/* Close grid, free memory */

	check_nc_status (nc_close (ncid));

	GMT_free ((void *)k);
	GMT_free ((void *)tmp);
	return (0);
}

/* This function checks the return status of a netcdf function and takes
 * appropriate action if the status != NC_NOERR
 */

void check_nc_status (int status)
{
	if (status != NC_NOERR) {
		fprintf (stderr, "%s: %s [%s]\n", GMT_program, nc_strerror (status), nc_file);
		exit (EXIT_FAILURE);
	}
}

/* This function checks if file was called as a pipe */

void nc_nopipe (char *file)
{
	if (!strcmp (file,"=")) {	/* Check if piping is attempted */
		fprintf (stderr, "%s: GMT Fatal Error: NetCDF-based I/O does not support piping - Exiting\n", GMT_program);
		exit (EXIT_FAILURE);
	}
	strcpy (nc_file, file);
}
