/*
 * @Author: JesseMK
 * @Date: 2017-10-19 16:35:59
 * @Last Modified by: JesseMK
 * @Last Modified time: 2017-10-19 20:53:06
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdbool.h>
#include <net/if.h>
#include <time.h>

// #include <rte_eal.h>
// #include <rte_common.h>
// #include <rte_ethdev.h>
#include <rte_errno.h>
#include <rte_string_fns.h>

static int parse_matrix_opt(char *arg, unsigned long *matrix,
                            unsigned long max_len)
{
    char *comma_tokens[100];
    int nb_comma_tokens;
    char *dot_tokens[3];
    int nb_dot_tokens;
    char *dash_tokens[3];
    int nb_dash_tokens;

    char *end;

    unsigned long left_key;
    unsigned long right_key;
    unsigned long value;

    nb_comma_tokens = rte_strsplit(arg, strlen(arg),
                                   comma_tokens, 100, ',');
    // Case with a single value
    if (nb_comma_tokens == 1 && strchr(arg, '.') == NULL)
    {
        errno = 0;
        value = strtoul(arg, &end, 10);
        if (errno || *end != '\0')
            return -EINVAL;
        unsigned long key;
        for (key = 0; key < max_len; key++)
        {
            matrix[key] = value;
        }
        return 0;
    }

    // Key-value matrix
    if (nb_comma_tokens > 0)
    {
        int comma;
        for (comma = 0; comma < nb_comma_tokens; comma++)
        {
            // Split between left and right side of the dot
            nb_dot_tokens = rte_strsplit(comma_tokens[comma],
                                         strlen(comma_tokens[comma]),
                                         dot_tokens, 3, '.');
            if (nb_dot_tokens != 2)
                return -EINVAL;

            // Handle value
            errno = 0;
            value = strtoul(dot_tokens[1], &end, 10);
            if (errno || *end != '\0')
                return -EINVAL;

            // Handle key
            nb_dash_tokens = rte_strsplit(dot_tokens[0],
                                          strlen(dot_tokens[0]),
                                          dash_tokens, 3, '-');
            if (nb_dash_tokens == 1)
            {
                // Single value
                left_key = strtoul(dash_tokens[0], &end, 10);
                if (errno || *end != '\0')
                    return -EINVAL;
                right_key = left_key;
            }
            else if (nb_dash_tokens == 2)
            {
                // Interval value
                left_key = strtoul(dash_tokens[0], &end, 10);
                if (errno || *end != '\0')
                    return -EINVAL;
                right_key = strtoul(dash_tokens[1], &end, 10);
                if (errno || *end != '\0')
                    return -EINVAL;
            }
            else
            {
                return -EINVAL;
            }

            // Fill-in the matrix
            if (right_key < max_len && right_key >= left_key)
            {
                unsigned long key;
                for (key = left_key; key <= right_key; key++)
                {
                    matrix[key] = value;
                }
            }
            else
            {
                return -EINVAL;
            }
        }
    }
    else
    {
        return -EINVAL;
    }
    return 0;
}
