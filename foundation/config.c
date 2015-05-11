/* config.c  -  Foundation library  -  Public Domain  -  2013 Mattias Jansson / Rampant Pixels
 *
 * This library provides a cross-platform foundation library in C11 providing basic support data types and
 * functions to write applications and games in a platform-independent fashion. The latest source code is
 * always available at
 *
 * https://github.com/rampantpixels/foundation_lib
 *
 * This library is put in the public domain; you can redistribute it and/or modify it without any restrictions.
 *
 */

#include <foundation/foundation.h>
#include <foundation/internal.h>


#define CONFIG_SECTION_BUCKETS    7
#define CONFIG_KEY_BUCKETS        11

enum config_value_type_t
{
	CONFIGVALUE_BOOL = 0,
	CONFIGVALUE_INT,
	CONFIGVALUE_REAL,
	CONFIGVALUE_STRING,
	CONFIGVALUE_STRING_CONST,
	CONFIGVALUE_STRING_VAR,
	CONFIGVALUE_STRING_CONST_VAR
};
typedef enum config_value_type_t config_value_type_t;

struct config_key_t
{
	hash_t                  name;
	config_value_type_t     type;
	bool                    bval;
	int64_t                 ival;
	string_t                sval;
	string_t                expanded;
	real                    rval;
};
typedef FOUNDATION_ALIGN(8) struct config_key_t config_key_t;

struct config_section_t
{
	hash_t                  name;
	config_key_t*           key[CONFIG_KEY_BUCKETS];
};
typedef FOUNDATION_ALIGN(8) struct config_section_t config_section_t;

FOUNDATION_STATIC_ASSERT( FOUNDATION_ALIGNOF( config_key_t ) == 8, "config_key_t alignment" );
FOUNDATION_STATIC_ASSERT( FOUNDATION_ALIGNOF( config_section_t ) == 8, "config_section_t alignment" );

//Global config store
static config_section_t* _config_section[CONFIG_SECTION_BUCKETS];


static int64_t _config_string_to_int( string_const_t str )
{
	size_t first_nonnumeric;
	size_t dot_position;
	if( str.length < 2 )
		return string_to_int64( str.str, str.length );

	first_nonnumeric = string_find_first_not_of( str.str, str.length, STRING_CONST( "0123456789." ), 0 );
	if( ( first_nonnumeric == ( str.length - 1 ) ) && ( ( str.str[ first_nonnumeric ] == 'm' ) || ( str.str[ first_nonnumeric ] == 'M' ) ) )
	{
		dot_position = string_find( str.str, str.length, '.', 0 );
		if( dot_position != STRING_NPOS )
		{
			if( string_find( str.str, str.length, '.', dot_position + 1 ) == STRING_NPOS )
				return (int64_t)( string_to_float64( str.str, str.length ) * ( 1024.0 * 1024.0 ) );
			return string_to_int64( str.str, str.length ); //More than one dot
		}
		return string_to_int64( str.str, str.length ) * ( 1024LL * 1024LL );
	}
	if( ( first_nonnumeric == ( str.length - 1 ) ) && ( ( str.str[ first_nonnumeric ] == 'k' ) || ( str.str[ first_nonnumeric ] == 'K' ) ) )
	{
		dot_position = string_find( str.str, str.length, '.', 0 );
		if( dot_position != STRING_NPOS )
		{
			 if( string_find( str.str, str.length, '.', dot_position + 1 ) == STRING_NPOS )
				return (int64_t)( string_to_float64( str.str, str.length ) * 1024.0 );
			 return string_to_int64( str.str, str.length ); //More than one dot
		}
		return string_to_int64( str.str, str.length ) * 1024LL;
	}

	return string_to_int64( str.str, str.length );
}


static real _config_string_to_real( string_const_t str )
{
	size_t first_nonnumeric;
	size_t dot_position;
	if( str.length < 2 )
		return string_to_real( str.str, str.length );

	first_nonnumeric = string_find_first_not_of( str.str, str.length, STRING_CONST( "0123456789." ), 0 );
	if( ( first_nonnumeric == ( str.length - 1 ) ) && ( ( str.str[ first_nonnumeric ] == 'm' ) || ( str.str[ first_nonnumeric ] == 'M' ) ) )
	{
		dot_position = string_find( str.str, str.length, '.', 0 );
		if( dot_position != STRING_NPOS )
		{
			if( string_find( str.str, str.length, '.', dot_position + 1 ) != STRING_NPOS )
				return string_to_real( str.str, str.length ); //More than one dot
		}
		return string_to_real( str.str, str.length ) * ( REAL_C( 1024.0 ) * REAL_C( 1024.0 ) );
	}
	if( ( first_nonnumeric == ( str.length - 1 ) ) && ( ( str.str[ first_nonnumeric ] == 'k' ) || ( str.str[ first_nonnumeric ] == 'K' ) ) )
	{
		dot_position = string_find( str.str, str.length, '.', 0 );
		if( dot_position != STRING_NPOS )
		{
			if( string_find( str.str, str.length, '.', dot_position + 1 ) != STRING_NPOS )
				return string_to_real( str.str, str.length ); //More than one dot
		}
		return string_to_real( str.str, str.length ) * REAL_C( 1024.0 );
	}

	return string_to_real( str.str, str.length );
}


static FOUNDATION_NOINLINE string_const_t _expand_environment( hash_t key, string_const_t var )
{
	if( key == HASH_EXECUTABLE_NAME )
		return environment_executable_name();
	else if( key == HASH_EXECUTABLE_DIRECTORY )
		return environment_executable_directory();
	else if( key == HASH_EXECUTABLE_PATH )
		return environment_executable_path();
	else if( key == HASH_INITIAL_WORKING_DIRECTORY )
		return environment_initial_working_directory();
	else if( key == HASH_CURRENT_WORKING_DIRECTORY )
		return environment_current_working_directory();
	else if( key == HASH_HOME_DIRECTORY )
		return environment_home_directory();
	else if( key == HASH_TEMPORARY_DIRECTORY )
		return environment_temporary_directory();
	else if( string_equal( var.str, var.length, STRING_CONST( "variable[" ) ) )  //variable[varname] - Environment variable named "varname"
	{
		string_const_t substr = string_substr_const( var.str, var.length, 9, var.length - 9 );
		return environment_variable( substr.str, substr.length );
	}
	return string_null();
}


static FOUNDATION_NOINLINE string_t _expand_string( hash_t section_current, string_t str )
{
	string_t expanded;
	string_const_t variable;
	size_t var_pos, var_end_pos, separator, var_offset;
	hash_t section, key;

	expanded = str;
	var_pos = string_find_string( expanded.str, expanded.length, STRING_CONST( "$(" ), 0 );

	while( var_pos != STRING_NPOS )
	{
		var_end_pos = string_find( expanded.str, expanded.length, ')', var_pos + 2 );
		FOUNDATION_ASSERT_MSG( var_end_pos != STRING_NPOS, "Malformed config variable statement" );
		variable = string_substr_const( expanded.str, expanded.length, var_pos, ( var_end_pos != STRING_NPOS ) ? ( 1 + var_end_pos - var_pos ) : STRING_NPOS );

		section = section_current;
		key = 0;
		separator = string_find( variable.str, variable.length, ':', 0 );
		if( separator != STRING_NPOS )
		{
			if( separator != 2 )
				section = hash( variable.str + 2, separator - 2 );
			var_offset = separator + 1;
		}
		else
		{
			var_offset = 2;
		}
		key = hash( variable.str + var_offset, variable.length - ( var_offset + ( variable.str[ variable.length - 1 ] == ')' ? 1 : 0 ) ) );

		if( expanded.str == str.str )
			expanded = string_clone( str.str, str.length );

		if( section != HASH_ENVIRONMENT )
		{
			string_const_t value = config_string( section, key );
			expanded = string_replace( expanded.str, expanded.length, variable.str, variable.length, value.str, value.length, false );
		}
		else
		{
			string_const_t value = _expand_environment( key, string_substr_const( variable.str, variable.length, var_offset, variable.length - var_offset ) );
			expanded = string_replace( expanded.str, expanded.length, variable.str, variable.length, value.str, value.length, false );
		}

		var_pos = string_find_string( expanded.str, expanded.length, STRING_CONST( "$(" ), 0 );
	}
#if BUILD_ENABLE_CONFIG_DEBUG
	if( str.str != expanded.str )
		log_debugf( HASH_CONFIG, STRING_CONST( "Expanded config value \"%.*s\" to \"%.*s\"" ), (int)str.length, str.str, (int)expanded.length, expanded.str );
#endif

	return expanded;
}


static FOUNDATION_NOINLINE void _expand_string_val( hash_t section, config_key_t* key )
{
	bool is_true;
	FOUNDATION_ASSERT( key->sval.str );
	if( key->expanded.str != key->sval.str )
		string_deallocate( key->expanded.str );
	key->expanded = _expand_string( section, key->sval );

	is_true = string_equal( key->expanded.str, key->expanded.length, STRING_CONST( "true" ) );
	key->bval = ( string_equal( key->expanded.str, key->expanded.length, STRING_CONST( "false" ) ) ||
				  string_equal( key->expanded.str, key->expanded.length, STRING_CONST( "0" ) ) ||
				  !key->expanded.length ) ? false : true;
	key->ival = is_true ? 1 : _config_string_to_int( string_to_const( key->expanded ) );
	key->rval = is_true ? REAL_C(1.0) : _config_string_to_real( string_to_const( key->expanded ) );
}


int _config_initialize( void )
{
	config_load( STRING_CONST( "foundation" ), 0ULL, true, false );
	config_load( STRING_CONST( "application" ), 0ULL, true, false );

	//Load per-user config
	config_load( STRING_CONST( "user" ), HASH_USER, false, true );

	return 0;
}


void _config_shutdown( void )
{
	size_t isb, is, ikb, ik, ssize, ksize;
	config_section_t* section;
	config_key_t* key;
	for( isb = 0; isb < CONFIG_SECTION_BUCKETS; ++isb )
	{
		section = _config_section[isb];
		for( is = 0, ssize = array_size( section ); is < ssize; ++is )
		{
			for( ikb = 0; ikb < CONFIG_KEY_BUCKETS; ++ikb )
			{
				/*lint -e{613} array_size( section ) in loop condition does the null pointer guard */
				key = section[is].key[ikb];
				for( ik = 0, ksize = array_size( key ); ik < ksize; ++ik )
				{
					/*lint --e{613} array_size( key ) in loop condition does the null pointer guard */
					if( key[ik].expanded.str != key[ik].sval.str )
						string_deallocate( key[ik].expanded.str );
					if( ( key[ik].type != CONFIGVALUE_STRING_CONST ) && ( key[ik].type != CONFIGVALUE_STRING_CONST_VAR ) )
						string_deallocate( key[ik].sval.str );
				}
				array_deallocate( key );
			}
		}
		array_deallocate( section );
	}
}


void config_load( const char* name, size_t length, hash_t filter_section, bool built_in, bool overwrite )
{
	/*lint --e{838} Safety null assign all pointers for all preprocessor paths */
	/*lint --e{750} Unused macros in some paths */
#define NUM_SEARCH_PATHS 10
	string_const_t exe_path;
	string_t sub_exe_path = { 0, 0 };
	string_t exe_parent_path = { 0, 0 };
	string_t exe_processed_path = { 0, 0 };
	string_t abs_exe_parent_path = { 0, 0 };
	string_t abs_exe_processed_path = { 0, 0 };
	string_t bundle_path = { 0, 0 };
	string_t home_dir = { 0, 0 };
	string_t cwd_config_path = { 0, 0 };
	string_const_t paths[NUM_SEARCH_PATHS];
#if !FOUNDATION_PLATFORM_FAMILY_MOBILE && !FOUNDATION_PLATFORM_PNACL
	const string_const_t* cmd_line;
	size_t icl, clsize;
#endif
	size_t start_path, i, j;

	string_const_t buildsuffix[4] = {
		(string_const_t){ STRING_CONST( "/debug" ) },
		(string_const_t){ STRING_CONST( "/release" ) },
		(string_const_t){ STRING_CONST( "/profile" ) },
		(string_const_t){ STRING_CONST( "/deploy" ) }
	};
	string_const_t platformsuffix[PLATFORM_INVALID+1] = {
		(string_const_t){ STRING_CONST( "/windows" ) },
		(string_const_t){ STRING_CONST( "/osx" ) },
		(string_const_t){ STRING_CONST( "/ios" ) },
		(string_const_t){ STRING_CONST( "/android" ) },
		(string_const_t){ STRING_CONST( "/raspberrypi" ) },
		(string_const_t){ STRING_CONST( "/pnacl" ) },
		(string_const_t){ STRING_CONST( "/bsd" ) },
		(string_const_t){ STRING_CONST( "/tizen" ) },
		(string_const_t){ STRING_CONST( "/unknown" ) }
	};
	string_const_t binsuffix[1] = {
		(string_const_t){ STRING_CONST( "/bin" ) }
	};

	FOUNDATION_ASSERT( name );

	memset( paths, 0, sizeof( string_const_t ) * NUM_SEARCH_PATHS );

	exe_path = environment_executable_directory();
	sub_exe_path = path_merge( exe_path.str, exe_path.length, STRING_CONST( "config" ) );
	exe_parent_path = path_merge( exe_path.str, exe_path.length, STRING_CONST( "../config" ) );
	exe_parent_path = path_clean( exe_parent_path.str, exe_parent_path.length, exe_parent_path.length, path_is_absolute( exe_parent_path.str, exe_parent_path.length ), true );
	abs_exe_parent_path = path_make_absolute( exe_parent_path.str, exe_parent_path.length );

	exe_processed_path = string_clone( exe_path.str, exe_path.length );
	for( i = 0; i < 4; ++i )
	{
		if( string_ends_with( exe_processed_path.str, exe_processed_path.length, buildsuffix[i].str, buildsuffix[i].length ) )
		{
			exe_processed_path.length = exe_processed_path.length - buildsuffix[i].length;
			exe_processed_path.str[ exe_processed_path.length ] = 0;
			break;
		}
	}
	for( i = 0; i < 7; ++i )
	{
		if( string_ends_with( exe_processed_path.str, exe_processed_path.length, platformsuffix[i].str, platformsuffix[i].length ) )
		{
			exe_processed_path.length = exe_processed_path.length - platformsuffix[i].length;
			exe_processed_path.str[ exe_processed_path.length ] = 0;
			break;
		}
	}
	for( i = 0; i < 1; ++i )
	{
		if( string_ends_with( exe_processed_path.str, exe_processed_path.length, binsuffix[i].str, binsuffix[i].length ) )
		{
			exe_processed_path.length = exe_processed_path.length - binsuffix[i].length;
			exe_processed_path.str[ exe_processed_path.length ] = 0;
			break;
		}
	}
	exe_processed_path = path_append( exe_processed_path.str, exe_processed_path.length, STRING_CONST( "config" ) );
	abs_exe_processed_path = path_make_absolute( exe_processed_path.str, exe_processed_path.length );

	paths[0] = exe_path;
#if !FOUNDATION_PLATFORM_PNACL
	paths[1] = string_to_const( sub_exe_path );
	paths[2] = string_to_const( abs_exe_parent_path );
#endif
	paths[3] = string_to_const( abs_exe_processed_path );

#if FOUNDATION_PLATFORM_FAMILY_DESKTOP && !BUILD_DEPLOY
	paths[4] = environment_initial_working_directory();
#endif

#if FOUNDATION_PLATFORM_MACOSX || FOUNDATION_PLATFORM_IOS
	bundle_path = string_clone_string( environment_executable_directory() );
#  if FOUNDATION_PLATFORM_MACOSX
	bundle_path = path_append( bundle_path.str, bundle_path.length, STRING_CONST( "../Resources/config" ) );
#  else
	bundle_path = path_append( bundle_path.str, bundle_path.length, STRING_CONST( "config" ) );
#  endif
	bundle_path = path_clean( bundle_path.str, bundle_path.length, bundle_path.length, path_is_absolute( bundle_path.str, bundle_path.length ), true );
	paths[5] = string_to_const( bundle_path );
#elif FOUNDATION_PLATFORM_ANDROID
#define ANDROID_ASSET_PATH_INDEX 5
	paths[5] = (string_const_t){ STRING_CONST( "/config" ) };
#endif

#if FOUNDATION_PLATFORM_FAMILY_DESKTOP
	paths[6] = environment_current_working_directory();
#endif

	string_deallocate( exe_parent_path.str );
	string_deallocate( exe_processed_path.str );

#if FOUNDATION_PLATFORM_FAMILY_DESKTOP
	cwd_config_path = string_clone_string( environment_current_working_directory());
	cwd_config_path = path_append( cwd_config_path.str, cwd_config_path.length, STRING_CONST( "config" ) );
	paths[7] = string_to_const( cwd_config_path );

	cmd_line = environment_command_line();
	/*lint -e{850} We modify loop var to skip extra arg */
	for( icl = 0, clsize = array_size( cmd_line ); icl < clsize; ++icl )
	{
		/*lint -e{613} array_size( cmd_line ) in loop condition does the null pointer guard */
		if( string_equal( cmd_line[icl].str, cmd_line[icl].length, STRING_CONST( "--configdir" ) ) )
		{
			if( string_equal( cmd_line[icl].str, cmd_line[icl].length, STRING_CONST( "--configdir=" ) ) )
			{
				paths[8] = string_substr_const( cmd_line[icl].str, cmd_line[icl].length, 12, STRING_NPOS );
			}
			else if( icl < ( clsize - 1 ) )
			{
				paths[8] = cmd_line[++icl];
			}
		}
	}
#endif

	start_path = 0;

#if FOUNDATION_PLATFORM_WINDOWS || FOUNDATION_PLATFORM_LINUX || FOUNDATION_PLATFORM_MACOSX || FOUNDATION_PLATFORM_BSD
	if( !built_in )
	{
		string_const_t home_path = environment_home_directory();
		home_dir = string_format( STRING_CONST( "%*.s/.%.*s" ), (int)home_path.length, home_path.str,
			(int)environment_application()->config_dir.length, environment_application()->config_dir.str );
		start_path = 9;
	}
#endif

	for( i = start_path; i < NUM_SEARCH_PATHS; ++i )
	{
		char filename_buffer[FOUNDATION_MAX_PATHLEN];
		string_t filename;
		stream_t* istream;
		bool path_already_searched = false;

		if( !paths[i].length )
			continue;

		for( j = start_path; j < i; ++j )
		{
			if( string_equal( paths[j].str, paths[j].length, paths[i].str, paths[i].length ) )
			{
				path_already_searched = true;
				break;
			}
		}
		if( path_already_searched )
			continue;

		//TODO: Support loading configs from virtual file system (i.e in zip/other packages)
		filename = string_format_buffer( filename_buffer, FOUNDATION_MAX_PATHLEN, STRING_CONST( "%.*s/%.*s.ini" ),
			(int)paths[i].length, paths[i].str, (int)length, name );
		filename = path_clean( filename.str, filename.length, FOUNDATION_MAX_PATHLEN, path_is_absolute( filename.str, filename.length ), false );
		istream = 0;
#if FOUNDATION_PLATFORM_ANDROID
		if( i == ANDROID_ASSET_PATH_INDEX )
			istream = asset_stream_open( filename.str, filename.length, STREAM_IN );
		else
#endif
		istream = stream_open( filename.str, filename.length, STREAM_IN );
		if( istream )
		{
			config_parse( istream, filter_section, overwrite );
			stream_deallocate( istream );
		}

		if( built_in )
		{
			const char* platform_name =
#if FOUNDATION_PLATFORM_WINDOWS
				"windows";
#elif FOUNDATION_PLATFORM_LINUX_RASPBERRYPI
				"raspberrypi";
#elif FOUNDATION_PLATFORM_LINUX
				"linux";
#elif FOUNDATION_PLATFORM_MACOSX
				"osx";
#elif FOUNDATION_PLATFORM_IOS
				"ios";
#elif FOUNDATION_PLATFORM_ANDROID
				"android";
#elif FOUNDATION_PLATFORM_PNACL
				"pnacl";
#elif FOUNDATION_PLATFORM_BSD
				"bsd";
#elif FOUNDATION_PLATFORM_TIZEN
				"tizen";
#else
#  error Insert platform name
				"unknown";
#endif
			filename = string_format_buffer( filename_buffer, FOUNDATION_MAX_PATHLEN, STRING_CONST( "%.*s/%s/%.*s.ini" ),
				(int)paths[i].length, paths[i].str, platform_name, (int)length, name );
			filename = path_clean( filename.str, filename.length, FOUNDATION_MAX_PATHLEN, path_is_absolute( filename.str, filename.length ), false );
#if FOUNDATION_PLATFORM_ANDROID
			if( i == ANDROID_ASSET_PATH_INDEX )
				istream = asset_stream_open( filename.str, filename.length, STREAM_IN );
			else
#endif
			istream = stream_open( filename.str, filename.length, STREAM_IN );
			if( istream )
			{
				config_parse( istream, filter_section, overwrite );
				stream_deallocate( istream );
			}
		}
	}

	string_deallocate( home_dir.str );
	string_deallocate( sub_exe_path.str );
	string_deallocate( abs_exe_processed_path.str );
	string_deallocate( abs_exe_parent_path.str );
	string_deallocate( bundle_path.str );
	string_deallocate( cwd_config_path.str );
}


static FOUNDATION_NOINLINE config_section_t* config_section( hash_t section, bool create )
{
	config_section_t* bucket;
	size_t ib, bsize;

	bucket = _config_section[ section % CONFIG_SECTION_BUCKETS ];
	for( ib = 0, bsize = array_size( bucket ); ib < bsize; ++ib )
	{
		/*lint --e{613} array_size( bucket ) in loop condition does the null pointer guard */
		if( bucket[ib].name == section )
			return bucket + ib;
	}

	if( !create )
		return 0;

	//TODO: Thread safeness
	{
		config_section_t new_section;
		memset( &new_section, 0, sizeof( new_section ) );
		new_section.name = section;

		array_push_memcpy( bucket, &new_section );
		_config_section[ section % CONFIG_SECTION_BUCKETS ] = bucket;
	}

	return bucket + bsize;
}


static FOUNDATION_NOINLINE config_key_t* config_key( hash_t section, hash_t key, bool create )
{
	config_key_t new_key;
	config_section_t* csection;
	config_key_t* bucket;
	size_t ib, bsize;

	csection = config_section( section, create );
	if( !csection )
	{
		FOUNDATION_ASSERT( !create );
		return 0;
	}
	bucket = csection->key[ key % CONFIG_KEY_BUCKETS ];
	for( ib = 0, bsize = array_size( bucket ); ib < bsize; ++ib )
	{
		/*lint --e{613} array_size( bucket ) in loop condition does the null pointer guard */
		if( bucket[ib].name == key )
			return bucket + ib;
	}

	if( !create )
		return 0;

	memset( &new_key, 0, sizeof( new_key ) );
	new_key.name = key;

	//TODO: Thread safeness
	array_push_memcpy( bucket, &new_key );
	csection->key[ key % CONFIG_KEY_BUCKETS ] = bucket;

	return bucket + bsize;
}


bool config_bool( hash_t section, hash_t key )
{
	config_key_t* key_val = config_key( section, key, false );
	if( key_val && ( key_val->type >= CONFIGVALUE_STRING_VAR ) )
		_expand_string_val( section, key_val );
	return key_val ? key_val->bval : false;
}


int64_t config_int( hash_t section, hash_t key )
{
	config_key_t* key_val = config_key( section, key, false );
	if( key_val && ( key_val->type >= CONFIGVALUE_STRING_VAR ) )
		_expand_string_val( section, key_val );
	return key_val ? key_val->ival : 0;
}


real config_real( hash_t section, hash_t key )
{
	config_key_t* key_val = config_key( section, key, false );
	if( key_val && ( key_val->type >= CONFIGVALUE_STRING_VAR ) )
		_expand_string_val( section, key_val );
	return key_val ? key_val->rval : 0;
}


string_const_t config_string( hash_t section, hash_t key )
{
	config_key_t* key_val = config_key( section, key, false );
	if( !key_val )
		return string_const( "", 0 );
	//Convert to string
	/*lint --e{788} We use default for remaining enums */
	switch( key_val->type )
	{
		case CONFIGVALUE_BOOL:
		{
			return key_val->bval ? (string_const_t){ STRING_CONST( "true" ) } : (string_const_t){ STRING_CONST( "false" ) };
		}

		case CONFIGVALUE_INT:
		{
			if( !key_val->sval.str )
				key_val->sval = string_from_int( key_val->ival, 0, 0 );
			return string_to_const( key_val->sval );
		}

		case CONFIGVALUE_REAL:
		{
			if( !key_val->sval.str )
				key_val->sval = string_from_real( key_val->rval, 4, 0, '0' );
			return string_to_const( key_val->sval );
		}

		case CONFIGVALUE_STRING:
		case CONFIGVALUE_STRING_CONST:
		case CONFIGVALUE_STRING_VAR:
		case CONFIGVALUE_STRING_CONST_VAR:
			break;
	}
	//String value of some form
	if( !key_val->sval.str )
		return string_const( "", 0 );
	if( key_val->type >= CONFIGVALUE_STRING_VAR )
	{
		_expand_string_val( section, key_val );
		return string_to_const( key_val->expanded );
	}
	return string_to_const( key_val->sval );
}


hash_t config_string_hash( hash_t section, hash_t key )
{
	string_const_t value = config_string( section, key );
	return value.length ? hash( value.str, value.length ) : HASH_EMPTY_STRING;
}


#define CLEAR_KEY_STRINGS( key_val ) \
	if( key_val->expanded.str != key_val->sval.str ) \
		string_deallocate( key_val->expanded.str ); \
	if( ( key_val->type != CONFIGVALUE_STRING_CONST ) && ( key_val->type != CONFIGVALUE_STRING_CONST_VAR ) ) \
		string_deallocate( key_val->sval.str ); \
	key_val->expanded = key_val->sval = (string_t){ 0, 0 }


void config_set_bool( hash_t section, hash_t key, bool value )
{
	config_key_t* key_val = config_key( section, key, true );
	if( !FOUNDATION_VALIDATE( key_val ) ) return;
	key_val->bval = value;
	key_val->ival = ( value ? 1 : 0 );
	key_val->rval = ( value ? REAL_C( 1.0 ) : REAL_C( 0.0 ) );
	CLEAR_KEY_STRINGS( key_val );
	key_val->type = CONFIGVALUE_BOOL;
}


void config_set_int( hash_t section, hash_t key, int64_t value )
{
	config_key_t* key_val = config_key( section, key, true );
	if( !FOUNDATION_VALIDATE( key_val ) ) return;
	key_val->bval = value ? true : false;
	key_val->ival = value;
	key_val->rval = (real)value;
	CLEAR_KEY_STRINGS( key_val );
	key_val->type = CONFIGVALUE_INT;
}


void config_set_real( hash_t section, hash_t key, real value )
{
	config_key_t* key_val = config_key( section, key, true );
	if( !FOUNDATION_VALIDATE( key_val ) ) return;
	key_val->bval = !math_realzero( value );
	key_val->ival = (int64_t)value;
	key_val->rval = value;
	CLEAR_KEY_STRINGS( key_val );
	key_val->type = CONFIGVALUE_REAL;
}


void config_set_string( hash_t section, hash_t key, const char* value, size_t length )
{
	config_key_t* key_val = config_key( section, key, true );
	if( !FOUNDATION_VALIDATE( key_val ) ) return;
	CLEAR_KEY_STRINGS( key_val );

	key_val->sval = string_clone( value, length );
	key_val->type = ( ( string_find_string( key_val->sval.str, key_val->sval.length, STRING_CONST( "$(" ), 0 ) != STRING_NPOS ) ? CONFIGVALUE_STRING_VAR : CONFIGVALUE_STRING );

	if( key_val->type == CONFIGVALUE_STRING )
	{
		bool is_true = string_equal( key_val->sval.str, key_val->sval.length, STRING_CONST( "true" ) );
		key_val->bval = ( string_equal( key_val->sval.str, key_val->sval.length, STRING_CONST( "false" ) ) ||
		                  string_equal( key_val->sval.str, key_val->sval.length, STRING_CONST( "0" ) ) ||
		                  !key_val->sval.length ) ? false : true;
		key_val->ival = is_true ? 1 : _config_string_to_int( string_to_const( key_val->sval ) );
		key_val->rval = is_true ? REAL_C(1.0) : _config_string_to_real( string_to_const( key_val->sval ) );
	}
}


void config_set_string_constant( hash_t section, hash_t key, const char* value, size_t length )
{
	config_key_t* key_val = config_key( section, key, true );
	if( !FOUNDATION_VALIDATE( key_val ) ) return;
	if( !FOUNDATION_VALIDATE( value ) ) return;
	CLEAR_KEY_STRINGS( key_val );

	//key_val->sval = (char*)value;
	memcpy( &key_val->sval.str, &value, sizeof( char* ) ); //Yeah yeah, we're storing a const pointer in a non-const var
	key_val->sval.length = length;
	key_val->type = ( ( string_find_string( key_val->sval.str, key_val->sval.length, STRING_CONST( "$(" ), 0 ) != STRING_NPOS ) ? CONFIGVALUE_STRING_CONST_VAR : CONFIGVALUE_STRING_CONST );

	if( key_val->type == CONFIGVALUE_STRING_CONST )
	{
		bool is_true = string_equal( key_val->sval.str, key_val->sval.length, STRING_CONST( "true" ) );
		key_val->bval = ( string_equal( key_val->sval.str, key_val->sval.length, STRING_CONST( "false" ) ) ||
		                  string_equal( key_val->sval.str, key_val->sval.length, STRING_CONST( "0" ) ) ||
		                  !key_val->sval.length ) ? false : true;
		key_val->ival = is_true ? 1 : _config_string_to_int( string_to_const( key_val->sval ) );
		key_val->rval = is_true ? REAL_C(1.0) : _config_string_to_real( string_to_const( key_val->sval ) );
	}
}


void config_parse( stream_t* stream, hash_t filter_section, bool overwrite )
{
	string_t buffer, stripped;
	hash_t section = 0;
	hash_t key = 0;
	unsigned int line = 0;
	size_t buffer_size;
	string_const_t path;

	path = stream_path( stream );
#if BUILD_ENABLE_CONFIG_DEBUG
	log_debugf( HASH_CONFIG, STRING_CONST( "Parsing config stream: %.*s" ), (int)path.length, path.str );
#endif
	buffer.length = buffer_size = 1024;
	buffer.str = memory_allocate( 0, buffer.length, 0, MEMORY_TEMPORARY | MEMORY_ZERO_INITIALIZED );	
	while( !stream_eos( stream ) )
	{
		++line;
		buffer.length = stream_read_line_buffer( stream, buffer.str, buffer_size, '\n' );
		stripped = string_strip( buffer.str, buffer.length, STRING_CONST( " \t\n\r" ) );
		if( !stripped.length || ( stripped.str[0] == ';' ) || ( stripped.str[0] == '#' ) )
			continue;
		if( stripped.str[0] == '[' )
		{
			//Section declaration
			size_t endpos = string_rfind( stripped.str, stripped.length, ']', stripped.length - 1 );
			if( endpos < 1 )
			{
				log_warnf( HASH_CONFIG, WARNING_BAD_DATA, "Invalid section declaration on line %u in config stream '%.*s'", line, (int)path.length, path.str );
				continue;
			}
			stripped.str[endpos] = 0;
			section = hash( stripped.str + 1, endpos - 1 );
#if BUILD_ENABLE_CONFIG_DEBUG
			log_debugf( HASH_CONFIG, "  config: section set to '%.*s' (0x%" PRIx64 ")", (int)endpos - 1, stripped.str + 1, section );
#endif
		}
		else if( !filter_section || ( filter_section == section ) )
		{
			//name=value declaration
			string_const_t name;
			string_const_t value;
			size_t separator = string_find( stripped.str, stripped.length, '=', 0 );
			if( separator == STRING_NPOS )
			{
				log_warnf( HASH_CONFIG, WARNING_BAD_DATA, "Invalid value declaration on line %u in config stream '%.*s', missing assignment operator '=': %.*s", line, (int)path.length, path.str, (int)stripped.length, stripped.str );
				continue;
			}

			name = string_strip_const( stripped.str, separator, STRING_CONST( " \t" ) );
			value = string_strip( buffer + separator + 1, " \t" );
			if( !string_length( name ) )
			{
				log_warnf( HASH_CONFIG, WARNING_BAD_DATA, "Invalid value declaration on line %d in config stream '%s', empty name string", line, stream_path( stream ) );
				continue;
			}

			key = hash( name, string_length( name ) );

			if( overwrite || !config_key( section, key, false ) )
			{
#if BUILD_ENABLE_CONFIG_DEBUG
				log_debugf( HASH_CONFIG, "  config: %s (0x%llx) = %s", name, key, value );
#endif

				if( !string_length( value ) )
					config_set_string( section, key, "" );
				else if( string_equal( value, "false" ) )
					config_set_bool( section, key, false );
				else if( string_equal( value, "true" ) )
					config_set_bool( section, key, true );
				else if( ( string_find( value, '.', 0 ) != STRING_NPOS ) && ( string_find_first_not_of( value, "0123456789.", 0 ) == STRING_NPOS ) && ( string_find( value, '.', string_find( value, '.', 0 ) + 1 ) == STRING_NPOS ) ) //Exactly one "."
					config_set_real( section, key, string_to_real( value ) );
				else if( string_find_first_not_of( value, "0123456789", 0 ) == STRING_NPOS )
					config_set_int( section, key, string_to_int64( value ) );
				else
					config_set_string( section, key, value );
			}
		}
	}
	memory_deallocate( buffer );
}


void config_parse_commandline( const char* const* cmdline, size_t num )
{
	//TODO: Implement, format --section:key=value
	size_t arg;
	for( arg = 0; arg < num; ++arg )
	{
		if( string_match_pattern( cmdline[arg], "--*:*=*" ) )
		{
			size_t first_sep = string_find( cmdline[arg], ':', 0 );
			size_t second_sep = string_find( cmdline[arg], '=', 0 );
			if( ( first_sep != STRING_NPOS ) && ( second_sep != STRING_NPOS ) && ( first_sep < second_sep ) )
			{
				size_t section_length = first_sep - 2;
				size_t end_pos = first_sep + 1;
				size_t key_length = second_sep - end_pos;

				const char* section_str = cmdline[arg] + 2;
				const char* key_str = pointer_offset_const( cmdline[arg], end_pos );

				hash_t section = hash( section_str, section_length );
				hash_t key = hash( key_str, key_length );

				char* value = string_substr( cmdline[arg], second_sep + 1, STRING_NPOS );
				char* set_value = value;

				size_t value_length = string_length( value );

				if( !value_length )
					config_set_string( section, key, "" );
				else if( string_equal( value, "false" ) )
					config_set_bool( section, key, false );
				else if( string_equal( value, "true" ) )
					config_set_bool( section, key, true );
				else if( ( string_find( value, '.', 0 ) != STRING_NPOS ) && ( string_find_first_not_of( value, "0123456789.", 0 ) == STRING_NPOS ) && ( string_find( value, '.', string_find( value, '.', 0 ) + 1 ) == STRING_NPOS ) ) //Exactly one "."
					config_set_real( section, key, string_to_real( value ) );
				else if( string_find_first_not_of( value, "0123456789", 0 ) == STRING_NPOS )
					config_set_int( section, key, string_to_int64( value ) );
				else
				{
					if( ( value_length > 1 ) && ( value[0] == '"' ) && ( value[ value_length - 1 ] == '"' ) )
					{
						value[ value_length - 1 ] = 0;
						set_value = value + 1;
						config_set_string( section, key, set_value );
					}
					else
					{
						config_set_string( section, key, value );
					}
				}

				log_infof( HASH_CONFIG, "Config value from command line: %.*s:%.*s = %s", (int)section_length, section_str, (int)key_length, key_str, set_value );

				string_deallocate( value );
			}
		}
	}
}


void config_write( stream_t* stream, hash_t filter_section, const char* (*string_mapper)( hash_t ) )
{
	config_section_t* csection;
	config_key_t* bucket;
	size_t key, ib, bsize;

	stream_set_binary( stream, false );

	//TODO: If random access stream, update section if available, else append at end of stream
	//if( stream_is_sequential( stream ) )
	{
		stream_write_format( stream, "[%s]", string_mapper( filter_section ) );
		stream_write_endl( stream );

		csection = config_section( filter_section, false );
		if( csection ) for( key = 0; key < CONFIG_KEY_BUCKETS; ++key )
		{
			bucket = csection->key[ key ];
			if( bucket ) for( ib = 0, bsize = array_size( bucket ); ib < bsize; ++ib )
			{
				stream_write_format( stream, "\t%s\t\t\t\t= ", string_mapper( bucket[ib].name ) );
				switch( bucket[ib].type )
				{
					case CONFIGVALUE_BOOL:
						stream_write_bool( stream, bucket[ib].bval );
						break;

					case CONFIGVALUE_INT:
						stream_write_int64( stream, bucket[ib].ival );
						break;

					case CONFIGVALUE_REAL:
#if FOUNDATION_SIZE_REAL == 64
						stream_write_float64( stream, bucket[ib].rval );
#else
						stream_write_float32( stream, bucket[ib].rval );
#endif
						break;

					case CONFIGVALUE_STRING:
					case CONFIGVALUE_STRING_CONST:
					case CONFIGVALUE_STRING_VAR:
					case CONFIGVALUE_STRING_CONST_VAR:
						stream_write_string( stream, bucket[ib].sval );
						break;
				}
				stream_write_endl( stream );
			}
		}
	}
}

