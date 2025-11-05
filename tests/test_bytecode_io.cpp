#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <cstring>

#include "v4front/compile.h"
#include "vendor/doctest/doctest.h"

TEST_CASE("Bytecode file I/O")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Save and load simple bytecode")
  {
    // Compile a simple program
    v4front_err err = v4front_compile("42 DUP +", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);
    REQUIRE(buf.size > 0);

    // Save bytecode to file
    const char* filename = "test_simple.v4b";
    err = v4front_save_bytecode(&buf, filename);
    REQUIRE(err == 0);

    // Load bytecode from file
    V4FrontBuf loaded;
    err = v4front_load_bytecode(filename, &loaded);
    REQUIRE(err == 0);

    // Verify loaded bytecode matches original
    REQUIRE(loaded.size == buf.size);
    CHECK(memcmp(loaded.data, buf.data, buf.size) == 0);

    // Cleanup
    v4front_free(&buf);
    v4front_free(&loaded);
  }

  SUBCASE("Save and load complex program with control flow")
  {
    const char* source = "10 0 DO I LOOP";
    v4front_err err = v4front_compile(source, &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    const char* filename = "test_complex.v4b";
    err = v4front_save_bytecode(&buf, filename);
    REQUIRE(err == 0);

    V4FrontBuf loaded;
    err = v4front_load_bytecode(filename, &loaded);
    REQUIRE(err == 0);

    REQUIRE(loaded.size == buf.size);
    CHECK(memcmp(loaded.data, buf.data, buf.size) == 0);

    v4front_free(&buf);
    v4front_free(&loaded);
  }

  SUBCASE("Save and load with SYS instruction")
  {
    v4front_err err = v4front_compile("13 1 0x01 SYS", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    const char* filename = "test_sys.v4b";
    err = v4front_save_bytecode(&buf, filename);
    REQUIRE(err == 0);

    V4FrontBuf loaded;
    err = v4front_load_bytecode(filename, &loaded);
    REQUIRE(err == 0);

    REQUIRE(loaded.size == buf.size);
    CHECK(memcmp(loaded.data, buf.data, buf.size) == 0);

    // Verify SYS instruction is preserved (SYS is now NoImm)
    bool found_sys = false;
    for (size_t i = 0; i < loaded.size; i++)
    {
      if (loaded.data[i] == 0x60)
      {  // SYS opcode
        found_sys = true;
        break;
      }
    }
    CHECK(found_sys);

    v4front_free(&buf);
    v4front_free(&loaded);
  }

  SUBCASE("Load validates magic number")
  {
    // Create a file with invalid magic number
    const char* filename = "test_invalid.v4b";
    FILE* fp = fopen(filename, "wb");
    REQUIRE(fp != nullptr);

    // Write invalid header
    V4BytecodeHeader header;
    memset(&header, 0, sizeof(header));
    header.magic[0] = 'X';
    header.magic[1] = 'X';
    header.magic[2] = 'X';
    header.magic[3] = 'X';
    header.code_size = 10;

    fwrite(&header, sizeof(header), 1, fp);
    uint8_t dummy[10] = {0};
    fwrite(dummy, 1, 10, fp);
    fclose(fp);

    // Try to load - should fail with invalid magic
    V4FrontBuf loaded;
    v4front_err err = v4front_load_bytecode(filename, &loaded);
    CHECK(err == -4);  // Invalid magic number
  }

  SUBCASE("Error: Load nonexistent file")
  {
    V4FrontBuf loaded;
    v4front_err err = v4front_load_bytecode("nonexistent_xyz.v4b", &loaded);
    CHECK(err == -2);  // Failed to open file
  }

  SUBCASE("Error: Save with NULL buffer")
  {
    v4front_err err = v4front_save_bytecode(nullptr, "test.v4b");
    CHECK(err == -1);  // Invalid parameters
  }

  SUBCASE("Error: Save with NULL filename")
  {
    v4front_err err = v4front_compile("42", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    err = v4front_save_bytecode(&buf, nullptr);
    CHECK(err == -1);  // Invalid parameters

    v4front_free(&buf);
  }

  SUBCASE("Error: Load with NULL out_buf")
  {
    v4front_err err = v4front_load_bytecode("test.v4b", nullptr);
    CHECK(err == -1);  // Invalid parameters
  }

  SUBCASE("Verify header format")
  {
    // Compile and save
    v4front_err err = v4front_compile("100 200 +", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    const char* filename = "test_header.v4b";
    err = v4front_save_bytecode(&buf, filename);
    REQUIRE(err == 0);

    // Read and verify header manually
    FILE* fp = fopen(filename, "rb");
    REQUIRE(fp != nullptr);

    V4BytecodeHeader header;
    size_t read = fread(&header, sizeof(header), 1, fp);
    fclose(fp);

    REQUIRE(read == 1);

    // Check magic number
    CHECK(header.magic[0] == 0x56);  // 'V'
    CHECK(header.magic[1] == 0x34);  // '4'
    CHECK(header.magic[2] == 0x42);  // 'B'
    CHECK(header.magic[3] == 0x43);  // 'C'

    // Check version
    CHECK(header.version_major == 0);
    CHECK(header.version_minor == 1);

    // Check flags and reserved
    CHECK(header.flags == 0);
    CHECK(header.reserved == 0);

    // Check code size
    CHECK(header.code_size == buf.size);

    v4front_free(&buf);
  }

  SUBCASE("Round-trip preserves all bytecode")
  {
    // Test with various opcodes
    const char* programs[] = {
        "1 2 3 4 5",          "10 20 + 30 - 40 * 50 /", "100 DUP SWAP DROP OVER",
        ": FOO 42 ; FOO FOO", "BEGIN 10 UNTIL",         "10 0 DO I LOOP",
        "1 IF 2 ELSE 3 THEN", "SYS 1 SYS 2 SYS 3",
    };

    for (const char* program : programs)
    {
      INFO("Testing program: ", program);

      V4FrontBuf orig;
      v4front_err err = v4front_compile(program, &orig, errmsg, sizeof(errmsg));
      REQUIRE(err == 0);

      const char* filename = "test_roundtrip.v4b";
      err = v4front_save_bytecode(&orig, filename);
      REQUIRE(err == 0);

      V4FrontBuf loaded;
      err = v4front_load_bytecode(filename, &loaded);
      REQUIRE(err == 0);

      REQUIRE(loaded.size == orig.size);
      CHECK(memcmp(loaded.data, orig.data, orig.size) == 0);

      v4front_free(&orig);
      v4front_free(&loaded);
    }
  }
}
