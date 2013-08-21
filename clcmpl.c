// compiles OpenCL kernels into binaries
#include <stdio.h>
#include <stdlib.h>

#include "cl_error_codes.h"

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/opencl.h>
#endif

static void check_cl_error(const char * message, const int error)
{
	if (error != CL_SUCCESS)
	{
		char ers[42];
		get_cl_error_string(ers, error);
		fprintf(stderr, "OpenCL> Error %s: error code %d (%s)\n", message, error, ers);
		exit(EXIT_FAILURE);
	}
}

typedef enum {CPU, GPU} t_device_type_id;	// CPU=0, GPU=1

void main(int argc, char **argv)
{
	size_t i, j;
	char *input_filename = NULL, *output_filename = NULL;
	char **cl_compiler_options = (char **)malloc(argc*sizeof(char*));
	for (i=0; i<argc; i++) cl_compiler_options[i] = NULL;
	int cl_compiler_options_count = 0;
	t_device_type_id device_type_id = GPU;
	for (i=1; i<argc;)
	{
		if (argv[i][0]=='-')
		{
			if (argv[i][1]=='o')
			{
				i++;
				output_filename = argv[i];
			}
			else
			if (argv[i][1]=='d')
			{
				if (argv[i][2]=='G')
					device_type_id = GPU;
				else
				if (argv[i][2]=='C')
					device_type_id = CPU;
				else
				{
					cl_compiler_options[i] = argv[i];
					cl_compiler_options_count++;
				}
			}
		}
		else
			input_filename = argv[i];
		i++;
	}
	if (input_filename==NULL)
	{
		fputs("Specify OpenCL source filename!\n", stderr);
		exit(EXIT_FAILURE);
	}
	if (output_filename==NULL)
	{
		output_filename = (char *)malloc(sizeof(char)*6);
		strcpy(output_filename, "a.out");
	}
	
	FILE * kf = fopen(input_filename, "r");
	if (kf==NULL) { fputs("Error opening kernel source file!\n", stderr); exit(EXIT_FAILURE); }
	
	size_t kfs = 0, ifc;
	char c, s[11] = "#include \"", include_filename[42];
	int ifsize;
	i = 0;
	while ((c=fgetc(kf))!=EOF)
	{
		kfs++;
		if (c==s[0])
		{
			i = 1;
			while ((c=fgetc(kf))!=EOF)
			{
				kfs++;
				if (c!=s[i]) break;
				i++;
				if (i>9) break;
			}
		}
		if (i==10)
		{
			ifc = kfs - 10;
			ifsize = 0;
			while ((c=fgetc(kf))!=EOF)
			{
				kfs++;
				if (c=='"')
				{
					include_filename[ifsize] = '\0';
					ifsize++;
					break;
				}
				include_filename[ifsize] = c;
				ifsize++;
			}
			i = 0;
		}
		if (c==EOF) break;
	}
	rewind(kf);
	
	char * program_src;
	if (ifsize>1)
	{
		char full_include_filename[64];
		for (j=0; input_filename[j]!='\0'; j++);
		for (; ((j>0)&&(input_filename[j]!='\\')&&(input_filename[j]!='/')); j--);
		if (j>0)
		{
			for (i=0; i<j+1; i++)
				full_include_filename[i] = input_filename[i];
			full_include_filename[++i]='\0';
		}
		else
			full_include_filename[0]='\0';
//		#ifdef _WIN32
//		full_include_filename[i]='\\';
//		#endif
		strcat(full_include_filename, include_filename);
		FILE * include_file = fopen(full_include_filename, "r");
		if (include_file==NULL)
		{
			fputs("Error opening '", stderr);
			fputs(full_include_filename, stderr);
			fputs("'!\n", stderr);
			exit(EXIT_FAILURE);
		}
		while (fgetc(include_file)!=EOF) kfs++; rewind(include_file); kfs -= ifsize+10;
		program_src = (char *)malloc(kfs);
		for (i=0; i<ifc; i++) program_src[i] = fgetc(kf);
		for (j=0; j<ifsize+10; j++) fgetc(kf);
		while ((c=fgetc(include_file))!=EOF) { program_src[i] = c; i++; }
		while ((c=fgetc(kf))!=EOF) { program_src[i] = c; i++; }
		fclose(include_file);
//		FILE * newfile = fopen("newkernel.cl", "w");
//		for (i=0; i<kfs; i++) fprintf(newfile, "%c", program_src[i]);
//		fclose(newfile);
	}
	else
	{
		program_src = (char *)malloc(kfs);
		for (i=0; i<kfs; i++) program_src[i] = fgetc(kf);
	}
	free(input_filename); input_filename = NULL;
	fclose(kf);
	
	int err;
	cl_platform_id platform_id;
	cl_device_id device_id;
	
	check_cl_error("getting platform id", clGetPlatformIDs(1, &platform_id, NULL));
	
	cl_context_properties cps[3] = { CL_CONTEXT_PLATFORM, (cl_context_properties)platform_id, 0 };
	switch (device_type_id)
	{
		case CPU: err = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_CPU, 1, &device_id, NULL); break;
		case GPU: err = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_GPU, 1, &device_id, NULL);
	}
	check_cl_error("getting device id", err);
	
	cl_context context = clCreateContext(cps, 1, &device_id, NULL, NULL, &err);
	check_cl_error("creating context", err);
	
	cl_program program = clCreateProgramWithSource(context, 1, (const char **)&program_src, (const size_t *)&kfs, &err);
	check_cl_error("creating program", err);
	free(program_src); program_src = NULL;
	
	char * build_options = (char *)malloc(sizeof(char)*42*cl_compiler_options_count);
	build_options[0] = '\0';
	for (i=j=0; i<cl_compiler_options_count; i++)
	{
		while (cl_compiler_options[j]==NULL) j++;
		strcat (build_options, cl_compiler_options[j]);
		strcat (build_options, " ");
	}
	free(cl_compiler_options); cl_compiler_options = NULL;
	err = clBuildProgram(program, 0, NULL, (const char *)build_options, NULL, NULL);
	free(build_options); build_options = NULL;
	
	size_t log_size;
	i = clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
	check_cl_error("getting program build log size", i);
	if (log_size>2)
	{
		fputs("OpenCL> Build log:\n", stderr);
		char * build_log = (char *)malloc(log_size);
		i = clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, log_size, build_log, NULL);
		check_cl_error("getting program build log", i);
		fputs(build_log, stderr);
		free(build_log); build_log = NULL;
	}
	check_cl_error("building program", err);
	
	size_t binary_size;
	err = clGetProgramInfo(program, CL_PROGRAM_BINARY_SIZES, sizeof(size_t), &binary_size, NULL);
	check_cl_error("getting binary size", err);
	unsigned char * binary;
	binary = (unsigned char *)malloc(binary_size*sizeof(char));
	err = clGetProgramInfo(program, CL_PROGRAM_BINARIES, sizeof(char *), &binary, NULL);
	check_cl_error("getting binary", err);
	
	kf = fopen(output_filename, "w"); //free(output_filename); output_filename = NULL;	//! WTF HERE ???
	if (kf==NULL) { fputs("Error opening kernel binary file!\n", stderr); exit(EXIT_FAILURE); }
	for (i=0; i<binary_size; i++) fputc((int)binary[i], kf); fclose(kf);
}
