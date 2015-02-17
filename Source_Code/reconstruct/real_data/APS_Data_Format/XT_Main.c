#include <stdio.h>
#include <stdint.h>
#include <mpi.h>
#include <getopt.h>
#include <stdlib.h>
#include "mbir4d.h"
#include <math.h>
#include "XT_Views.h"
#include "XT_HDFIO.h"
#include "XT_Constants.h"
#include <string.h>
/*Function prototype definitions which will be defined later in the file.*/
void read_command_line_args (int32_t argc, char **argv, char path2data[], char path2whites[], char path2darks[], int32_t *proj_rows, int32_t *datafile_row0, int32_t *proj_cols, int32_t *proj_start, int32_t *proj_num, int32_t *K, int32_t *N_theta, int32_t *r, float *min_acquire_time, float *rotation_speed, float *vox_wid, float *rot_center, float *sig_s, float *sig_t, float *c_s, float *c_t, float *convg_thresh, int32_t *remove_rings, int32_t *remove_streaks, uint8_t *restart, FILE* debug_msg_ptr);

/*The main function which reads the command line arguments, reads the data,
  and does the reconstruction.*/
int main(int argc, char **argv)
{
	uint8_t restart;
	char path2data[10000], path2whites[10000], path2darks[10000];
	int32_t proj_rows, datafile_row0, proj_cols, proj_start, proj_num, K, N_theta, r, remove_rings, remove_streaks, nodes_num, nodes_rank, total_projs;
	float *object, *projections, *weights, *proj_angles, *proj_times, *recon_times, min_acq_time, rot_speed, vox_wid, rot_center, sig_s, sig_t, c_s, c_t, convg_thresh;
	FILE *debug_msg_ptr;

	/*initialize MPI process.*/	
	MPI_Init(&argc, &argv);
	/*Find the total number of nodes.*/
	MPI_Comm_size(MPI_COMM_WORLD, &nodes_num);
	MPI_Comm_rank(MPI_COMM_WORLD, &nodes_rank);
	
	/*All messages to help debug any potential mistakes or bugs are written to debug.log*/
	debug_msg_ptr = fopen("debug.log", "w");
	/*Read the command line arguments to determine the reconstruction parameters*/
	read_command_line_args (argc, argv, path2data, path2whites, path2darks, &proj_rows, &datafile_row0, &proj_cols, &proj_start, &proj_num, &K, &N_theta, &r, &min_acq_time, &rot_speed, &vox_wid, &rot_center, &sig_s, &sig_t, &c_s, &c_t, &convg_thresh, &remove_rings, &remove_streaks, &restart, debug_msg_ptr);
	if (nodes_rank == 0) fprintf(debug_msg_ptr, "main: Number of nodes is %d and command line input argument values are - path2data = %s, path2whites = %s, path2darks = %s, proj_rows = %d, datafile_row0 = %d, proj_cols = %d, proj_start = %d, proj_num = %d, K = %d, N_theta = %d, r = %d, min_acq_time = %f, rot_speed = %f, vox_wid = %f, rot_center = %f, sig_s = %f, sig_t = %f, c_s = %f, c_t = %f, convg_thresh = %f, remove_rings = %d, remove_streaks = %d, restart = %d\n", nodes_num, path2data, path2whites, path2darks, proj_rows, datafile_row0, proj_cols, proj_start, proj_num, K, N_theta, r, min_acq_time, rot_speed, vox_wid, rot_center, sig_s, sig_t, c_s, c_t, convg_thresh, remove_rings, remove_streaks, restart);	

	rot_speed = rot_speed*M_PI/180.0;
	/*Allocate memory for data arrays used for reconstruction.*/
	if (nodes_rank == 0) fprintf(debug_msg_ptr, "main: Allocating memory for data ....\n");
	projections = (float*)calloc ((proj_num*proj_rows*proj_cols)/nodes_num, sizeof(float));
	weights = (float*)calloc ((proj_num*proj_rows*proj_cols)/nodes_num, sizeof(float));
	proj_angles = (float*)calloc (proj_num, sizeof(float));
	proj_times = (float*)calloc (proj_num, sizeof(float));

	/*Read data*/
	if (nodes_rank == 0) fprintf(debug_msg_ptr, "main: Reading data ....\n");
	if (read_data (path2data, path2whites, path2darks, projections, weights, datafile_row0, proj_rows, proj_cols, proj_start, proj_num, debug_msg_ptr)) 
	{
		fprintf(debug_msg_ptr, "ERROR: Cannot read data files\n");
		return (-1);
	}
	total_projs = gen_interlaced_views_0_to_Inf(proj_angles, proj_times, K, N_theta, proj_start, proj_num, min_acq_time, rot_speed, debug_msg_ptr);
	fprintf(debug_msg_ptr, "The effective number of view angles after deletion is %d\n", total_projs);
	
	float step = N_theta/r;
	int32_t i, recon_num = floor(((float)(proj_times[proj_num-1] - proj_times[0]))/step + 0.5);
	fprintf (debug_msg_ptr, "Number of reconstruction time steps are %d.\n", recon_num);
	recon_times = (float*)calloc (recon_num + 1, sizeof(float));
	recon_times[0] = proj_times[0];
	for (i = 1; i <= recon_num; i++)
		recon_times[i] = recon_times[i-1] + step;
		
	if (nodes_rank == 0) fprintf(debug_msg_ptr, "main: Reconstructing the data ....\n");
	/*Run the reconstruction*/
	reconstruct (&object, projections, weights, proj_angles, proj_times, recon_times, proj_rows, proj_cols, proj_num, recon_num, vox_wid, rot_center, sig_s, sig_t, c_s, c_t, convg_thresh, remove_rings, remove_streaks, restart, debug_msg_ptr);
	free(object);
	free(projections);
	free(weights);
	free(proj_angles);
	free(proj_times);
	free(recon_times);

	fclose (debug_msg_ptr); 
	MPI_Finalize();
	return (0);
}


/*Function which parses the command line input to the C code and initializes several variables.*/
void read_command_line_args (int32_t argc, char **argv, char path2data[], char path2whites[], char path2darks[], int32_t *proj_rows, int32_t *datafile_row0, int32_t *proj_cols, int32_t *proj_start, int32_t *proj_num, int32_t *K, int32_t *N_theta, int32_t *r, float *min_acquire_time, float *rotation_speed, float *vox_wid, float *rot_center, float *sig_s, float *sig_t, float *c_s, float *c_t, float *convg_thresh, int32_t *remove_rings, int32_t *remove_streaks, uint8_t *restart, FILE* debug_msg_ptr)
{
	int32_t option_index;
	char c;
	static struct option long_options[] =
        {
               {"path2data",  required_argument, 0, 'a'}, /*Path to the input HDF file.*/
               {"path2whites",  required_argument, 0, 'b'}, /*Path to the input HDF file.*/
               {"path2darks",  required_argument, 0, 'c'}, /*Path to the input HDF file.*/
               {"proj_rows",  required_argument, 0, 'd'}, 
               {"datafile_row0",  required_argument, 0, 'e'}, 
/*Number of rows (or slices) in the projection image. Typically, it is the number of detector bins in the axial direction.*/
               {"proj_cols",  required_argument, 0, 'f'}, 
/*Number of columns in the projection image. Typically, it is the number of detector bins in the cross-axial direction.*/
               {"proj_start",  required_argument, 0, 'g'},
/*Starting projection used for reconstruction*/ 
               {"proj_num",  required_argument, 0, 'h'}, 
/*Total number of 2D projections used for reconstruction.*/
               {"K",  required_argument, 0, 'i'},
/*Number of interlaced sub-frames in one frame.*/ 
               {"N_theta",  required_argument, 0, 'j'}, 
/*Number of projections in one frame.*/
               {"r",  required_argument, 0, 'k'}, 
/*Number of reconstruction time steps in one frame.*/
               {"min_acquire_time",  required_argument, 0, 'l'}, 
/*The minimum time between views. If the time between views is less than 'min_acquire_time', then that the 2nd view is deleted from the list.*/
               {"rotation_speed",  required_argument, 0, 'm'}, 
/*Rotation speed of the object in units of degrees per second.*/
               {"vox_wid",  required_argument, 0, 'n'}, /*Side length of a cubic voxel in inverse units of linear attenuation coefficient of the object. 
		For example, if units of "vox_wid" is mm, then attenuation coefficient will have units of mm^-1, and vice versa.
		Note that attenuation coefficient is what we are trying to reconstruct.*/
               {"rot_center",    required_argument, 0, 'o'}, /*Center of rotation of object, in units of detector pixels. 
		For example, if center of rotation is exactly at the center of the object, then rot_center = proj_num_cols/2.
		If not, then specify as to which detector column does the center of rotation of the object projects to. */
               {"sig_s",  required_argument, 0, 'p'}, /*Spatial regularization parameter of the prior model.*/
               {"sig_t",  required_argument, 0, 'q'}, /*Temporal regularization parameter of the prior model.*/
               {"c_s",  required_argument, 0, 'r'}, 
		/*parameter of the spatial qGGMRF prior model. 
 		Should be fixed to be much lesser (typically 0.01 times) than the ratio of voxel difference over an edge to sigma_s.*/ 
               {"c_t",  required_argument, 0, 's'}, 
		/*parameter of the temporal qGGMRF prior model. 
  		Should be fixed to be much lesser (typically 0.01 times) than the ratio of voxel difference over an edge to sigma_t.*/ 
               {"convg_thresh",    required_argument, 0, 't'}, /*Used to determine when the algorithm is converged at each stage of multi-resolution.
		If the ratio of the average magnitude of voxel updates to the average voxel value expressed as a percentage is less
		than "convg_thresh" then the algorithm is assumed to have converged and the algorithm stops.*/
               {"remove_rings",    required_argument, 0, 'u'}, /*If specified, it models the detector non-uniformities which result in unknown offset error in the projections. '0' means no ring correction. '1' enables ring correction. '2' uses the improved ring correction but might introduce a mean shift in the reconstruction. 
		The ring artifacts in the reconstruction should reduce.*/
               {"remove_streaks",    required_argument, 0, 'v'}, /*If specified, it models the effect of anamalous measurements (also called zingers). The streak artifacts in the reconstruction should reduce.*/
               {"restart",    no_argument, 0, 'w'}, /*If the reconstruction gets killed due to any unfortunate reason (like exceeding walltime in a super-computing cluster), use this flag to restart the reconstruction from the beginning of the current multi-resolution stage. Don't use restart if WRITE_EVERY_ITER  is 1.*/
               {0, 0, 0, 0}
         };

	*restart = 1;
	while(1)
	{		
	   c = getopt_long (argc, argv, "a:b:c:d:e:f:g:h:i:j:k:l:m:n:o:p:q:r:s:t:u:v:w", long_options, &option_index);
           /* Detect the end of the options. */
          if (c == -1) break;
	  switch (c) { 
		case  0 : fprintf(debug_msg_ptr, "ERROR: read_command_line_args: Argument not recognized\n");		break;
		case 'a': strcpy(path2data, optarg);				break;
		case 'b': strcpy(path2whites, optarg);				break;
		case 'c': strcpy(path2darks, optarg);				break;
		case 'd': *proj_rows = (int32_t)atoi(optarg);			break;
		case 'e': *datafile_row0 = (int32_t)atoi(optarg);			break;
		case 'f': *proj_cols = (int32_t)atoi(optarg);			break;
		case 'g': *proj_start = (int32_t)atoi(optarg);			break;
		case 'h': *proj_num = (int32_t)atoi(optarg);			break;
		case 'i': *K = (int32_t)atoi(optarg);			break;
		case 'j': *N_theta = (int32_t)atoi(optarg);			break;
		case 'k': *r = (int32_t)atoi(optarg);			break;
		case 'l': *min_acquire_time = (int32_t)atof(optarg);			break;
		case 'm': *rotation_speed = (int32_t)atof(optarg);			break;
		case 'n': *vox_wid = (float)atof(optarg);			break;
		case 'o': *rot_center = (float)atof(optarg);			break;
		case 'p': *sig_s = (float)atof(optarg);			break;
		case 'q': *sig_t = (float)atof(optarg);			break;
		case 'r': *c_s = (float)atof(optarg);				break;
		case 's': *c_t = (float)atof(optarg);				break;
		case 't': *convg_thresh = (float)atof(optarg);			break;
		case 'u': *remove_rings = (int32_t)atoi(optarg);		break;
		case 'v': *remove_streaks = (int32_t)atoi(optarg);		break;
		case 'w': *restart = 1;		break;
		case '?': fprintf(debug_msg_ptr, "ERROR: Cannot recognize argument %s\n",optarg); break;
		}
	}

	if(argc-optind > 0)
		fprintf(debug_msg_ptr, "ERROR: Argument list has an error\n");
}


