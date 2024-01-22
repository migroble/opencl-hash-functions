/*
 * Copyright (c) 2024 Miguel "Peppermint" Robledo
 *
 * SPDX-License-Identifier: 0BSD
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CL_TARGET_OPENCL_VERSION 300

#include <CL/cl.h>

void usage(char **argv) {
  fprintf(stderr,
          "Usage: %s [options] <hash_function>\n"
          "\n"
          "Options:\n"
          "  -h            Show this message\n"
          "  -d <cpu/gpu>  Use CPU or GPU\n"
          "  -p <n>        Use n-th platform\n",
          argv[0]);
}

typedef long unsigned int u64;
typedef unsigned int u32;
typedef unsigned char u8;

typedef struct {
  char *name;
  char *program;
  char *kernel;
  size_t hash_size;
  void (*print)(void *);
} hash_info_t;

typedef struct {
  u32 platform;
  cl_device_type device_type;
  hash_info_t *hash_function;
} args_t;

typedef struct {
  u32 offset;
  u32 length;
} metadata_t;

int parse_args(args_t *args, int argc, char **argv);

cl_int get_platform(u32 index, cl_platform_id *id);
cl_int get_device(cl_platform_id platform, cl_device_type device_type,
                  cl_device_id *id);
cl_int build_program(cl_context ctx, const char *filename, cl_program *program);

const char *clGetErrorString(cl_int error);

static hash_info_t HASH_FUNCTIONS[] = {NULL, NULL, NULL, 0, NULL};

cl_int hash(cl_context ctx, cl_kernel kernel, cl_command_queue queue,
            hash_info_t *h, u32 items, metadata_t *metadata, u8 *data,
            u32 data_len, void **hashes) {
  cl_int ret;
  size_t global_size = items;
  size_t metadata_len = items * sizeof(metadata_t);
  size_t hashes_len = items * h->hash_size;
  cl_mem metadata_buf;
  cl_mem data_buf;
  cl_mem hashes_buf;

  *hashes = malloc(hashes_len);

  metadata_buf = clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                metadata_len, metadata, &ret);
  data_buf = clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                            data_len, data, &ret);
  hashes_buf = clCreateBuffer(ctx, CL_MEM_WRITE_ONLY | CL_MEM_COPY_HOST_PTR,
                              hashes_len, *hashes, &ret);

  clSetKernelArg(kernel, 0, sizeof(cl_mem), &metadata_buf);
  clSetKernelArg(kernel, 1, sizeof(cl_mem), &data_buf);
  clSetKernelArg(kernel, 2, sizeof(cl_mem), &hashes_buf);

  clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_size, NULL, 0, NULL,
                         NULL);
  clEnqueueReadBuffer(queue, hashes_buf, CL_TRUE, 0, hashes_len, *hashes, 0,
                      NULL, NULL);

  return ret;
}

int main(int argc, char **argv) {
  args_t args;
  cl_int ret = CL_SUCCESS;
  cl_platform_id platform;
  cl_device_id device;
  cl_context ctx;
  cl_program program;
  cl_command_queue queue;
  cl_kernel kernel;
  hash_info_t *h;
  char *line;
  size_t len = 0;
  ssize_t line_size = 0;
  u32 items = 0;
  metadata_t *metadata = NULL;
  u8 *data = NULL;
  size_t data_len = 0;
  void *hashes;
  int i;

  if (parse_args(&args, argc, argv) != 0) {
    goto err;
  }

  h = args.hash_function;

  if ((ret = get_platform(args.platform, &platform)) != 0) {
    fprintf(stderr, "Failed to get platform IDs: %s\n", clGetErrorString(ret));
    goto err;
  }

  if ((ret = get_device(platform, args.device_type, &device)) != 0) {
    fprintf(stderr, "Failed to get device ID: %s\n", clGetErrorString(ret));
    goto err;
  }

  ctx = clCreateContext(NULL, 1, &device, NULL, NULL, &ret);
  if (ret != CL_SUCCESS) {
    printf("Failed to create OpenCL context: %s\n", clGetErrorString(ret));
    goto err;
  }

  if ((ret = build_program(ctx, args.hash_function->program, &program)) != 0) {
    fprintf(stderr, "Failed to build program: %s\n", clGetErrorString(ret));
    goto err;
  }

  queue = clCreateCommandQueueWithProperties(ctx, device, NULL, NULL);
  kernel = clCreateKernel(program, args.hash_function->kernel, NULL);

  while ((line_size = getline(&line, &len, stdin)) != -1) {
    if (line[line_size - 1] == '\n') {
      line_size -= 1;
    }

    data = realloc(data, data_len + line_size);
    metadata = realloc(metadata, (items + 1) * sizeof(metadata_t));

    (metadata + items)->offset = data_len;
    (metadata + items)->length = line_size;

    memcpy(data + data_len, line, line_size);
    data_len += line_size;

    ++items;
  }
  free(line);

  hash(ctx, kernel, queue, h, items, metadata, data, data_len, &hashes);

  for (i = 0; i < items; ++i) {
    h->print(((char *)hashes) + i * h->hash_size);
  }

  free(hashes);

  clReleaseKernel(kernel);
  clReleaseCommandQueue(queue);
  clReleaseProgram(program);
  clReleaseContext(ctx);

err:
  return ret;
}

int parse_args(args_t *args, int argc, char **argv) {
  int c;
  int i;
  char *hash_function;
  int hash_function_count = sizeof(HASH_FUNCTIONS) / sizeof(hash_info_t);

  args->platform = 0;
  args->device_type = CL_DEVICE_TYPE_ALL;

  opterr = 0;

  while ((c = getopt(argc, argv, "d:lhp:")) != -1) {
    switch (c) {
    case 'h':
      usage(argv);
      return 1;
    case 'd':
      if (strcmp(optarg, "cpu") == 0) {
        args->device_type = CL_DEVICE_TYPE_CPU;
      } else if (strcmp(optarg, "gpu") == 0) {
        args->device_type = CL_DEVICE_TYPE_GPU;
      }
      break;
    case 'p':
      args->platform = atoi(optarg);
      break;
    case '?':
      if (optopt == 'd' || optopt == 'p') {
        fprintf(stderr, "Option -%c requires an argument.\n", optopt);
      } else {
        fprintf(stderr, "Unknown option `-%c'.\n", optopt);
      }
      return 1;
    default:
      abort();
    }
  }

  if (argc - optind < 1) {
    fprintf(stderr,
            "Missing required positional argument \"hash_function\"\n\n");
    usage(argv);
    return 1;
  }

  hash_function = argv[optind];

  for (i = 0; i < hash_function_count; ++i) {
    if (strcmp((HASH_FUNCTIONS + i)->name, hash_function) == 0) {
      args->hash_function = HASH_FUNCTIONS + i;
      return 0;
    }
  }

  fprintf(stderr,
          "Invalid hash function \"%s\"\n"
          "\n"
          "Hash function must be one of:\n",
          hash_function);
  for (i = 0; i < hash_function_count; ++i) {
    fprintf(stderr, " - %s\n", (HASH_FUNCTIONS + i)->name);
  }

  return 1;
}

cl_int get_platform(u32 index, cl_platform_id *id) {
  cl_uint n_platforms = 0;
  cl_platform_id *platforms;
  cl_int ret;

  ret = clGetPlatformIDs(0, NULL, &n_platforms);
  if (ret != CL_SUCCESS) {
    return ret;
  } else if (index >= n_platforms) {
    return CL_INVALID_PLATFORM;
  }

  platforms = malloc((index + 1) * sizeof(cl_platform_id));
  clGetPlatformIDs(index + 1, platforms, NULL);
  *id = platforms[index];
  free(platforms);

  return ret;
}

cl_int get_device(cl_platform_id platform, cl_device_type device_type,
                  cl_device_id *id) {
  cl_uint n_devices;
  cl_int ret;

  ret = clGetDeviceIDs(platform, device_type, 0, NULL, &n_devices);
  if (ret != CL_SUCCESS) {
    return ret;
  }

  return clGetDeviceIDs(platform, device_type, 1, id, NULL);
}

cl_int build_program(cl_context ctx, const char *filename,
                     cl_program *program) {
  FILE *fd;
  char *buf;
  size_t len;

  fd = fopen(filename, "r");
  if (fd == NULL) {
    return CL_INVALID_VALUE;
  }

  fseek(fd, 0, SEEK_END);
  len = ftell(fd);
  rewind(fd);

  buf = (char *)malloc(len + 1);
  fread(buf, sizeof(char), len, fd);
  fclose(fd);
  buf[len] = '\0';

  *program = clCreateProgramWithSource(ctx, 1, (const char **)&buf, &len, NULL);
  free(buf);

  return clBuildProgram(*program, 0, NULL, NULL, NULL, NULL);
}

const char *clGetErrorString(cl_int error) {
  switch (error) {
  // run-time and JIT compiler errors
  case 0:
    return "CL_SUCCESS";
  case -1:
    return "CL_DEVICE_NOT_FOUND";
  case -2:
    return "CL_DEVICE_NOT_AVAILABLE";
  case -3:
    return "CL_COMPILER_NOT_AVAILABLE";
  case -4:
    return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
  case -5:
    return "CL_OUT_OF_RESOURCES";
  case -6:
    return "CL_OUT_OF_HOST_MEMORY";
  case -7:
    return "CL_PROFILING_INFO_NOT_AVAILABLE";
  case -8:
    return "CL_MEM_COPY_OVERLAP";
  case -9:
    return "CL_IMAGE_FORMAT_MISMATCH";
  case -10:
    return "CL_IMAGE_FORMAT_NOT_SUPPORTED";
  case -11:
    return "CL_BUILD_PROGRAM_FAILURE";
  case -12:
    return "CL_MAP_FAILURE";
  case -13:
    return "CL_MISALIGNED_SUB_BUFFER_OFFSET";
  case -14:
    return "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST";
  case -15:
    return "CL_COMPILE_PROGRAM_FAILURE";
  case -16:
    return "CL_LINKER_NOT_AVAILABLE";
  case -17:
    return "CL_LINK_PROGRAM_FAILURE";
  case -18:
    return "CL_DEVICE_PARTITION_FAILED";
  case -19:
    return "CL_KERNEL_ARG_INFO_NOT_AVAILABLE";

  // compile-time errors
  case -30:
    return "CL_INVALID_VALUE";
  case -31:
    return "CL_INVALID_DEVICE_TYPE";
  case -32:
    return "CL_INVALID_PLATFORM";
  case -33:
    return "CL_INVALID_DEVICE";
  case -34:
    return "CL_INVALID_CONTEXT";
  case -35:
    return "CL_INVALID_QUEUE_PROPERTIES";
  case -36:
    return "CL_INVALID_COMMAND_QUEUE";
  case -37:
    return "CL_INVALID_HOST_PTR";
  case -38:
    return "CL_INVALID_MEM_OBJECT";
  case -39:
    return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
  case -40:
    return "CL_INVALID_IMAGE_SIZE";
  case -41:
    return "CL_INVALID_SAMPLER";
  case -42:
    return "CL_INVALID_BINARY";
  case -43:
    return "CL_INVALID_BUILD_OPTIONS";
  case -44:
    return "CL_INVALID_PROGRAM";
  case -45:
    return "CL_INVALID_PROGRAM_EXECUTABLE";
  case -46:
    return "CL_INVALID_KERNEL_NAME";
  case -47:
    return "CL_INVALID_KERNEL_DEFINITION";
  case -48:
    return "CL_INVALID_KERNEL";
  case -49:
    return "CL_INVALID_ARG_INDEX";
  case -50:
    return "CL_INVALID_ARG_VALUE";
  case -51:
    return "CL_INVALID_ARG_SIZE";
  case -52:
    return "CL_INVALID_KERNEL_ARGS";
  case -53:
    return "CL_INVALID_WORK_DIMENSION";
  case -54:
    return "CL_INVALID_WORK_GROUP_SIZE";
  case -55:
    return "CL_INVALID_WORK_ITEM_SIZE";
  case -56:
    return "CL_INVALID_GLOBAL_OFFSET";
  case -57:
    return "CL_INVALID_EVENT_WAIT_LIST";
  case -58:
    return "CL_INVALID_EVENT";
  case -59:
    return "CL_INVALID_OPERATION";
  case -60:
    return "CL_INVALID_GL_OBJECT";
  case -61:
    return "CL_INVALID_BUFFER_SIZE";
  case -62:
    return "CL_INVALID_MIP_LEVEL";
  case -63:
    return "CL_INVALID_GLOBAL_WORK_SIZE";
  case -64:
    return "CL_INVALID_PROPERTY";
  case -65:
    return "CL_INVALID_IMAGE_DESCRIPTOR";
  case -66:
    return "CL_INVALID_COMPILER_OPTIONS";
  case -67:
    return "CL_INVALID_LINKER_OPTIONS";
  case -68:
    return "CL_INVALID_DEVICE_PARTITION_COUNT";

  // extension errors
  case -1000:
    return "CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR";
  case -1001:
    return "CL_PLATFORM_NOT_FOUND_KHR";
  case -1002:
    return "CL_INVALID_D3D10_DEVICE_KHR";
  case -1003:
    return "CL_INVALID_D3D10_RESOURCE_KHR";
  case -1004:
    return "CL_D3D10_RESOURCE_ALREADY_ACQUIRED_KHR";
  case -1005:
    return "CL_D3D10_RESOURCE_NOT_ACQUIRED_KHR";
  default:
    return "Unknown";
  }
}
