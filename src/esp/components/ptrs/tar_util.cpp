
#include "tar_util.h"

size_t compute_padding_for_tar_file_size(size_t file_size)
{
  return (512 - (file_size % 512)) % 512;
}

size_t octal_to_decimal(const char *octal)
{
  size_t result = 0;

  while (*octal >= '0' && *octal <= '7') {
    result = (result << 3) + (*octal - '0');
    octal++;
  }

  return result;
}
