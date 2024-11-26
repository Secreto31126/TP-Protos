#ifndef LOGREAD_H
#define LOGREAD_H
#include "statistics.h"

/**
 * @brief A function that receives the data field from a log and returns either NULL or a memory allocated string that represents this data
 *
 */
typedef char *(*data_parser)(void *);

/**
 * @brief Get a memory allocated string representing a log
 *
 * @note set parser to NULL to ignore the data field
 * @param l
 * @param parser
 * @return char*
 */
char *parse_log(pop_log l, data_parser parser);

#endif