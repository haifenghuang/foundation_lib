/* main.c  -  Foundation crash test  -  Public Domain  -  2013 Mattias Jansson / Rampant Pixels
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

static bool _crash_callback_called = false;

static hash_t handled_context;
static char handled_condition[32];
static char handled_file[32];
static unsigned int handled_line;
static char handled_msg[32];
#if BUILD_ENABLE_LOG
static char handled_log[512];
static log_callback_fn _global_log_callback = 0;
#endif

static error_level_t _error_level_test;
static error_t _error_test;

static application_t
test_crash_application(void) {
	application_t app;
	memset(&app, 0, sizeof(app));
	app.name = string_const(STRING_CONST("Foundation crash tests"));
	app.short_name = string_const(STRING_CONST("test_crash"));
	app.config_dir = string_const(STRING_CONST("test_crash"));
	app.flags = APPLICATION_UTILITY;
	app.dump_callback = test_crash_handler;
	return app;
}

static memory_system_t
test_crash_memory_system(void) {
	return memory_system_malloc();
}

static foundation_config_t
test_crash_config(void) {
	foundation_config_t config;
	memset(&config, 0, sizeof(config));
	return config;
}

static int
test_crash_initialize(void) {
	return 0;
}

static void
test_crash_finalize(void) {
}

static void
test_crash_callback(const char* dump_path, size_t length) {
	FOUNDATION_UNUSED(dump_path);
#if !BUILD_ENABLE_LOG
	FOUNDATION_UNUSED(length);
#endif
	log_infof(HASH_TEST, STRING_CONST("Crash callback called: %.*s"), (int)length, dump_path);
	_crash_callback_called = true;
}

static int
handle_assert(hash_t context, const char* condition, size_t cond_length, const char* file,
              size_t file_length, unsigned int line, const char* msg, size_t msg_length) {
	handled_context = context;
	string_copy(handled_condition, sizeof(handled_condition), condition, cond_length);
	string_copy(handled_file, sizeof(handled_file), file, file_length);
	handled_line = line;
	string_copy(handled_msg, sizeof(handled_msg), msg, msg_length);
	return 1234;
}

#if BUILD_ENABLE_LOG

static void
handle_log(hash_t context, error_level_t severity, const char* msg, size_t msg_length) {
	FOUNDATION_UNUSED(context);
	FOUNDATION_UNUSED(severity);
	string_copy(handled_log, sizeof(handled_log), msg, msg_length);
	if (_global_log_callback)
		_global_log_callback(context, severity, msg, msg_length);
}

#endif

static int
instant_crash(void* arg) {
	FOUNDATION_UNUSED(arg);
	crash_debug_break();
#if !FOUNDATION_COMPILER_CLANG
	return 1;
#endif
}

static void*
thread_crash(void* arg) {
	return (void*)(uintptr_t)instant_crash(arg);
}

DECLARE_TEST(crash, assert_callback) {
	EXPECT_EQ(assert_handler(), 0);

	assert_set_handler(handle_assert);
	EXPECT_EQ(assert_handler(), handle_assert);

	log_enable_stdout(false);
	EXPECT_EQ(assert_report(1, STRING_CONST("condition"), STRING_CONST("file"), 2, STRING_CONST("msg")),
	          1234);
	log_enable_stdout(true);
	EXPECT_EQ(assert_handler(), handle_assert);
	EXPECT_EQ(handled_context, 1);
	EXPECT_STRINGEQ(string(handled_condition, string_length(handled_condition)), string_const(STRING_CONST("condition")));
	EXPECT_STRINGEQ(string(handled_file, string_length(handled_file)), string_const(STRING_CONST("file")));
	EXPECT_EQ(handled_line, 2);
	EXPECT_STRINGEQ(string(handled_msg, string_length(handled_msg)), string_const(STRING_CONST("msg")));

	assert_set_handler(0);
	EXPECT_EQ(assert_handler(), 0);

#if BUILD_ENABLE_LOG
	_global_log_callback = log_callback();
	log_set_callback(handle_log);
#endif
	log_enable_stdout(false);
	EXPECT_EQ(assert_report_formatted(1, STRING_CONST("assert_report_formatted"), STRING_CONST("file"),
	                                  2, STRING_CONST("%.*s"), 3, "msg"), 1);
	log_enable_stdout(true);
	EXPECT_EQ(error(), ERROR_ASSERT);
#if BUILD_ENABLE_LOG
	EXPECT_TRUE(string_find_string(handled_log, string_length(handled_log),
	                               STRING_CONST("assert_report_formatted"), 0) != STRING_NPOS);
	EXPECT_TRUE(string_find_string(handled_log, string_length(handled_log), STRING_CONST("msg"),
	                               0) != STRING_NPOS);

	log_enable_stdout(false);
	log_set_suppress(HASH_TEST, ERRORLEVEL_NONE);
#if BUILD_ENABLE_DEBUG_LOG
	log_debugf(HASH_TEST, STRING_CONST("%s"),
#else
	log_infof(HASH_TEST, STRING_CONST("%s"),
#endif
	           "To test log callback and memory handling this test will print "
	           "a really long log line with complete nonsense. Log callbacks only occur for non-suppressed "
	           "log levels, which is why this will be visible. However, it will not be printed to stdout. "
	           "Lorem ipsum dolor sit amet, an quas vivendum sed, in est summo conclusionemque, an est nulla nonumy option. "
	           "Malorum invidunt et mel, mei et hinc adolescens, eu velit deleniti urbanitas cum. Ei pericula omittantur duo, "
	           "eam ei malis pertinacia, eum hinc dictas et. Duo et velit dolorem explicari, an tacimates abhorreant qui, "
	           "esse possit intellegat ad vis. Eros populo numquam pro ea. Eius altera volumus duo ex, offendit comprehensam "
	           "sit te. Ea facete nostrum fabellas sea. Vel ea rebum ridens quodsi, etiam urbanitas mea an. Ornatus commune et his, "
	           "quo habeo denique an, id his amet diceret. Eam ei essent denique, cu quaestio perpetua vim. Mei utamur maluisset ex, "
	           "iriure tritani eu per. Pro at rebum maluisset, nec ei eirmod scaevola consulatu, ius in meis patrioque. Vis at summo "
	           "ancillae omnesque, inani moderatius delicatissimi qui an. Et illum vocibus eum, aliquando intellegat ex ius. Ius at "
	           "tation veritus. Scripta reprehendunt at sed. Hinc idque mollis in cum, at elit habemus civibus eam, sea et modus "
	           "eripuit. Alii ipsum electram id vel, mei alterum percipitur cu. Pro cu minim erant graecis, no vis tation nominavi "
	           "imperdiet, mei affert probatus ut. Quo veri modus ad, solet nostrud atomorum ius ea. Everti aliquid ne usu, populo "
	           "sapientem pro te. Persecuti definitionem qui ei, dicit dicunt ea quo. Sed minimum copiosae ei, pri dicat possit "
	           "urbanitas eu. Tritani interesset theophrastus id sit, phaedrum facilisis his eu. Dictas accusam eu quo. Ea democritum "
	           "consetetur vel. Iudicabit definitionem est eu, oportere temporibus at nec."
	          );
	log_set_suppress(HASH_TEST, ERRORLEVEL_DEBUG);
	log_enable_stdout(true);
	EXPECT_TRUE(string_find_string(handled_log, string_length(handled_log), STRING_CONST("Lorem ipsum"),
	                               0) != STRING_NPOS);

	log_set_callback(_global_log_callback);
#endif

	return 0;
}

static int
_error_callback_test(error_level_t level, error_t error) {
	_error_level_test = level;
	_error_test = error;
	return 2;
}

DECLARE_TEST(crash, error) {
	error_callback_fn callback;
	int ret;

	error();
	EXPECT_EQ(error(), ERROR_NONE);

	error_report(ERRORLEVEL_ERROR, ERROR_NONE);
	EXPECT_EQ(error(), ERROR_NONE);

	error_report(ERRORLEVEL_ERROR, ERROR_EXCEPTION);
	EXPECT_EQ(error(), ERROR_EXCEPTION);

	callback = error_callback();
	error_set_callback(_error_callback_test);

	ret = error_report(ERRORLEVEL_WARNING, ERROR_INVALID_VALUE);
	EXPECT_EQ(error(), ERROR_INVALID_VALUE);
	EXPECT_EQ(ret, 2);
	EXPECT_EQ(_error_level_test, ERRORLEVEL_WARNING);
	EXPECT_EQ(_error_test, ERROR_INVALID_VALUE);
	EXPECT_EQ(error_callback(), _error_callback_test);

	error_set_callback(callback);

	{
#if BUILD_ENABLE_ERROR_CONTEXT
		const char context_data[] = "another message";
#endif
		char context_buffer[512];
		string_t contextstr;
		error_context_clear();
		error_context_push(STRING_CONST("test context"), STRING_CONST("some message"));
		error_context_push(STRING_CONST("foo bar"), 0, 0);
		error_context_pop();
		error_context_pop();
		error_context_pop();
		error_context_push(STRING_CONST("test context"), STRING_CONST(context_data));

#if BUILD_ENABLE_ERROR_CONTEXT
		EXPECT_NE(error_context(), 0);
		EXPECT_EQ(error_context()->depth, 1);
		EXPECT_CONSTSTRINGEQ(error_context()->frame[0].name, string_const(STRING_CONST("test context")));
		EXPECT_EQ(error_context()->frame[0].data.str, context_data);
		EXPECT_EQ(error_context()->frame[0].data.length, sizeof(context_data) - 1);
#endif

		contextstr = error_context_buffer(context_buffer, 512);
#if BUILD_ENABLE_ERROR_CONTEXT
		EXPECT_NE_MSGFORMAT(string_find_string(STRING_ARGS(contextstr), STRING_CONST("test context"), 0),
		                    STRING_NPOS, "context name 'test context' not found in buffer: %s", context_buffer);
		EXPECT_NE_MSGFORMAT(string_find_string(STRING_ARGS(contextstr), STRING_CONST(context_data), 0),
		                    STRING_NPOS, "context data '%s' not found in buffer: %s", context_data, context_buffer);
#else
		EXPECT_EQ(contextstr.length, 0);
#endif

		error_context_clear();
		contextstr = error_context_buffer(context_buffer, 512);
#if BUILD_ENABLE_ERROR_CONTEXT
		EXPECT_STRINGEQ(contextstr, string_empty());
#endif
	}

	return 0;
}

DECLARE_TEST(crash, crash_guard) {
	int crash_result;

	if (system_debugger_attached() || (system_platform() == PLATFORM_PNACL))
		return 0; //Don't do crash tests with debugger attached

	_crash_callback_called = false;
	log_enable_stdout(false);
	crash_result = crash_guard(instant_crash, 0, test_crash_callback, STRING_CONST("instant_crash"));
	log_enable_stdout(true);
	EXPECT_EQ(crash_result, FOUNDATION_CRASH_DUMP_GENERATED);
	EXPECT_TRUE(_crash_callback_called);

	return 0;
}

DECLARE_TEST(crash, crash_thread) {
	thread_t thread;

	if (system_debugger_attached() || (system_platform() == PLATFORM_PNACL))
		return 0; //Don't do crash tests with debugger attached

	_crash_callback_called = false;
	crash_guard_set(test_crash_callback, STRING_CONST("thread_crash"));

	log_enable_stdout(false);
	thread_initialize(&thread, thread_crash, 0, STRING_CONST("crash"), THREAD_PRIORITY_NORMAL, 0);
	thread_start(&thread);
	while (!thread_is_started(&thread))
		thread_sleep(100);
	thread_finalize(&thread);
	log_enable_stdout(true);

	EXPECT_TRUE(_crash_callback_called);

	return 0;
}

static void
test_crash_declare(void) {
	ADD_TEST(crash, assert_callback);
	ADD_TEST(crash, error);
	ADD_TEST(crash, crash_guard);
	ADD_TEST(crash, crash_thread);
}

static test_suite_t test_crash_suite = {
	test_crash_application,
	test_crash_memory_system,
	test_crash_config,
	test_crash_declare,
	test_crash_initialize,
	test_crash_finalize
};

#if BUILD_MONOLITHIC

int
test_crash_run(void);

int
test_crash_run(void) {
	test_suite = test_crash_suite;
	return test_run_all();
}

#else

test_suite_t
test_suite_define(void);

test_suite_t
test_suite_define(void) {
	return test_crash_suite;
}

#endif
