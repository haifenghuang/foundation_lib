/* main.c  -  Foundation stacktrace test  -  Public Domain  -  2013 Mattias Jansson / Rampant Pixels
 *
 * This library provides a cross-platform foundation library in C11 providing basic support
 * data types and functions to write applications and games in a platform-independent fashion.
 * The latest source code is always available at
 *
 * https://github.com/rampantpixels/foundation_lib
 *
 * This library is put in the public domain; you can redistribute it and/or modify it without
 * any restrictions.
 */

#include <foundation/foundation.h>
#include <test/test.h>

static application_t
test_stacktrace_application(void) {
	application_t app;
	memset(&app, 0, sizeof(app));
	app.name = string_const(STRING_CONST("Foundation stacktrace tests"));
	app.short_name = string_const(STRING_CONST("test_stacktrace"));
	app.config_dir = string_const(STRING_CONST("test_stacktrace"));
	app.flags = APPLICATION_UTILITY;
	app.dump_callback = test_crash_handler;
	return app;
}

static memory_system_t
test_stacktrace_memory_system(void) {
	return memory_system_malloc();
}

static foundation_config_t
test_stacktrace_config(void) {
	foundation_config_t config;
	memset(&config, 0, sizeof(config));
	return config;
}

static int
test_stacktrace_initialize(void) {
	return 0;
}

static void
test_stacktrace_finalize(void) {
}

DECLARE_TEST(stacktrace, capture) {
#define TEST_DEPTH 64
	void* trace[TEST_DEPTH];
	size_t num_frames;

	if (system_platform() == PLATFORM_PNACL)
		return 0;

	num_frames = stacktrace_capture(trace, TEST_DEPTH, 0);
	EXPECT_GT(num_frames, 3);

	return 0;
}

DECLARE_TEST(stacktrace, resolve) {
#define TEST_DEPTH 64
	void* trace[TEST_DEPTH];
	size_t num_frames;
	char* buffer;
	string_t resolved;

	if (system_platform() == PLATFORM_PNACL)
		return 0;

	num_frames = stacktrace_capture(trace, TEST_DEPTH, 0);
	EXPECT_GT(num_frames, 3);

	buffer = memory_allocate(0, 1024, 0, MEMORY_TEMPORARY);
	resolved = stacktrace_resolve(buffer, 1024, trace, num_frames, 0);
	EXPECT_NE(resolved.str, 0);
	EXPECT_NE(resolved.length, 0);

	//log_infof(HASH_TEST, STRING_CONST("Resolved stack trace:\n%.*s"), (int)resolved.length,
	//          resolved.str);

#if !FOUNDATION_PLATFORM_ANDROID
	EXPECT_NE(string_find_string(resolved.str, resolved.length, STRING_CONST("stacktraceresolve_fn"),
	                             0), STRING_NPOS);
#if !BUILD_DEPLOY && !BUILD_PROFILE
	EXPECT_NE(string_find_string(resolved.str, resolved.length, STRING_CONST("test_run"), 0),
	          STRING_NPOS);
#endif
	EXPECT_NE(string_find_string(resolved.str, resolved.length, STRING_CONST("main"), 0), STRING_NPOS);
#endif

	memory_deallocate(buffer);

	return 0;
}

static void
test_stacktrace_declare(void) {
	ADD_TEST(stacktrace, capture);
	ADD_TEST(stacktrace, resolve);
}

static test_suite_t test_stacktrace_suite = {
	test_stacktrace_application,
	test_stacktrace_memory_system,
	test_stacktrace_config,
	test_stacktrace_declare,
	test_stacktrace_initialize,
	test_stacktrace_finalize
};

#if BUILD_MONOLITHIC

int
test_stacktrace_run(void);

int
test_stacktrace_run(void) {
	test_suite = test_stacktrace_suite;
	return test_run_all();
}

#else

test_suite_t
test_suite_define(void);

test_suite_t
test_suite_define(void) {
	return test_stacktrace_suite;
}

#endif
