#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "v4front/compile.h"

// Magic number for .v4b files: "V4BC"
static const uint8_t V4B_MAGIC[4] = {0x56, 0x34, 0x42, 0x43};

// Current file format version
static const uint8_t V4B_VERSION_MAJOR = 0;
static const uint8_t V4B_VERSION_MINOR = 2;

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
  header.word_count = static_cast<uint32_t>(buf->word_count);

  // Write header
  if (fwrite(&header, sizeof(header), 1, fp) != 1)
  {
    fclose(fp);
    return -3;
  }

  // Write main bytecode
  if (fwrite(buf->data, 1, buf->size, fp) != buf->size)
  {
    fclose(fp);
    return -4;
  }

  // Write word definitions
  for (int i = 0; i < buf->word_count; i++)
  {
    const V4FrontWord* word = &buf->words[i];

    // Write name length
    size_t name_len = strlen(word->name);
    if (name_len > 255)
    {
      fclose(fp);
      return -5;  // Name too long
    }
    uint8_t name_len_u8 = static_cast<uint8_t>(name_len);
    if (fwrite(&name_len_u8, 1, 1, fp) != 1)
    {
      fclose(fp);
      return -6;
    }

    // Write name (without null terminator)
    if (fwrite(word->name, 1, name_len, fp) != name_len)
    {
      fclose(fp);
      return -7;
    }

    // Write code length
    if (fwrite(&word->code_len, sizeof(uint32_t), 1, fp) != 1)
    {
      fclose(fp);
      return -8;
    }

    // Write code
    if (word->code_len > 0)
    {
      if (fwrite(word->code, 1, word->code_len, fp) != word->code_len)
      {
        fclose(fp);
        return -9;
      }
    }
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

  // Allocate main bytecode buffer
  uint8_t* data = static_cast<uint8_t*>(malloc(header.code_size));
  if (!data)
  {
    fclose(fp);
    return -5;
  }

  // Read main bytecode
  if (fread(data, 1, header.code_size, fp) != header.code_size)
  {
    free(data);
    fclose(fp);
    return -6;
  }

  // Initialize output buffer
  out_buf->data = data;
  out_buf->size = header.code_size;
  out_buf->words = nullptr;
  out_buf->word_count = 0;

  // Read word definitions (v0.2+)
  if (header.version_minor >= 2 && header.word_count > 0)
  {
    // Allocate word array
    V4FrontWord* words =
        static_cast<V4FrontWord*>(malloc(sizeof(V4FrontWord) * header.word_count));
    if (!words)
    {
      free(data);
      fclose(fp);
      return -7;
    }

    // Initialize all word pointers to null
    for (uint32_t i = 0; i < header.word_count; i++)
    {
      words[i].name = nullptr;
      words[i].code = nullptr;
      words[i].code_len = 0;
    }

    // Read each word definition
    for (uint32_t i = 0; i < header.word_count; i++)
    {
      // Read name length
      uint8_t name_len;
      if (fread(&name_len, 1, 1, fp) != 1)
      {
        // Cleanup
        for (uint32_t j = 0; j < i; j++)
        {
          free(words[j].name);
          free(words[j].code);
        }
        free(words);
        free(data);
        fclose(fp);
        return -8;
      }

      // Allocate and read name
      char* name = static_cast<char*>(malloc(name_len + 1));
      if (!name)
      {
        // Cleanup
        for (uint32_t j = 0; j < i; j++)
        {
          free(words[j].name);
          free(words[j].code);
        }
        free(words);
        free(data);
        fclose(fp);
        return -9;
      }

      if (fread(name, 1, name_len, fp) != name_len)
      {
        free(name);
        // Cleanup
        for (uint32_t j = 0; j < i; j++)
        {
          free(words[j].name);
          free(words[j].code);
        }
        free(words);
        free(data);
        fclose(fp);
        return -10;
      }
      name[name_len] = '\0';
      words[i].name = name;

      // Read code length
      uint32_t code_len;
      if (fread(&code_len, sizeof(uint32_t), 1, fp) != 1)
      {
        // Cleanup
        for (uint32_t j = 0; j <= i; j++)
        {
          free(words[j].name);
          free(words[j].code);
        }
        free(words);
        free(data);
        fclose(fp);
        return -11;
      }
      words[i].code_len = code_len;

      // Allocate and read code
      if (code_len > 0)
      {
        uint8_t* code = static_cast<uint8_t*>(malloc(code_len));
        if (!code)
        {
          // Cleanup
          for (uint32_t j = 0; j <= i; j++)
          {
            free(words[j].name);
            free(words[j].code);
          }
          free(words);
          free(data);
          fclose(fp);
          return -12;
        }

        if (fread(code, 1, code_len, fp) != code_len)
        {
          free(code);
          // Cleanup
          for (uint32_t j = 0; j <= i; j++)
          {
            free(words[j].name);
            free(words[j].code);
          }
          free(words);
          free(data);
          fclose(fp);
          return -13;
        }
        words[i].code = code;
      }
      else
      {
        words[i].code = nullptr;
      }
    }

    out_buf->words = words;
    out_buf->word_count = header.word_count;
  }

  fclose(fp);
  return 0;
}
