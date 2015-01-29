/* compiles OpenCL kernels into binaries */

#include "cl_error.h"

#ifdef __APPLE__
	#include <OpenCL/opencl.h>
#else
	#include <CL/opencl.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

//#define int cl_int
#define uint cl_uint

cl_context context;
cl_device_id * devices;
uint ndevices;

char * opencl_get_platform_info_string(cl_platform_id platform, cl_platform_info info, char **str)
{
	size_t s;
	free(*str);
	if (clGetPlatformInfo(platform, info, 0, NULL, &s))
	{
		*str = (char *)malloc(8);
		sprintf(*str, "UNKNOWN");
	}
	else
	{
		*str = (char *)malloc(s);
		if (clGetPlatformInfo(platform, info, s, *str, NULL))
		{
			*str = (char *)realloc(*str, 8);
			sprintf(*str, "UNKNOWN");
		}
	}
	return *str;
}

char * opencl_get_device_info_string(cl_device_id device, cl_device_info info, char **str)
{
	size_t s;
	free(*str);
	if (clGetDeviceInfo(device, info, 0, NULL, &s))
	{
		*str = (char *)malloc(8);
		sprintf(*str, "UNKNOWN");
	}
	else
	{
		*str = (char *)malloc(s);
		if (clGetDeviceInfo(device, info, s, *str, NULL))
		{
			*str = (char *)realloc(*str, 8);
			sprintf(*str, "UNKNOWN");
		}
	}
	return *str;
}

void list()
{
	cl_uint i, j, n_platforms, n_devices;
	cl_platform_id *platforms;
	cl_device_id *devices;
	cl_device_type device_type;
	char *str = NULL;
	int ret = clGetPlatformIDs(0, NULL, &n_platforms);
#ifdef CL_PLATFORM_NOT_FOUND_KHR
	if (ret == CL_PLATFORM_NOT_FOUND_KHR)
		puts("No OpenCL platforms found.");
#endif
	if (!ret)
	{
		platforms = (cl_platform_id *)malloc(sizeof(cl_platform_id) * n_platforms);
		if (!clGetPlatformIDs(n_platforms, platforms, NULL))
			for (i=0; i<n_platforms; i++)
			{
				printf(" Platform #%u: ", i);
				opencl_get_platform_info_string(platforms[i], CL_PLATFORM_NAME, &str); printf("%s ", str);
				opencl_get_platform_info_string(platforms[i], CL_PLATFORM_VERSION, &str); printf("(%s)\n", str);
				if (!clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_ALL, 0, NULL, &n_devices))
				{
					devices = (cl_device_id *)malloc(sizeof(cl_device_id) * n_devices);
					if (!clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_ALL, n_devices, devices, NULL))
						for (j=0; j<n_devices; j++)
						{
							if (j == n_devices - 1)
								printf("  \\_ Device #%u: ", j);
							else
								printf("  |_ Device #%u: ", j);
							if (clGetDeviceInfo(devices[j], CL_DEVICE_TYPE, sizeof(cl_device_type), &device_type, NULL))
								printf("UNKNOWN");
							else
								switch (device_type)
								{
									case CL_DEVICE_TYPE_CPU:
										printf("CPU"); break;
									case CL_DEVICE_TYPE_GPU:
										printf("GPU"); break;
									default:
										printf("UNKNOWN");
								}
							opencl_get_device_info_string(devices[j], CL_DEVICE_OPENCL_C_VERSION, &str); printf(" (%s)\n", str);
						}
					free(devices);
				}
			}
		free(platforms);
	}
	free(str);
}

cl_program opencl_create_program_from_source(const char *kernel_filename, const char *options)
{
	cl_program program;
	int err;
	char * kernel_src = malloc(11+strlen(kernel_filename)), * opts = malloc(strlen(options)+5);
	strcat(strcat(strcpy(kernel_src, "#include<"), kernel_filename), ">");
	program = clCreateProgramWithSource(context, 1, (const char **)&kernel_src, NULL, &err);
	clCheckError(err, "creating 'include' program ");
	err = clBuildProgram(program, 0, NULL, strcat(strcpy(opts, options), " -I."), NULL, NULL);
	if (err == CL_BUILD_PROGRAM_FAILURE)
	{
		char * build_log;
		uint i;
		for (i=0; i<ndevices; i++)
		{
			size_t log_size;
			clGetProgramBuildInfo(program, devices[i], CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
			if (log_size>2)
			{
				fprintf(stderr, "OpenCL> %s build log [device #%i]:\n", kernel_filename, i);
				build_log = (char *)malloc(log_size);
				clGetProgramBuildInfo(program, devices[i], CL_PROGRAM_BUILD_LOG, log_size, build_log, NULL);
				fputs(build_log, stderr);
				free(build_log);
			}
		}
	}
	clCheckError(err, "building program");
	return program;
}

void opencl_write_program_to_file(const cl_program program, const char * output_filename)
{
	size_t binary_size;
	clCheckError(clGetProgramInfo(program, CL_PROGRAM_BINARY_SIZES, sizeof(size_t), &binary_size, NULL), "getting binary size");
	unsigned char * binary = (unsigned char *)malloc(binary_size);
	clCheckError(clGetProgramInfo(program, CL_PROGRAM_BINARIES, sizeof(char *), &binary, NULL), "getting binary");
	FILE * f = fopen(output_filename, "w");
	if (f==NULL) { fprintf(stderr, "Error opening \"%s\"!\n", output_filename); exit(EXIT_FAILURE); }
	uint i;
	for (i=0; i<binary_size; i++) fputc((int)binary[i], f);
	fclose(f);
}

int main(int argc, char **argv)
{
	size_t i, j;
	char *input_filename = NULL, *output_filename = NULL, **cl_compiler_options = (char **)malloc(argc*sizeof(char*));
	for (i=0; i<argc; i++) cl_compiler_options[i] = NULL;
	int cl_compiler_options_count = 0;
	cl_device_type device_type_id = CL_DEVICE_TYPE_DEFAULT;
	int device_id = -1, platform_id = -1;
	char platform_vendor[7] = "";
	for (i=1; i<argc;)
	{
		if (argv[i][0]=='-')
		{
			if (argv[i][1]=='h')
			{
				printf("usage: %s [-h] [-l] [-p platform_id | platform_vendor] [-d device_id | device_type] [-o filename] [OpenCL_build_options] filename\n", argv[0]);
				puts("Options:");
				puts("    -h");
				puts("        display this help;");
				puts("    -l");
				puts("        list available platforms and devices;");
				puts("    -p platform_id | platform_vendor");
				puts("        select platform;");
				puts("    -d device_id | device_type");
				puts("        select devices;");
				puts("    -o filename");
				puts("        select output filename;");
			}
			else
			if (argv[i][1]=='l')
			{
				list();
			}
			else
			if (argv[i][1]=='o')
			{
				i++;
				output_filename = argv[i];
			}
			else
			if (argv[i][1]=='p')
			{
				if (isdigit(argv[i][2]))
					platform_id = atoi(&argv[i][2]);
				else
				if ((argv[i][2]=='I') || (argv[i][2]=='i'))
				{
					sprintf(platform_vendor, "Intel");
					platform_id = -1;
				}
				else
				if ((argv[i][2]=='A') || (argv[i][2]=='a'))
				{
					sprintf(platform_vendor, "AMD");
					platform_id = -1;
				}
				else
				if ((argv[i][2]=='N') || (argv[i][2]=='n'))
				{
					sprintf(platform_vendor, "NVIDIA");
					platform_id = -1;
				}
				else
				if ((argv[i][2]=='\0') && (argc > i+1))
				{
					i++;
					if (isdigit(argv[i][0]))
						platform_id = atoi(argv[i]);
					else
					if ((argv[i][0]=='I') || (argv[i][0]=='i'))
					{
						sprintf(platform_vendor, "Intel");
						platform_id = -1;
					}
					else
					if ((argv[i][0]=='A') || (argv[i][0]=='a'))
					{
						sprintf(platform_vendor, "AMD");
						platform_id = -1;
					}
					else
					if ((argv[i][0]=='N') || (argv[i][0]=='n'))
					{
						sprintf(platform_vendor, "NVIDIA");
						platform_id = -1;
					}
				}
			}
			else
			if (argv[i][1]=='d')
			{
				if (isdigit(argv[i][2]))
					device_id = atoi(&argv[i][2]);
				else
				if ((argv[i][2]=='G') || (argv[i][2]=='g'))
				{
					device_type_id = CL_DEVICE_TYPE_GPU;
					device_id = -1;
				}
				else
				if ((argv[i][2]=='C') || (argv[i][2]=='c'))
				{
					device_type_id = CL_DEVICE_TYPE_CPU;
					device_id = -1;
				}
				else
				if ((argv[i][2]=='A') || (argv[i][2]=='a'))
				{
					device_type_id = CL_DEVICE_TYPE_ACCELERATOR;
					device_id = -1;
				}
				else
				if ((argv[i][2]=='\0') && (argc > i+1))
				{
					i++;
					if (isdigit(argv[i][0]))
						device_id = atoi(argv[i]);
					else
					if ((argv[i][0]=='G') || (argv[i][2]=='g'))
					{
						device_type_id = CL_DEVICE_TYPE_GPU;
						device_id = -1;
					}
					else
					if ((argv[i][0]=='C') || (argv[i][2]=='c'))
					{
						device_type_id = CL_DEVICE_TYPE_CPU;
						device_id = -1;
					}
					else
					if ((argv[i][0]=='A') || (argv[i][2]=='a'))
					{
						device_type_id = CL_DEVICE_TYPE_ACCELERATOR;
						device_id = -1;
					}
				}
			}
			else
			if (argv[i][1]=='x')
			{
				cl_compiler_options[i] = argv[i];
				i++;
				cl_compiler_options[i] = argv[i];
				cl_compiler_options_count += 2;
			}
			else
			{
				cl_compiler_options[i] = argv[i];
				cl_compiler_options_count++;
			}
		}
		else
			input_filename = argv[i];
		i++;
	}
	//if (input_filename==NULL)
	//{
		//fputs("Specify OpenCL source filename!\n", stderr);
		//exit(EXIT_FAILURE);
	//}
	
	char * build_options = (char *)malloc(sizeof(char)*42*cl_compiler_options_count);
	build_options[0] = '\0';
	for (i=j=0; i<cl_compiler_options_count; i++, j++)
	{
		while (cl_compiler_options[j]==NULL) j++;
		strcat (build_options, cl_compiler_options[j]);
		strcat (build_options, " ");
	}
	free(cl_compiler_options); cl_compiler_options = NULL;
	if (cl_compiler_options_count>0) printf("Build options: %s\n", build_options);
	
	if (input_filename==NULL)
	{
		puts("No target specified.");
		return 0;
	}
	
	char *str = NULL;
	uint n_platforms;
	clCheckError(clGetPlatformIDs(0, NULL, &n_platforms), "getting platforms number");
	cl_platform_id *platforms = (cl_platform_id *)malloc(sizeof(cl_platform_id) * n_platforms);
	clCheckError(clGetPlatformIDs(n_platforms, platforms, NULL), "getting platforms");
	if (platform_id == -1)
		for (i=0; i<n_platforms; i++)
			if (strstr(opencl_get_platform_info_string(platforms[i], CL_PLATFORM_NAME, &str), platform_vendor) != NULL)
				{ platform_id = i; break; }
	free(str);
	
	if (device_id == -1)
	{
		clCheckError(clGetDeviceIDs(platforms[platform_id], device_type_id, 0, NULL, &ndevices), "getting devices number");
		devices = (cl_device_id *)malloc(ndevices * sizeof(cl_device_id));
		clCheckError(clGetDeviceIDs(platforms[platform_id], device_type_id, ndevices, devices, NULL), "getting devices");
	}
	else
	{
		clCheckError(clGetDeviceIDs(platforms[platform_id], CL_DEVICE_TYPE_ALL, 0, NULL, &ndevices), "getting devices number");
		devices = (cl_device_id *)malloc(ndevices * sizeof(cl_device_id));
		clCheckError(clGetDeviceIDs(platforms[platform_id], CL_DEVICE_TYPE_ALL, ndevices, devices, NULL), "getting devices");
		cl_device_id *device = (cl_device_id *)malloc(sizeof(cl_device_id));
		*device = devices[device_id]; free(devices); devices = device;
	}
	
	int err;
	cl_context_properties cps[3] = { CL_CONTEXT_PLATFORM, (cl_context_properties)platforms[platform_id], 0 };
	context = clCreateContext(cps, ndevices, devices, NULL, NULL, &err); clCheckError(err, "creating context");
	
	opencl_write_program_to_file(opencl_create_program_from_source(input_filename, build_options), output_filename==NULL ? "a.out" : output_filename);
	
	clCheckError(clReleaseContext(context), "releasing context");
	for (i=0; i<ndevices; i++)
		clCheckError(clReleaseDevice(devices[i]), "releasing devices");
	free(devices);
	free(platforms);
	
	return 0;
}
