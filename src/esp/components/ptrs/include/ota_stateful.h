
#pragma once

#include <esp_ota_ops.h>
#include "tar_util.h"

// Error code from the untarring process.
enum extract_tar_error {
    // No error.
    ete_ok,

    // The tar file is corrupted.
    ete_corrupt_tar,

    // Error writing plain files to the flash partition.
    ete_bad_esp_flash,

    // Error writing firmware to the update partition.
    ete_firmware_update_failed,
};

// State of the extraction state machine.
enum extract_tar_state {
  // Reading the header of a file in the tar file.
  ets_read_header,

  // Skipping bytes in the tar file.
  ets_skip,

  // Copying a file out of the tar file.
  ets_copy_file,

  // All done with the extraction.
  ets_done,
};

// Context of the extraction.
struct extract_tar_context {
  // Current state.
  extract_tar_state state;

  // Buffer for the tar file header (allocated).
  tar_header *header;

  // Index into the file.
  size_t index;

  // Total bytes to be read.
  size_t file_size;

  // For writing files.
  FILE *f;
  uint8_t *buf;

  // For writing firmware.
  const esp_partition_t *update_partition;
  esp_ota_handle_t update_handle;
};

// Initialize the context structure.
void extract_tar_begin(struct extract_tar_context *ctx);

// Handle a byte from the tar file. Returns 
extract_tar_error extract_tar_handle_byte(struct extract_tar_context *ctx, uint8_t byte);

// Clean up. Call this even in the case of error.
void extract_tar_end(struct extract_tar_context *ctx);

