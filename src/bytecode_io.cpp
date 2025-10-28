#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "v4front/compile.h"

// Magic number for .v4b files: "V4BC"
static const uint8_t V4B_MAGIC[4] = {0x56, 0x34, 0x42, 0x43};

// Current file format version
static const uint8_t V4B_VERSION_MAJOR = 0;
static const uint8_t V4B_VERSION_MINOR = 1;

extern "C" v4front_err v4front_save_bytecode(const V4FrontBuf* buf, const char* filename)
{
  if (!buf || !filename)
  {
    return -1;
  }

  if (!buf->data || buf->size == 0)
  {
    return -1;
  }

  FILE* fp = fopen(filename, "wb");
  if (!fp)
  {
    return -2;
  }

  // Build header
  V4BytecodeHeader header;
  memcpy(header.magic, V4B_MAGIC, 4);
  header.version_major = V4B_VERSION_MAJOR;
  header.version_minor = V4B_VERSION_MINOR;
  header.flags = 0;
  header.code_size = static_cast<uint32_t>(buf->size);
  header.reserved = 0;

  // Write header
  if (fwrite(&header, sizeof(header), 1, fp) != 1)
  {
    fclose(fp);
    return -3;
  }

  // Write bytecode
  if (fwrite(buf->data, 1, buf->size, fp) != buf->size)
  {
    fclose(fp);
    return -4;
  }

  fclose(fp);
  return 0;
}

extern "C" v4front_err v4front_load_bytecode(const char* filename, V4FrontBuf* out_buf)
{
  if (!filename || !out_buf)
  {
    return -1;
  }

  FILE* fp = fopen(filename, "rb");
  if (!fp)
  {
    return -2;
  }

  // Read header
  V4BytecodeHeader header;
  if (fread(&header, sizeof(header), 1, fp) != 1)
  {
    fclose(fp);
    return -3;
  }

  // Validate magic number
  if (memcmp(header.magic, V4B_MAGIC, 4) != 0)
  {
    fclose(fp);
    return -4;
  }

  // Allocate bytecode buffer
  uint8_t* data = static_cast<uint8_t*>(malloc(header.code_size));
  if (!data)
  {
    fclose(fp);
    return -5;
  }

  // Read bytecode
  if (fread(data, 1, header.code_size, fp) != header.code_size)
  {
    free(data);
    fclose(fp);
    return -6;
  }

  fclose(fp);

  // Populate output buffer
  out_buf->data = data;
  out_buf->size = header.code_size;
  out_buf->words = nullptr;
  out_buf->word_count = 0;

  return 0;
}
