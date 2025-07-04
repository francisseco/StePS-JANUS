/********************************************************************************/
/*  StePS - STEreographically Projected cosmological Simulations                */
/*    Copyright (C) 2017-2019 Gabor Racz                                        */
/*                                                                              */
/*    This program is free software; you can redistribute it and/or modify      */
/*    it under the terms of the GNU General Public License as published by      */
/*    the Free Software Foundation; either version 2 of the License, or         */
/*    (at your option) any later version.                                       */
/*                                                                              */
/*    This program is distributed in the hope that it will be useful,           */
/*    but WITHOUT ANY WARRANTY; without even the implied warranty of            */
/*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             */
/*    GNU General Public License for more details.                              */
/********************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <math.h>
#include <omp.h>
#include <time.h>
#include <algorithm>
#include <unistd.h>
#include "mpi.h"
#include "global_variables.h"
#ifdef HAVE_HDF5
#include <hdf5.h>
#endif

#ifdef USE_SINGLE_PRECISION
typedef float REAL;
#else
typedef double REAL;
#endif
//JANUS
int JanusAcc;
int t,N,el,hl;
int e[2202][4];
int H[2202][4];
REAL SOFT_CONST[8];
REAL w[3];
double a_max;
REAL* x;
REAL* v;
REAL* F;
bool* IN_CONE;
double h, h_min, h_max, T, t_next, t_bigbang;
REAL ACC_PARAM;
double FIRST_T_OUT, H_OUT; //First output time, output frequency in Gy
double rho_crit; //Critical density
REAL mass_in_unit_sphere; //Mass in unit sphere
bool ForceError = false;

int n_GPU; //number of cuda capable GPUs
int numtasks, rank; //Variables for MPI
int N_mpi_thread; //Number of calculated forces in one MPI thread
int ID_MPI_min, ID_MPI_max; //max and min ID of of calculated forces in one MPI thread
MPI_Status Stat;
int BUFFER_start_ID;
REAL* F_buffer;

REAL x4, err, errmax;
REAL beta, ParticleRadi, rho_part, M_min;

int IS_PERIODIC, COSMOLOGY;
int COMOVING_INTEGRATION; //Comoving integration 0=no, 1=yes, used only when  COSMOLOGY=1
REAL L;
char IC_FILE[1024];
char OUT_DIR[1024];
char OUT_LST[1024]; //output redshift list file. only used when OUTPUT_TIME_VARIABLE=1
extern char __BUILD_DATE;
int IC_FORMAT; // 0: ASCII, 1:GADGET
int OUTPUT_FORMAT; // 0:ASCII, 2:HDF5
int OUTPUT_TIME_VARIABLE; // 0: time, 1: redshift
double MIN_REDSHIFT; //The minimal output redshift. Lower redshifts considered 0.
int REDSHIFT_CONE; // 0: standard output files 1: one output redshift cone file
int HAVE_OUT_LIST; // 0: output list not found. 1: output list found
double TIME_LIMIT_IN_MINS; //Simulation wall-clock time limit in minutes.
double *out_list; //Output redshits
double *r_bin_limits; //bin limints in Dc for redshift cone simulations
int out_list_size; //Number of output redshits
unsigned int N_snapshot; //number of written out snapshots

double Omega_b,Omega_lambda,Omega_dm,Omega_r,Omega_k,Omega_m,H0,Hubble_param, Decel_param, delta_Hubble_param, Hubble_tmp; //Cosmologycal parameters

double epsilon=1;
double sigma=1;
REAL* M;//Particle mass
REAL* SOFT_LENGTH; //particle softening lengths
REAL M_tmp;
double a, a_start,a_prev,a_tmp;//Scalefactor, scalefactor at the starting time, previous scalefactor
double Omega_m_eff; //Effective Omega_m
double delta_a;

//Functions for reading GADGET2 format IC
int gadget_format_conversion(void);
int load_snapshot(char *fname, int files);
int allocate_memory(void);
int reordering(void);

void read_param(FILE *param_file);
//JANUS
void step(REAL* x, REAL* v, REAL* F, int JanusAcc);
void forces(REAL* x, REAL* F, int ID_min, int ID_max, int JanusAcc);
void forces_periodic(REAL*x, REAL*F, int ID_min, int ID_max);
double friedmann_solver_start(double a0, double t0, double h, double Omega_lambda, double Omega_r, double Omega_m, double H0, double a_start);
double friedman_solver_step(double a0, double h, double Omega_lambda, double Omega_r, double Omega_m, double Omega_k, double H0);
int ewald_space(REAL R, int ewald_index[2102][4]);
double CALCULATE_decel_param(double a);
//Functions used in MPI parallelisation
void BCAST_global_parameters();

//Input/Output functions
int file_exist(char *file_name);
int dir_exist(char *dir_name);
int measure_N_part_from_ascii_snapshot(char * filename);
void read_ascii_ic(FILE *ic_file, int N);
int read_OUT_LST();
void write_redshift_cone(REAL *x, REAL *v, double *limits, int z_index, int delta_z_index, int ALL);
void write_ascii_snapshot(REAL* x, REAL *v);
void Log_write();
#ifdef HAVE_HDF5
int N_redshiftcone, HDF5_redshiftcone_firstshell;
//Functions for HDF5 I/O
void write_hdf5_snapshot(REAL *x, REAL *v, REAL *M);
void write_header_attributes_in_hdf5(hid_t handle);
void read_hdf5_ic(char *ic_file);
#endif

int main(int argc, char *argv[])
{
	//initialize MPI
	MPI_Init(&argc,&argv);
	// get number of tasks
	MPI_Comm_size(MPI_COMM_WORLD,&numtasks);
	// get my rank
	MPI_Comm_rank(MPI_COMM_WORLD,&rank);
	JanusAcc=1;
	if(rank == 0)
	{
		printf("+-----------------------------------------------------------------------------------------------+\n|   _____ _       _____   _____ \t\t\t\t\t\t\t\t|\n|  / ____| |     |  __ \\ / ____|\t\t\t\t\t\t\t\t|\n| | (___ | |_ ___| |__) | (___  \t\t\t\t\t\t\t\t|\n|  \\___ \\| __/ _ \\  ___/ \\___ \\ \t\t\t\t\t\t\t\t|\n|  ____) | ||  __/ |     ____) |\t\t\t\t\t\t\t\t|\n| |_____/ \\__\\___|_|    |_____/ \t\t\t\t\t\t\t\t|\n|StePS %s\t\t\t\t\t\t\t\t\t\t\t|\n| (STEreographically Projected cosmological Simulations)\t\t\t\t\t|\n+-----------------------------------------------------------------------------------------------+\n| Copyright (C) 2017-2019 Gabor Racz\t\t\t\t\t\t\t\t|\n|\tDepartment of Physics of Complex Systems, Eotvos Lorand University | Budapest, Hungary\t|\n|\tDepartment of Physics & Astronomy, Johns Hopkins University | Baltimore, MD, USA\t|\n|\t\t\t\t\t\t\t\t\t\t\t\t|\n|", PROGRAM_VERSION);
		printf("Build date: %s\t\t\t\t\t\t\t|\n|",  BUILD_DATE);
		printf("Compiled with: %s", COMPILER_VERSION);
		unsigned long int I;
		for(I = 0; I<10-((sizeof(COMPILER_VERSION)-1)/8); I++)
			printf("\t");
		printf("|\n+-----------------------------------------------------------------------------------------------+\n\n");
		printf("+---------------------------------------------------------------+\n| StePS comes with ABSOLUTELY NO WARRANTY.\t\t\t|\n| This is free software, and you are welcome to redistribute it\t|\n| under certain conditions. See the LICENSE file for details.\t|\n+---------------------------------------------------------------+\n\n");
	}
	char HOSTNAME_BUF[1024];
	if(rank == 0)
	{
		gethostname(HOSTNAME_BUF, sizeof(HOSTNAME_BUF));
		printf("\tRunning on %s.\n", HOSTNAME_BUF);
	}
	#ifdef USE_CUDA
	if(rank == 0)
		printf("\tUsing CUDA capable GPUs for force calculation.\n");
	#endif
	#ifdef GLASS_MAKING
	if(rank == 0)
		printf("\tGlass making.\n");
	#endif
	#ifdef USE_SINGLE_PRECISION
	if(rank == 0)
		printf("\tSingle precision (32bit) force calculation.\n");
	#else
	if(rank == 0)
		printf("\tDouble precision (64bit) force calculation.\n");
	#endif
	#ifdef PERIODIC
	if(rank == 0)
		printf("\tPeriodic boundary conditions.\n\n");
	#else
	if(rank == 0)
		printf("\tNon-periodic boundary conditions.\n\n");
	#endif
	if(numtasks != 1 && rank == 0)
	{
		printf("Number of MPI tasks: %i\n", numtasks);
	}
	int i,j;
	int CONE_ALL=0;
	N_snapshot = 0;
	OUTPUT_TIME_VARIABLE = -1;
	if( argc < 2 )
	{
		if(rank == 0)
		{
			fprintf(stderr, "Missing parameter file!\n");
			fprintf(stderr, "Call with: ./%s  <parameter file>\n", PROGRAMNAME);
		}
		return (-1);
	}
	else if(argc > 3)
	{
		if(rank == 0)
		{
			fprintf(stderr, "Too many arguments!\n");
			#ifndef USE_CUDA
				fprintf(stderr, "Call with: ./%s  <parameter file>\n", PROGRAMNAME);
			#else
				fprintf(stderr, "Call with: ./%s  <parameter file> \'i\', where \'i\' is the number of the CUDA capable GPUs per node.\nif \'i\' is not set, than one GPU per MPI task will be used.\n", PROGRAMNAME);
			#endif
		}
		return (-1);
	}
	//the rank=0 thread reads the paramfile, and bcast the variables to the other threads
	if(rank == 0)
	{
		if(file_exist(argv[1]) == 0)
		{
			fprintf(stderr, "Error: The %s parameter file does not exist!\nExiting.\n", argv[1]);
			return (-1);
		}
		FILE *param_file;
		param_file = fopen(argv[1], "r");
		read_param(param_file);
		if(dir_exist(OUT_DIR) == 0)
		{
			fprintf(stderr, "Error: The %s output directory does not exist!\nExiting.\n",OUT_DIR);
			return (-1);
		}
	}
	BCAST_global_parameters();
	#ifdef PERIODIC
		if(IS_PERIODIC < 1 || IS_PERIODIC > 2)
		{
			if(rank == 0)
				fprintf(stderr, "Error: Bad boundary condition were set in the paramfile!\nThis executable are able to deal with periodic simulation only.\nExiting.\n");
			return (-2);
		}

		if(IS_PERIODIC>1)
		{
			el = ewald_space(3.6,e);
 			if(IS_PERIODIC>2)
			{
				hl = ewald_space(8.0,H);
			}
		}
		else
		{
			el = 2202;
			for(i=0;i<el;i++)
			{
				for(j=0;j<3;j++)
				{
					e[i][j]=0.0;
				}
			}
			i=0;
			j=0;
		}

	#else
		if(IS_PERIODIC  != 0)
		{
			if(rank == 0)
				fprintf(stderr, "Error: Bad boundary condition were set in the paramfile!\nThis executable are able to run non-periodic simulations only.\nExiting.\n");
			return (-2);
		}
	#endif
	if(OUTPUT_TIME_VARIABLE != 0 && OUTPUT_TIME_VARIABLE !=1)
	{
		if(rank == 0)
			fprintf(stderr, "Error: bad OUTPUT time variable %i!\nExiting.\n", OUTPUT_TIME_VARIABLE);
		return (-2);
	}
	if(OUTPUT_TIME_VARIABLE == 1 && COSMOLOGY != 1)
	{
		if(rank == 0)
			fprintf(stderr, "Error: you can not use redshift output format in non-cosmological simulations. \nExiting.\n");
		return (-2);
	}
	if(rank == 0)
	{
		if(file_exist(OUT_LST) == 0)
		{
			HAVE_OUT_LIST = 0;
			printf("Output list not found. Using the FIRST_T_OUT and H_OUT variables for calculating the output");
		}
		else
		{
			HAVE_OUT_LIST = 1;
			printf("Output list found. Using the contents of this file for the output");
		}
		if(OUTPUT_TIME_VARIABLE == 1)
			printf(" redshifts.\n");
		else
			printf(" times.\n");
	}
	if(HAVE_OUT_LIST==1 && rank==0)
	{
		if(0 != read_OUT_LST())
		{
			fprintf(stderr, "Exiting.\n");
			return (-2);
		}
	}
	if(rank == 0)
	{
		#ifndef HAVE_HDF5
		if(IC_FORMAT != 0 && IC_FORMAT != 1)
		{
			fprintf(stderr, "Error: bad IC format!\nExiting.\n");
			return (-1);
		}
		#else
		if(IC_FORMAT < 0 || IC_FORMAT > 2)
                {
                        fprintf(stderr, "Error: bad IC format!\nExiting.\n");
                        return (-1);
                }
		#endif
		if(IC_FORMAT == 0)
		{
			printf("\nThe IC file is in ASCII format.\n");
			if(file_exist(IC_FILE) == 0)
			{
				fprintf(stderr, "Error: The %s IC file does not exist!\nExiting.\n", IC_FILE);
				return (-1);
			}
			N = measure_N_part_from_ascii_snapshot(IC_FILE);
			FILE *ic_file = fopen(IC_FILE, "r");
			read_ascii_ic(ic_file, N);
		}
		if(IC_FORMAT == 1)
		{
			int files;
			printf("\nThe IC file is in Gadget format.\nThe IC determines the box size.\n");
			files = 1;      /* number of files per snapshot */
			if(file_exist(IC_FILE) == 0)
			{
				fprintf(stderr, "Error: The %s IC file does not exist!\nExiting.\n", IC_FILE);
				return (-1);
			}
			load_snapshot(IC_FILE, files);
			reordering();
			gadget_format_conversion();
		}
		#ifdef HAVE_HDF5
		if(IC_FORMAT == 2)
		{
			printf("\nThe IC is in HDF5 format\n");
			if(file_exist(IC_FILE) == 0)
			{
				fprintf(stderr, "Error: The %s IC file does not exist!\nExiting.\n", IC_FILE);
				return (-1);
			}
			read_hdf5_ic(IC_FILE);
		}
		#endif
		if(REDSHIFT_CONE == 1 && COSMOLOGY != 1)
		{
			fprintf(stderr, "Error: you can not use redshift cone output format in non-cosmological simulations. \nExiting.\n");
			return (-2);
		}
		if(REDSHIFT_CONE == 1 && OUTPUT_TIME_VARIABLE != 1)
		{
			fprintf(stderr, "Error: you must use redshift output format in redshift cone simulations. \nExiting.\n");
			return (-2);
		}
		if(REDSHIFT_CONE == 1)
		{
			//Allocating memory for the bool array
			IN_CONE = new bool[N];
			std::fill(IN_CONE, IN_CONE+N, false ); //setting every element to false
			#ifdef HAVE_HDF5
			HDF5_redshiftcone_firstshell = 1;
			N_redshiftcone = 0; //number of particles written out to the redshiftcone
			#endif
		}
		//Rescaling speeds. We are using the same convention that the Gadget uses: http://wwwmpa.mpa-garching.mpg.de/gadget/gadget-list/0113.html
		if(COSMOLOGY == 1 && COMOVING_INTEGRATION == 1)
		{
			for(i=0;i<N;i++)
			{
				if (M[i] < 0 )//JANUS Init Acc if neg mass, scale the velocity up can be done here
				{
				v[3*i] = v[3*i]/sqrt(a_start)/UNIT_V;
				v[3*i+1] =v[3*i+1]/sqrt(a_start)/UNIT_V;
				v[3*i+2] =v[3*i+2]/sqrt(a_start)/UNIT_V;
				}
				else
				{
				v[3*i] = v[3*i]/sqrt(a_start)/UNIT_V;
				v[3*i+1] = v[3*i+1]/sqrt(a_start)/UNIT_V;
				v[3*i+2] = v[3*i+2]/sqrt(a_start)/UNIT_V;
				}
			}
		}
		else if(COSMOLOGY == 1 && COMOVING_INTEGRATION == 0)
		{
			for(i=0;i<N;i++)
			{
				if (M[i] < 0 )//JANUS Init Acc if neg mass, scale the velocity up can be done here
				{
				v[3*i] = v[3*i]/sqrt(a_start)/UNIT_V;
				v[3*i+1] =v[3*i+1]/sqrt(a_start)/UNIT_V;
				v[3*i+2] =v[3*i+2]/sqrt(a_start)/UNIT_V;
				}
				else
				{
				v[3*i] = v[3*i]/sqrt(a_start)/UNIT_V;
				v[3*i+1] = v[3*i+1]/sqrt(a_start)/UNIT_V;
				v[3*i+2] = v[3*i+2]/sqrt(a_start)/UNIT_V;
				}
		}
		if(numtasks > 1)
		{
			F_buffer = (REAL*)malloc(3*(N/numtasks)*sizeof(REAL));
		}
	}
	#ifdef USE_CUDA
	if(argc == 3)
	{
		n_GPU = atoi( argv[2] );
		if(rank == 0)
			printf("Using %i cuda capable GPU per MPI task.\n", n_GPU);
	}
	else
	{
		n_GPU = 1;
	}
	#endif
	//Bcasting the number of particles
	MPI_Bcast(&N,1,MPI_INT,0,MPI_COMM_WORLD);
	if(rank == 0)
		N_mpi_thread = (N/numtasks) + (N%numtasks);
	else
		N_mpi_thread = N/numtasks;
	if(rank != 0)
	{
		//Allocating memory for the particle datas on the rank != 0 MPI threads
		x = (REAL*)malloc(3*N*sizeof(REAL)); //Allocating memory fo the coordinates
		//v = (REAL*)malloc(3*N*sizeof(REAL)); //Allocating memory for the velocities
		F = (REAL*)malloc(3*N_mpi_thread*sizeof(REAL));//There is no need to allocate for N forces. N/numtasks should be enough
		M = (REAL*)malloc(N*sizeof(REAL));
		SOFT_LENGTH = (REAL*)malloc(N*sizeof(REAL));

	}
	//Bcasting the ICs to the rank!=0 threads
#ifdef USE_SINGLE_PRECISION
	MPI_Bcast(x,3*N,MPI_FLOAT,0,MPI_COMM_WORLD);
        MPI_Bcast(M,N,MPI_FLOAT,0,MPI_COMM_WORLD);
#else
	MPI_Bcast(x,3*N,MPI_DOUBLE,0,MPI_COMM_WORLD);
	MPI_Bcast(M,N,MPI_DOUBLE,0,MPI_COMM_WORLD);
#endif
	#ifdef GLASS_MAKING
	//setting all velocities to zero
	int k;
	if(rank == 0)
		printf("Glass making: setting all velocities to zero.\n");
	for(i=0; i<N; i++)
	{
		for(k=0; k<3; k++)
		{
			v[3*i+k] = 0.0;
		}
	}
	#endif
	//Critical density and particle masses
	if(COSMOLOGY == 1)
	{
	if(COMOVING_INTEGRATION == 1)//JANUS
	{
		Omega_m = Omega_b+Omega_dm;
		Omega_k = 1.-Omega_m-Omega_lambda-Omega_r;
		rho_crit = 3.0*H0*H0/(8.0*pi);
		mass_in_unit_sphere = (REAL) (4.0*pi*rho_crit*Omega_m/3.0);
		M_tmp = Omega_dm*rho_crit*pow(L, 3.0)/((REAL) N);
		#ifdef PERIODIC
		if(IC_FORMAT == 1)
		{
			if(rank == 0)
				printf("Every particle has the same mass in periodic cosmological simulations, if the input is in GADGET format.\nM=%.10f*10e+11M_sol\n", M_tmp);
			for(i=0;i<N;i++)//Every particle has the same mass in periodic cosmological simulations, if the IC is in GADGET format
			{
				M[i] = M_tmp;
			}
		}
		//Calculating the total mean desity of the simulation volume
		//in here we sum the total particle mass with Kahan summation
		REAL rho_mean_full_box = 0.0;
		REAL Kahan_compensation = 0.0;
		REAL Kahan_t, Kahan_y;
		for(i=0;i<N;i++)
		{
			Kahan_y = fabs(M[i]) - Kahan_compensation;//JANUS
			Kahan_t = rho_mean_full_box + Kahan_y;
			Kahan_compensation = (Kahan_t - rho_mean_full_box) - Kahan_y;
			rho_mean_full_box = Kahan_t;
		}
		rho_mean_full_box /= pow(L, 3.0); //dividing the total mass by the simulation volume
		if(fabs(rho_mean_full_box/(rho_crit*Omega_dm) - 1) > 1e-5)//JANUS
		{
			 fprintf(stderr, "Error: The particle masses are inconsistent with the cosmological parameters!\nrho_part/rho_cosm = %.6f\nExiting.\n", rho_mean_full_box/(rho_crit*Omega_dm));
			 return (-1);
		}
		#endif
	}
	else
	{
		if(IS_PERIODIC>0)
		{
			if(rank == 0)
				fprintf(stderr, "Error: COSMOLOGY = 1, IS_PERIODOC>0 and COMOVING_INTEGRATION = 0!\nThis code can not handle non-comoving periodic cosmological simulations.\nExiting.\n");
			return (-1);
		}
		if(rank == 0)
			printf("COSMOLOGY = 1 and COMOVING_INTEGRATION = 0:\nThis run will be in non-comoving coodinates. This means that, this will be a full Newtonian cosmological simulation. Make sure that you set the correct parameters at the IC making.\na_max is used as maximal time in Gy in the parameter file.\n\n");
		Omega_m = Omega_b+Omega_dm;
		Omega_k = 1.-Omega_m-Omega_lambda-Omega_r;
		rho_crit = 3*H0*H0/(8*pi);
	}
	}
	else
	{
		if(rank == 0)
			printf("Running non-cosmological gravitational N-body simulation.\n");
	}
	//Searching the minimal mass particle
	if(rank == 0)
	{
		M_min = fabs(M[0]);//JANUS
		for(i=0;i<N;i++)
		{
			if(M_min>fabs(M[i]))//JANUS
			{
				M_min = fabs(M[i]);//JANUS
			}
		}
		rho_part = M_min/(4.0*pi*pow(ParticleRadi, 3.0) / 3.0);
	}
	//Calculating the softening length for each particle:
	REAL const_beta = 3.0/rho_part/(4.0*pi);
	printf("Calculating the softening lengths...\n");
	for(i=0;i<N;i++)
	{//JANUS
		SOFT_LENGTH[i] = cbrt(fabs(M[i])*const_beta); //setting up the softening length for each particle
	}
	printf("...done\n");
#ifdef USE_SINGLE_PRECISION
	MPI_Bcast(&M_min,1,MPI_FLOAT,0,MPI_COMM_WORLD);
	MPI_Bcast(&rho_part,1,MPI_FLOAT,0,MPI_COMM_WORLD);
	MPI_Bcast(SOFT_LENGTH,N,MPI_FLOAT,0,MPI_COMM_WORLD);
#else
	MPI_Bcast(&M_min,1,MPI_DOUBLE,0,MPI_COMM_WORLD);
	MPI_Bcast(&rho_part,1,MPI_DOUBLE,0,MPI_COMM_WORLD);
	MPI_Bcast(SOFT_LENGTH,N,MPI_DOUBLE,0,MPI_COMM_WORLD);
#endif
	beta = ParticleRadi;
	a=a_start;//scalefactor
	t_next = 0.;
	T = 0.0;
	REAL Delta_T_out = 0;
	if(COSMOLOGY == 0)
		a=1;//scalefactor
	int out_z_index = 0;
	int delta_z_index = 1;
	//Calculating initial time on the task=0 MPI thread
	if(rank == 0)
	{
	if(COSMOLOGY == 1)
	{
		a = a_start;
		a_tmp = a;
		if(COMOVING_INTEGRATION == 1)
		{
			printf("a_start=%.9f\tz=%.9f\n", a, 1/a-1);
		}
		T = friedmann_solver_start(1,0,h_min*0.05,Omega_lambda,Omega_r,Omega_m,H0,a_start);
		if(HAVE_OUT_LIST == 0)
		{
			if(OUTPUT_TIME_VARIABLE==0)
			{
				Delta_T_out = H_OUT/UNIT_T; //Output frequency in internal time units
				if(FIRST_T_OUT >= T) //Calculating first output time
				{
					t_next = FIRST_T_OUT/UNIT_T;
				}
				else
				{
					t_next = T+Delta_T_out;
				}
			}
			else
			{
				Delta_T_out = H_OUT; //Output frequency in redshift
				if(FIRST_T_OUT >= T) //Calculating first output redshift
				{
					t_next = FIRST_T_OUT;
				}
				else
				{
					t_next = FIRST_T_OUT - Delta_T_out;
				}
			}
		}
		else
		{
			if(OUTPUT_TIME_VARIABLE==1)
			{
				if(1.0/a-1.0 > out_list[0])
				{
					t_next = out_list[0];
				}
				else
				{
					i=0;
					while(out_list[i] > 1.0/a-1.0)
					{
						t_next = out_list[i];
						i++;
						if(i == out_list_size && out_list[i] > 1.0/a-1.0)
						{
							fprintf(stderr, "Error: No valid output redshift!\nExiting.\n");
							return (-2);
						}
					}
				}
			}
			else
			{
				if(T < out_list[0])
				{
					t_next = out_list[0];
				}
				else
				{
					i=0;
					while(out_list[i] < T)
					{
						t_next = out_list[i];
						i++;
						if(i == out_list_size && out_list[i] < T)
						{
							fprintf(stderr, "Error: No valid output time found in the OUT_LST file!\nExiting.\n");
							return (-2);
						}
					}
				}
			}
		}
		if(COMOVING_INTEGRATION == 1)
		{
		printf("Initial time:\tt_start = %.10fGy\nInitial scalefactor:\ta_start = %.8f\nMaximal scalefactor:\t%.8f\n\n", T*UNIT_T, a, a_max);
		}
		if(COMOVING_INTEGRATION == 0)
		{
			Hubble_param = 0;
			a_tmp = 0;
			a_max = a_max/UNIT_T;
			a = 1;
			printf("Initial time:\tt_start = %.10fGy\nMaximal time:\t%.8f\n\n", T*UNIT_T, a_max*UNIT_T);
		}
	}
	else
	{
		a = 1;
		Hubble_param = 0;
		T = 0.0; //If we do not running cosmological simulations, the initial time will be 0.
		printf("t_start = %f\tt_max = %f\n", T, a_max);
		a_tmp = 0;
		Delta_T_out = H_OUT;
		if(HAVE_OUT_LIST==0)
		{
			t_next = T+Delta_T_out;
		}
		else
		{
			i = 0;
			while(out_list[i] < 0.0)
			{
				t_next=out_list[i];
				i++;
				if(i == out_list_size && out_list[i] < T)
				{
					fprintf(stderr, "Error: No valid output time found in the OUT_LST file!\nExiting.\n");
					return (-2);
				}
			}
		}
	}
	}
	//Bcasting the initial time and other variables
	MPI_Bcast(&t_next,1,MPI_DOUBLE,0,MPI_COMM_WORLD);
	MPI_Bcast(&T,1,MPI_DOUBLE,0,MPI_COMM_WORLD);
	double SIM_omp_start_time;
	//Timing
	SIM_omp_start_time = omp_get_wtime();
	//Timing
	if(rank == 0)
		printf("Initial force calculation...\n");
					printf("debug Rank %i \n", JanusAcc); //JANUS debug output, just in case
	//Initial force calculation
	if(rank==0)
	{
		JanusAcc=1;//JANUS
		ID_MPI_min = 0;
		ID_MPI_max = (N%numtasks) + (rank+1)*(N/numtasks)-1;
		#ifndef PERIODIC
			forces(x, F, ID_MPI_min, ID_MPI_max,JanusAcc);//JANUS
			
			printf("debug initial \f");
		#else
			forces_periodic(x, F, ID_MPI_min, ID_MPI_max);
		#endif
	}
	else
	{
		//JANUS
		
		ID_MPI_min = (N%numtasks) + (rank)*(N/numtasks);
		ID_MPI_max = (N%numtasks) + (rank+1)*(N/numtasks)-1;
		#ifndef PERIODIC
			forces(x, F, ID_MPI_min, ID_MPI_max,JanusAcc);//JANUS
			printf("2 debug Rank %i \n", JanusAcc);
		#else
			forces_periodic(x, F, ID_MPI_min, ID_MPI_max);
		#endif
	}
	//if the force calculation is finished, the calculated forces should be collected into the rank=0 thread`s F matrix
	if(rank !=0)
	{
#ifdef USE_SINGLE_PRECISION
		MPI_Send(F, 3*N_mpi_thread, MPI_FLOAT, 0, rank, MPI_COMM_WORLD);//JANUS
#else
		MPI_Send(F, 3*N_mpi_thread, MPI_DOUBLE, 0, rank, MPI_COMM_WORLD);
#endif
	}
	else
	{
		if(numtasks > 1)
		{
			for(i=1; i<numtasks;i++)
			{
				BUFFER_start_ID = i*(N/numtasks)+(N%numtasks);
#ifdef USE_SINGLE_PRECISION
				MPI_Recv(F_buffer, 3*(N/numtasks), MPI_FLOAT, i, i, MPI_COMM_WORLD, &Stat);
#else
				MPI_Recv(F_buffer, 3*(N/numtasks), MPI_DOUBLE, i, i, MPI_COMM_WORLD, &Stat);
#endif
				for(j=0; j<(N/numtasks); j++)
				{
					F[3*(BUFFER_start_ID+j)] = F_buffer[3*j];
					F[3*(BUFFER_start_ID+j)+1] = F_buffer[3*j+1];
					F[3*(BUFFER_start_ID+j)+2] = F_buffer[3*j+2];
				}
			}
		}
	}
	//The simulation is starting...
	//Calculating the initial Hubble parameter, using the Friedmann-equations
	if(COSMOLOGY == 1)
	{
		Hubble_tmp = H0*sqrt(Omega_m*pow(a, -3)+Omega_r*pow(a, -4)+Omega_lambda+Omega_k*pow(a, -2));
		Hubble_param = H0*sqrt(Omega_m*pow(a, -3)+Omega_r*pow(a, -4)+Omega_lambda+Omega_k*pow(a, -2));
		if(rank == 0)
			printf("Initial Hubble-parameter from the cosmological parameters:\nH(z=%f) = %fkm/s/Mpc\n\n", 1.0/a-1.0, Hubble_param*UNIT_V);
	}
	if(COSMOLOGY == 0 || COMOVING_INTEGRATION == 0)
	{
		Hubble_param = 0;
	}
	if(rank == 0)
	{
		h = calculate_init_h();
		if(h>h_max)
                {
			h=h_max;
                }
		else if(h<h_min)
		{
			h = h_min;
		}
	}
	MPI_Bcast(&h,1,MPI_DOUBLE,0,MPI_COMM_WORLD);
	if(rank == 0)
		printf("The simulation is starting...\n");
	REAL T_prev,Hubble_param_prev;
	T_prev = T;
	Hubble_param_prev = Hubble_param;
	//Main loop JANUS edited
	for(t=0; a_tmp<a_max; t++)
	{
		if(rank == 0)
		{
		printf("\n\n------------------------------------------------------------------------------------------------\n");
		JanusAcc=JanusAcc+1;
		if(COSMOLOGY == 1)
                {
			if(COMOVING_INTEGRATION == 1)
			{
				if(h*UNIT_T >= 1.0)
                        		printf("Timestep %i, t=%.8fGy, h=%fGy, a=%.8f, H=%.8fkm/s/Mpc, z=%.8f:\n", t, T*UNIT_T, h*UNIT_T, a, Hubble_param*UNIT_V, 1.0/a-1.0);
				else
					printf("Timestep %i, t=%.8fGy, h=%fMy, a=%.8f, H=%.8fkm/s/Mpc, z=%.8f:\n", t, T*UNIT_T, h*UNIT_T*1000.0, a, Hubble_param*UNIT_V, 1.0/a-1.0);
			}
			else
			{
				if(h*UNIT_T >= 1.0)
					printf("Timestep %i, t=%.8fGy, h=%fGy\n", t, T*UNIT_T, h*UNIT_T);
				else
					printf("Timestep %i, t=%.8fGy, h=%fMy\n", t, T*UNIT_T, h*UNIT_T*1000.0);
			}
                }
                else
                {
                        printf("Timestep %i, t=%f, h=%f:\n", t, T, h);
                }
		}
		Hubble_param_prev = Hubble_param;
		T_prev = T;
		T = T+h;
		step(x, v, F,JanusAcc);
		if(rank == 0)
		{
			Log_write();	//Writing logfile
			if(HAVE_OUT_LIST == 0)
			{
				if(OUTPUT_TIME_VARIABLE == 0)
				{
					if(T > t_next)
					{
						if(OUTPUT_FORMAT == 0)
							write_ascii_snapshot(x, v);
						#ifdef HAVE_HDF5
						if(OUTPUT_FORMAT == 2)
							write_hdf5_snapshot(x, v, M);
						#endif
						t_next+=Delta_T_out;
						printf("...done.\n");
					}
				}
				else
				{
					if( 1.0/a-1.0 < t_next)
					{
						if(OUTPUT_FORMAT == 0)
							write_ascii_snapshot(x, v);
						#ifdef HAVE_HDF5
						if(OUTPUT_FORMAT == 2)
							write_hdf5_snapshot(x, v, M);
						#endif
						t_next-=Delta_T_out;
						if(COSMOLOGY == 1)
						{
							printf("t = %f Gy\n\th=%f Gy\n", T*UNIT_T, h*UNIT_T);
						}
						else
						{
							printf("t = %f\n\terr_max = %e\th=%f\n", T, errmax, h);
						}
					}
				}
			}
			else
			{
				if(OUTPUT_TIME_VARIABLE == 1)
				{
					if( 1.0/a-1.0 < t_next)
					{
						if(REDSHIFT_CONE != 1)
						{
							if(OUTPUT_FORMAT == 0)
								write_ascii_snapshot(x, v);
							#ifdef HAVE_HDF5
							if(OUTPUT_FORMAT == 2)
								write_hdf5_snapshot(x, v, M);
							#endif
							out_z_index += delta_z_index;
							t_next = out_list[out_z_index];
						}
						else
						{
							if(a_tmp >= a_max)
							{
								CONE_ALL = 1;
								printf("Last timestep.\n");
								if(OUTPUT_FORMAT == 0)
									write_ascii_snapshot(x, v);
								#ifdef HAVE_HDF5
								if(OUTPUT_FORMAT == 2)
									write_hdf5_snapshot(x, v, M);
								#endif
							}
							write_redshift_cone(x, v, r_bin_limits, out_z_index, delta_z_index, CONE_ALL);
							if(1.0/a-1.0 <= out_list[out_z_index+delta_z_index])
							{
								if( (out_z_index+delta_z_index+8) < out_list_size)
									delta_z_index += 8;
								else
									CONE_ALL = 1;
							}
							if(CONE_ALL == 1)
							{
								t_next = 0.0;
							}
							else
							{
								out_z_index += delta_z_index;
								t_next = out_list[out_z_index];
							}
							if(MIN_REDSHIFT>t_next && CONE_ALL != 1)
							{
								CONE_ALL = 1;
								printf("Warning: The simulation reached the minimal z = %f redshift. After this point the z=0 coordinates will be written out with redshifts taken from the input file. This can cause inconsistencies, if this minimal redshift is not low enough.\n", MIN_REDSHIFT);
								t_next = 0.0;
							}
						}
					}
				}
				else
				{
					if(T >= t_next)
					{
						if(OUTPUT_FORMAT == 0)
							write_ascii_snapshot(x, v);
						#ifdef HAVE_HDF5
						if(OUTPUT_FORMAT == 2)
							write_hdf5_snapshot(x, v, M);
						#endif
						out_z_index += delta_z_index;
						t_next = out_list[out_z_index];
						if(COSMOLOGY == 1)
						{
							printf("t = %f Gy\n\th=%f Gy\n", T*UNIT_T, h*UNIT_T);
						}
						else
						{
							printf("t = %f\n\terr_max = %e\th=%f\n", T, errmax, h);
						}
					}
				}
			}
			h = (double) pow(2*ACC_PARAM/errmax, 0.5);
			if(h<h_min)
			{
				h=h_min;
			}
			else if(h>h_max)
			{
				h=h_max;
			}
			if((h+T > t_next) && OUTPUT_TIME_VARIABLE == 0)
			{
				h = t_next-T+(1E-9*h_min);
			}
		}
		MPI_Bcast(&h,1,MPI_DOUBLE,0,MPI_COMM_WORLD);
		fflush(stdout);
		MPI_Barrier(MPI_COMM_WORLD);
		if(ForceError == true)
		{
			if(rank == 0)
				printf("\nFatal error has been detected in the force calculation.\n Exiting...\n");
			break;
		}
		if( TIME_LIMIT_IN_MINS != 0 && (omp_get_wtime()-SIM_omp_start_time)/60.0 >= TIME_LIMIT_IN_MINS)
		{
			if(rank == 0)
				printf("\nSimulation wall-clock time limit reached (%.1fmin >= %.1fmin). Stopping...\n", (omp_get_wtime()-SIM_omp_start_time)/60.0, TIME_LIMIT_IN_MINS);
			break;
		}
	}
	if(OUTPUT_TIME_VARIABLE == 0 && rank == 0)
	{
		if(OUTPUT_FORMAT == 0)
			write_ascii_snapshot(x, v); //writing output
		#ifdef HAVE_HDF5
		if(OUTPUT_FORMAT == 2)
			write_hdf5_snapshot(x, v, M);
		#endif
	}
	if(rank == 0)
	{
		printf("\n\n------------------------------------------------------------------------------------------------\n");
		printf("The simulation ended. The final state:\n");
		if(COSMOLOGY == 1)
		{
			if(COMOVING_INTEGRATION == 1)
			{
				printf("Timestep %i, t=%.8fGy, h=%fMy, a=%.8f, H=%.8fkm/s/Mpc, z=%.8f\n", t, T*UNIT_T, h*UNIT_T*1000.0, a, Hubble_param*UNIT_V, 1.0/a-1.0);

				double a_end, b_end;
				a_end = (Hubble_param - Hubble_param_prev)/(a-a_prev);
				b_end = Hubble_param_prev-a_end*a_prev;
				double H_end = a_max*a_end+b_end;
				a_end = (T - T_prev)/(a-a_prev);
			        b_end = T_prev-a_end*a_prev;
				double T_end = a_max*a_end+b_end;
				printf("\nAt a = %f state, with linear interpolation:\n",a_max);
				printf("t=%.8fGy, a=%.8f, H=%.8fkm/s/Mpc\n\n", T_end*UNIT_T, a_max, H_end*UNIT_V);
			}
			else
			{
				printf("Timestep %i, t=%.8fGy, h=%fGy\n", t, T*UNIT_T, h*UNIT_T);
			}
		}
		else
		{
			printf("Timestep %i, t=%f, h=%f, a=%f:\n", t, T, h, a);
		}
		//Timing
		double SIM_omp_end_time = omp_get_wtime();
		//Timing
		printf("Wall-clock time of the simulation = %fs\n", SIM_omp_end_time-SIM_omp_start_time);
	}
	// done with MPI
	MPI_Finalize();
	return 0;
}
