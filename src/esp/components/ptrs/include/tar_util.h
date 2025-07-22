
#pragma once

#include <stdint.h>
#include <cstddef>

// String in "magic" field.
#define TAR_MAGIC "ustar"

// TAR header structure.
union tar_header {
  struct _header {
    char filename[100];  // Filename (null-terminated string)
    char mode[8];        // File mode (not used in this code)
    char uid[8];         // Owner's numeric user ID (not used)
    char gid[8];         // Group's numeric user ID (not used)
    char size[12];       // File size in octal (as a string)
    char mtime[12];      // Modification time (not used)
    char chksum[8];      // Checksum (not used)
    char typeflag;       // Type of file (file, directory, etc.)
    char linkname[100];  // Link name (not used)
    char magic[6];       // UStar indicator, see TAR_MAGIC

    // ... (other fields in the TAR header, not relevant here)
  } header;
  char buffer[512];
};

// Compute the padding used in a tar file for a file of the given size.
size_t compute_padding_for_tar_file_size(size_t file_size);

// Helper function to convert octal string to integer.
// Stops at the first non-octal digit.
size_t octal_to_decimal(const char *octal);

