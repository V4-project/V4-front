#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <cstring>

#include "v4/opcodes.hpp"
#include "v4front/compile.h"
#include "v4front/disasm.hpp"
#include "v4front/errors.hpp"
#include "vendor/doctest/doctest.h"

using namespace v4front;
using Op = v4::Op;

TEST_CASE("Task management opcodes compile correctly")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("SPAWN compiles to TASK_SPAWN")
  {
    v4front_err err = v4front_compile("SPAWN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[0] == static_cast<uint8_t>(Op::TASK_SPAWN));
    v4front_free(&buf);
  }

  SUBCASE("TASK-EXIT compiles to TASK_EXIT")
  {
    v4front_err err = v4front_compile("TASK-EXIT", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[0] == static_cast<uint8_t>(Op::TASK_EXIT));
    v4front_free(&buf);
  }

  SUBCASE("SLEEP compiles to TASK_SLEEP")
  {
    v4front_err err = v4front_compile("SLEEP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[0] == static_cast<uint8_t>(Op::TASK_SLEEP));
    v4front_free(&buf);
  }

  SUBCASE("MS compiles to TASK_SLEEP (alias)")
  {
    v4front_err err = v4front_compile("MS", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[0] == static_cast<uint8_t>(Op::TASK_SLEEP));
    v4front_free(&buf);
  }

  SUBCASE("YIELD compiles to TASK_YIELD")
  {
    v4front_err err = v4front_compile("YIELD", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[0] == static_cast<uint8_t>(Op::TASK_YIELD));
    v4front_free(&buf);
  }

  SUBCASE("PAUSE compiles to TASK_YIELD (alias)")
  {
    v4front_err err = v4front_compile("PAUSE", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[0] == static_cast<uint8_t>(Op::TASK_YIELD));
    v4front_free(&buf);
  }

  SUBCASE("CRITICAL compiles to CRITICAL_ENTER")
  {
    v4front_err err = v4front_compile("CRITICAL", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[0] == static_cast<uint8_t>(Op::CRITICAL_ENTER));
    v4front_free(&buf);
  }

  SUBCASE("UNCRITICAL compiles to CRITICAL_EXIT")
  {
    v4front_err err = v4front_compile("UNCRITICAL", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[0] == static_cast<uint8_t>(Op::CRITICAL_EXIT));
    v4front_free(&buf);
  }

  SUBCASE("SEND compiles to TASK_SEND")
  {
    v4front_err err = v4front_compile("SEND", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[0] == static_cast<uint8_t>(Op::TASK_SEND));
    v4front_free(&buf);
  }

  SUBCASE("RECEIVE compiles to TASK_RECEIVE")
  {
    v4front_err err = v4front_compile("RECEIVE", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[0] == static_cast<uint8_t>(Op::TASK_RECEIVE));
    v4front_free(&buf);
  }

  SUBCASE("RECEIVE-BLOCKING compiles to TASK_RECEIVE_BLOCKING")
  {
    v4front_err err = v4front_compile("RECEIVE-BLOCKING", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[0] == static_cast<uint8_t>(Op::TASK_RECEIVE_BLOCKING));
    v4front_free(&buf);
  }

  SUBCASE("ME compiles to TASK_SELF")
  {
    v4front_err err = v4front_compile("ME", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[0] == static_cast<uint8_t>(Op::TASK_SELF));
    v4front_free(&buf);
  }

  SUBCASE("TASKS compiles to TASK_COUNT")
  {
    v4front_err err = v4front_compile("TASKS", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[0] == static_cast<uint8_t>(Op::TASK_COUNT));
    v4front_free(&buf);
  }
}

TEST_CASE("Task management opcodes disassemble correctly")
{
  SUBCASE("TASK_SPAWN disassembles")
  {
    uint8_t code[] = {static_cast<uint8_t>(Op::TASK_SPAWN)};
    std::string line;
    size_t consumed = disasm_one(code, sizeof(code), 0, line);
    CHECK(consumed == 1);
    CHECK(line.find("TASK_SPAWN") != std::string::npos);
  }

  SUBCASE("TASK_EXIT disassembles")
  {
    uint8_t code[] = {static_cast<uint8_t>(Op::TASK_EXIT)};
    std::string line;
    size_t consumed = disasm_one(code, sizeof(code), 0, line);
    CHECK(consumed == 1);
    CHECK(line.find("TASK_EXIT") != std::string::npos);
  }

  SUBCASE("TASK_SLEEP disassembles")
  {
    uint8_t code[] = {static_cast<uint8_t>(Op::TASK_SLEEP)};
    std::string line;
    size_t consumed = disasm_one(code, sizeof(code), 0, line);
    CHECK(consumed == 1);
    CHECK(line.find("TASK_SLEEP") != std::string::npos);
  }

  SUBCASE("TASK_YIELD disassembles")
  {
    uint8_t code[] = {static_cast<uint8_t>(Op::TASK_YIELD)};
    std::string line;
    size_t consumed = disasm_one(code, sizeof(code), 0, line);
    CHECK(consumed == 1);
    CHECK(line.find("TASK_YIELD") != std::string::npos);
  }

  SUBCASE("CRITICAL_ENTER disassembles")
  {
    uint8_t code[] = {static_cast<uint8_t>(Op::CRITICAL_ENTER)};
    std::string line;
    size_t consumed = disasm_one(code, sizeof(code), 0, line);
    CHECK(consumed == 1);
    CHECK(line.find("CRITICAL_ENTER") != std::string::npos);
  }

  SUBCASE("CRITICAL_EXIT disassembles")
  {
    uint8_t code[] = {static_cast<uint8_t>(Op::CRITICAL_EXIT)};
    std::string line;
    size_t consumed = disasm_one(code, sizeof(code), 0, line);
    CHECK(consumed == 1);
    CHECK(line.find("CRITICAL_EXIT") != std::string::npos);
  }

  SUBCASE("TASK_SEND disassembles")
  {
    uint8_t code[] = {static_cast<uint8_t>(Op::TASK_SEND)};
    std::string line;
    size_t consumed = disasm_one(code, sizeof(code), 0, line);
    CHECK(consumed == 1);
    CHECK(line.find("TASK_SEND") != std::string::npos);
  }

  SUBCASE("TASK_RECEIVE disassembles")
  {
    uint8_t code[] = {static_cast<uint8_t>(Op::TASK_RECEIVE)};
    std::string line;
    size_t consumed = disasm_one(code, sizeof(code), 0, line);
    CHECK(consumed == 1);
    CHECK(line.find("TASK_RECEIVE") != std::string::npos);
  }

  SUBCASE("TASK_RECEIVE_BLOCKING disassembles")
  {
    uint8_t code[] = {static_cast<uint8_t>(Op::TASK_RECEIVE_BLOCKING)};
    std::string line;
    size_t consumed = disasm_one(code, sizeof(code), 0, line);
    CHECK(consumed == 1);
    CHECK(line.find("TASK_RECEIVE_BLOCKING") != std::string::npos);
  }

  SUBCASE("TASK_SELF disassembles")
  {
    uint8_t code[] = {static_cast<uint8_t>(Op::TASK_SELF)};
    std::string line;
    size_t consumed = disasm_one(code, sizeof(code), 0, line);
    CHECK(consumed == 1);
    CHECK(line.find("TASK_SELF") != std::string::npos);
  }

  SUBCASE("TASK_COUNT disassembles")
  {
    uint8_t code[] = {static_cast<uint8_t>(Op::TASK_COUNT)};
    std::string line;
    size_t consumed = disasm_one(code, sizeof(code), 0, line);
    CHECK(consumed == 1);
    CHECK(line.find("TASK_COUNT") != std::string::npos);
  }
}
