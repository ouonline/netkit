#ifndef __STD_LOGGER_H__
#define __STD_LOGGER_H__

#include <stdio.h>

#define log_debug(fmt, ...)     printf("%s:%u: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define log_misc(fmt, ...)      printf("%s:%u: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define log_info(fmt, ...)      printf("%s:%u: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define log_warning(fmt, ...)   fprintf(stderr, "%s:%u: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define log_error(fmt, ...)     fprintf(stderr, "%s:%u: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define log_fatal(fmt, ...)     fprintf(stderr, "%s:%u: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)

#endif
