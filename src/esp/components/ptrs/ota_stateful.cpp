
#include "mongoose.h"
#include "esp_log.h"
#include "ota_stateful.h"
#include "tar_util.h"

#define TAG "OTAS"

#define BUFFER_SIZE 4096

void extract_tar_begin(struct extract_tar_context *ctx) {
  ctx->state = ets_read_header;
  ctx->header = static_cast<tar_header *>(calloc(sizeof(*ctx->header), 1));
  ctx->index = 0;
  ctx->file_size = 0;
  ctx->f = NULL;
  ctx->buf = NULL;
  ctx->update_partition = NULL;
  ctx->update_handle = 0;
}

// Discard this many bytes, then read the next header.
static void extract_tar_skip(struct extract_tar_context *ctx, size_t count) {
  if (count > 0) {
    ctx->state = ets_skip;
    ctx->file_size = count;
  } else {
    ctx->state = ets_read_header;
  }
  ctx->index = 0;
}

// Clean up the file-copying process and advance the state.
// Returns whether successful.
static bool extract_tar_copy_file_end(struct extract_tar_context *ctx) {
  MG_INFO(("OTA: End of file"));
  free(ctx->buf);
  ctx->buf = NULL;

  if (ctx->f == NULL) {
    esp_err_t esp_err = esp_ota_end(ctx->update_handle);
    ctx->update_handle = 0;
    if (esp_err != ESP_OK) {
      return false;
    }

    esp_err = esp_ota_set_boot_partition(ctx->update_partition);
    ctx->update_partition = NULL;
    if (esp_err != ESP_OK) {
      return false;
    }
    MG_INFO(("OTA: Switched boot partition"));
  } else {
    fclose(ctx->f);
    ctx->f = NULL;
  }

  // Skip padding bytes (to align to 512-byte blocks).
  extract_tar_skip(ctx, compute_padding_for_tar_file_size(ctx->file_size));

  return true;
}

// Configures writing the tar file to the specified filename. If the filename is NULL,
// writes to the firmware update partition. Returns whether it was successful in setting
// up the file for writing.
static bool extract_tar_copy_file_begin(struct extract_tar_context *ctx, char *filename, size_t file_size) {
  MG_INFO(("OTA: Starting file %s", filename == NULL ? "firmware" : filename));

  // Do this even if the size is zero, we need to blank out the file.
  ctx->state = ets_copy_file;
  ctx->index = 0;
  ctx->file_size = file_size;

  if (filename == NULL) {
    ESP_LOGI(TAG, "Performing OTA");
    ctx->update_partition = esp_ota_get_next_update_partition(NULL);
    if (ctx->update_partition == NULL) {
      ESP_LOGE(TAG, "Found no update partition");
      return false;
    }
    ESP_LOGI(TAG, "Writing to partition %s subtype %d at offset 0x%x",
             ctx->update_partition->label,
             ctx->update_partition->subtype,
             ctx->update_partition->address);

    esp_err_t esp_err = esp_ota_begin(ctx->update_partition,
        OTA_SIZE_UNKNOWN, &ctx->update_handle);
    if (esp_err != ESP_OK) {
      return false;
    }
  } else {
    char* abs_path;
    asprintf(&abs_path, "/%s", filename);
    ctx->f = fopen(abs_path, "wb");
    free(abs_path);
    if (ctx->f == NULL) {
      return false;
    }
  }

  ctx->buf = (uint8_t *) malloc(BUFFER_SIZE);

  if (file_size == 0) {
    return extract_tar_copy_file_end(ctx);
  }

  return true;
}

// Write a byte to the file. Returns whether successful.
static bool extract_tar_copy_file_byte(struct extract_tar_context *ctx, uint8_t byte) {
  ctx->buf[ctx->index++ % BUFFER_SIZE] = byte;

  // Write full block.
  if (ctx->index % BUFFER_SIZE == 0) {
    // Too noisy:
    // MG_INFO(("OTA: Writing block"));
    if (ctx->f == NULL) {
      esp_err_t esp_err = esp_ota_write(ctx->update_handle, (const void *) ctx->buf, BUFFER_SIZE);
      if (esp_err != ESP_OK) {
        return false;
      }
    } else {
      fwrite(ctx->buf, 1, BUFFER_SIZE, ctx->f);
    }
  }

  if (ctx->index == ctx->file_size) {
    size_t partial_block = ctx->index % BUFFER_SIZE;
    if (partial_block != 0) {
      // Write partial last block.
      if (ctx->f == NULL) {
        esp_err_t esp_err = esp_ota_write(ctx->update_handle, (const void *) ctx->buf, partial_block);
        if (esp_err != ESP_OK) {
          return false;
        }
      } else {
        fwrite(ctx->buf, 1, partial_block, ctx->f);
      }
    }

    return extract_tar_copy_file_end(ctx);
  }

  return true;
}

extract_tar_error extract_tar_handle_byte(struct extract_tar_context *ctx, uint8_t byte) {
  switch (ctx->state) {
    case ets_read_header:
      ctx->header->buffer[ctx->index++] = byte;
      if (ctx->index == sizeof(*ctx->header)) {
        // If the filename is empty, we've reached the end of the tar file.
        if (ctx->header->header.filename[0] == '\0') {
          ctx->state = ets_done;
          return ete_ok;
        }
        MG_INFO(("OTA: Filename: %s", ctx->header->header.filename));

        // Abort if the magic value does not match.
        if (strncmp(ctx->header->header.magic, TAR_MAGIC, 5) != 0) {
          MG_INFO(("OTA: Bad tar magic \"%s\"", ctx->header->header.magic));
          ctx->state = ets_done;
          return ete_corrupt_tar;
        }

        // Get the file size from the header (stored as an octal string).
        size_t file_size = octal_to_decimal(ctx->header->header.size);
        MG_INFO(("OTA: File size: %u", file_size));

        // Skip non-regular files (typeflag '0' or '\0' is for regular files)
        if (ctx->header->header.typeflag != '0' && ctx->header->header.typeflag != '\0') {
          MG_INFO(("OTA: Skipping non-regular file"));
          extract_tar_skip(ctx, file_size + compute_padding_for_tar_file_size(file_size));
          return ete_ok;
        }

        // Files in these directories are written straight to the file system.
        if (strncmp("html/", ctx->header->header.filename, 5) == 0 ||
            strncmp("fpga/", ctx->header->header.filename, 5) == 0) {

          bool success = extract_tar_copy_file_begin(ctx, ctx->header->header.filename, file_size);
          if (!success) {
            return ete_bad_esp_flash;
          }
        } else if (strcmp("build/trs-io.bin", ctx->header->header.filename) == 0) {
          bool success = extract_tar_copy_file_begin(ctx, NULL, file_size);
          if (!success) {
            return ete_firmware_update_failed;
          }
        } else {
          // Read and discard the file contents.
          MG_INFO(("OTA: Skipping unknown file"));
          extract_tar_skip(ctx, file_size + compute_padding_for_tar_file_size(file_size));
        }
      }
      break;

    case ets_skip:
      ctx->index += 1;
      if (ctx->index == ctx->file_size) {
        ctx->state = ets_read_header;
        ctx->index = 0;
      }
      break;

    case ets_copy_file: {
      bool success = extract_tar_copy_file_byte(ctx, byte);
      if (!success) {
        return ctx->f == NULL ? ete_firmware_update_failed : ete_bad_esp_flash;
      }
      break;
    }

    case ets_done:
      // Nothing to do, throw away the byte.
      break;
  }

  return ete_ok;
}

void extract_tar_end(struct extract_tar_context *ctx) {
  free(ctx->header);
  ctx->header = NULL;
}

