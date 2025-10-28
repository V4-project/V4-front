/*
 * Demo: Error Position Tracking
 *
 * Demonstrates the new error position tracking and formatting features.
 */

#include <stdio.h>
#include "v4front/compile.h"

int main(void)
{
    V4FrontBuf buf;
    V4FrontError error;
    char formatted[512];

    printf("=== V4-front Error Position Tracking Demo ===\n\n");

    // Example 1: Unknown token
    printf("Example 1: Unknown token\n");
    printf("Source: \"1 2 UNKNOWN +\"\n\n");

    const char* source1 = "1 2 UNKNOWN +";
    if (v4front_compile_ex(source1, &buf, &error) < 0)
    {
        v4front_format_error(&error, source1, formatted, sizeof(formatted));
        printf("%s\n", formatted);
    }

    // Example 2: Multiline error
    printf("\nExample 2: Multiline error\n");
    printf("Source:\n");
    printf("  \"1 2 +\\n\"\n");
    printf("  \"3 BADWORD\\n\"\n");
    printf("  \"5 6 *\"\n\n");

    const char* source2 = "1 2 +\n3 BADWORD\n5 6 *";
    if (v4front_compile_ex(source2, &buf, &error) < 0)
    {
        v4front_format_error(&error, source2, formatted, sizeof(formatted));
        printf("%s\n", formatted);
    }

    // Example 3: Control flow error
    printf("\nExample 3: Control flow error\n");
    printf("Source: \"BEGIN 1 2 + REPEAT\"\n\n");

    const char* source3 = "BEGIN 1 2 + REPEAT";
    if (v4front_compile_ex(source3, &buf, &error) < 0)
    {
        v4front_format_error(&error, source3, formatted, sizeof(formatted));
        printf("%s\n", formatted);
    }

    // Example 4: Unclosed structure
    printf("\nExample 4: Unclosed structure\n");
    printf("Source: \": SQUARE DUP *\"\n\n");

    const char* source4 = ": SQUARE DUP *";
    if (v4front_compile_ex(source4, &buf, &error) < 0)
    {
        v4front_format_error(&error, source4, formatted, sizeof(formatted));
        printf("%s\n", formatted);
    }

    return 0;
}
