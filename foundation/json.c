/* json.c  -  Foundation library  -  Public Domain  -  2013 Mattias Jansson / Rampant Pixels
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

static json_token_t*
get_token(json_token_t* tokens, size_t capacity, unsigned int index) {
	return index < capacity ? tokens + index : nullptr;
}

static bool
is_valid_token(json_token_t* tokens, size_t capacity, unsigned int index) {
	json_token_t* token = get_token(tokens, capacity, index);
	return token ? (token->type != JSON_UNDEFINED) : true;
}

static void
set_token_primitive(json_token_t* tokens, size_t capacity, unsigned int current, json_type_t type,
                    size_t value, size_t value_length) {
	json_token_t* token = get_token(tokens, capacity, current);
	if (token) {
		token->type = type;
		token->child = 0;
		token->sibling = 0;
		token->value = (unsigned int)value;
		token->value_length = (unsigned int)value_length;
	}
}

static void
set_token_complex(json_token_t* tokens, size_t capacity, unsigned int current, json_type_t type) {
	json_token_t* token = get_token(tokens, capacity, current);
	if (token) {
		token->type = type;
		token->child = current + 1;
		token->sibling = 0;
		token->value = 0;
		token->value_length = 0;
	}
}

static void
set_token_id(json_token_t* tokens, size_t capacity, unsigned int current,
             size_t id, size_t id_length) {
	json_token_t* token = get_token(tokens, capacity, current);
	if (token) {
		token->id = (unsigned int)id;
		token->id_length = (unsigned int)id_length;
	}
}

static bool
is_whitespace(char c) {
	return (c == ' ')  || (c == '\t') || (c == '\n') || (c == '\r');
}

static bool
is_token_delimiter(char c) {
	return is_whitespace(c) || (c == ']')  || (c == '}') || (c == ',');
}

static size_t
skip_whitespace(const char* buffer, size_t length, size_t pos) {
	while (pos < length) {
		if (!is_whitespace(buffer[pos]))
			return pos;
		++pos;
	}
	return pos;
}

static size_t
parse_string(const char* buffer, size_t length, size_t pos, bool simple) {
	size_t start = pos;
	size_t esc;
	while (pos < length) {
		char c = buffer[pos];
		if (simple && (is_token_delimiter(c) || (c == '=') || (c == ':')))
			return pos - start;
		if (c == '"')
			return pos - start;
		++pos;
		if (c == '\\' && (pos < length)) {
			switch (buffer[pos]) {
			// Escaped symbols \X
			case '\"': case '/': case '\\': case 'b':
			case 'f' : case 'r': case 'n' : case 't':
				break;
			// Escaped symbol \uXXXX
			case 'u':
				for (esc = 0; esc < 4 && pos < length; ++esc) {
					++pos;
					if (!((buffer[pos] >= 48 && buffer[pos] <= 57) || // 0-9
					        (buffer[pos] >= 65 && buffer[pos] <= 70) || // A-F
					        (buffer[pos] >= 97 && buffer[pos] <= 102))) // a-f
						return STRING_NPOS;
				}
				break;
			default:
				return STRING_NPOS;
			}
		}
	}
	return simple ? pos - start : STRING_NPOS;
}

static size_t
parse_number(const char* buffer, size_t length, size_t pos) {
	size_t start = pos;
	bool has_dot = false;
	bool has_digit = false;
	bool has_exp = false;
	while (pos < length) {
		char c = buffer[pos];
		if (is_token_delimiter(c))
			break;
		else if ((c == '-') && !(start == pos))
			return STRING_NPOS;
		else if (c == '.') {
			if (has_dot || has_exp)
				return STRING_NPOS;
			has_dot = true;
		}
		else if ((c == 'e') || (c == 'E')) {
			if (!has_digit || has_exp)
				return STRING_NPOS;
			has_exp = true;
			if ((pos + 1) < length) {
				if ((buffer[pos+1] == '+') ||
				        (buffer[pos+1] == '-'))
					++pos;
			}
		}
		else if ((c < '0') || (c > '9'))
			return STRING_NPOS;
		else
			has_digit = true;
		++pos;
	}
	return pos - start;
}

static size_t
parse_object(const char* buffer, size_t length, size_t pos,
             json_token_t* tokens, size_t capacity, unsigned int* current, bool simple);

static size_t
parse_value(const char* buffer, size_t length, size_t pos,
            json_token_t* tokens, size_t capacity, unsigned int* current, bool simple);

static size_t
parse_array(const char* buffer, size_t length, size_t pos,
            json_token_t* tokens, size_t capacity, unsigned int* current, bool simple);

static size_t
parse_object(const char* buffer, size_t length, size_t pos,
             json_token_t* tokens, size_t capacity, unsigned int* current, bool simple) {
	json_token_t* token;
	size_t string;
	unsigned int last = 0;

	pos = skip_whitespace(buffer, length, pos);
	while (pos < length) {
		char c = buffer[pos++];

		switch (c) {
		case '}':
			if (last && !is_valid_token(tokens, capacity, last))
				return STRING_NPOS;
			return pos;

		case ',':
			if (!last || !is_valid_token(tokens, capacity, last))
				return STRING_NPOS;
			if ((token = get_token(tokens, capacity, last)))
				token->sibling = *current;
			last = 0;
			pos = skip_whitespace(buffer, length, pos);
			break;

		case '"':
		default:
			if (last)
				return STRING_NPOS;
			if (c != '"') {
				if (!simple)
					return STRING_NPOS;
				--pos;
			}

			string = parse_string(buffer, length, pos, simple);
			if (string == STRING_NPOS)
				return STRING_NPOS;

			last = *current;
			set_token_id(tokens, capacity, *current, pos, string);
			//Skip terminating '"' (optional for simplified)
			if (!simple || ((pos + string < length) && (buffer[pos + string] == '"')))
				++string;
			pos += string;

			pos = skip_whitespace(buffer, length, pos);
			if ((buffer[pos] != ':') &&
			        (!simple || (buffer[pos] != '=')))
				return STRING_NPOS;
			pos = parse_value(buffer, length, pos + 1, tokens, capacity, current, simple);
			pos = skip_whitespace(buffer, length, pos);
			if (simple && ((pos < length) && (buffer[pos] != ',') && (buffer[pos] != '}'))) {
				if ((token = get_token(tokens, capacity, last)))
					token->sibling = *current;
				last = 0;
			}
			break;
		}
	}

	return simple ? pos : STRING_NPOS;
}

static size_t
parse_array(const char* buffer, size_t length, size_t pos,
            json_token_t* tokens, size_t capacity, unsigned int* current, bool simple) {
	json_token_t* token;
	unsigned int now;
	unsigned int last = 0;

	pos = skip_whitespace(buffer, length, pos);
	if (buffer[pos] == ']')
		return skip_whitespace(buffer, length, ++pos);

	while (pos < length) {
		now = *current;
		set_token_id(tokens, capacity, now, 0, 0);
		pos = parse_value(buffer, length, pos, tokens, capacity, current, simple);
		if (pos == STRING_NPOS)
			return STRING_NPOS;
		if (last && (token = get_token(tokens, capacity, last)))
			token->sibling = now;
		last = now;
		pos = skip_whitespace(buffer, length, pos);
		if (buffer[pos] == ',')
			++pos;
		else if (buffer[pos] == ']')
			return ++pos;
		else if (!simple)
			return STRING_NPOS;
	}

	return STRING_NPOS;
}

static size_t
parse_value(const char* buffer, size_t length, size_t pos,
            json_token_t* tokens, size_t capacity, unsigned int* current, bool simple) {
	size_t string;

	pos = skip_whitespace(buffer, length, pos);
	while (pos < length) {
		char c = buffer[pos++];
		switch (c) {
		case '{':
			set_token_complex(tokens, capacity, *current, JSON_OBJECT);
			++(*current);
			pos = parse_object(buffer, length, pos, tokens, capacity, current, simple);
			return pos;

		case '[':
			set_token_complex(tokens, capacity, *current, JSON_ARRAY);
			++(*current);
			pos = parse_array(buffer, length, pos, tokens, capacity, current, simple);
			return pos;

		case '-': case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9': case '.':
			string = parse_number(buffer, length, pos - 1);
			if (string == STRING_NPOS)
				return STRING_NPOS;
			set_token_primitive(tokens, capacity, *current, JSON_PRIMITIVE, pos - 1, string);
			++(*current);
			return pos + string - 1;

		case 't':
		case 'f':
			if ((c == 't') && (length - pos >= 4) && string_equal(buffer + pos, 3, STRING_CONST("rue")) &&
			        is_token_delimiter(buffer[pos+3])) {
				set_token_primitive(tokens, capacity, *current, JSON_PRIMITIVE, pos - 1, 4);
				++(*current);
				return pos + 3;
			}
			if ((c == 'f') && (length - pos >= 5) && string_equal(buffer + pos, 4, STRING_CONST("alse")) &&
			        is_token_delimiter(buffer[pos+4])) {
				set_token_primitive(tokens, capacity, *current, JSON_PRIMITIVE, pos - 1, 5);
				++(*current);
				return pos + 4;
			}
			if (!simple)
				return STRING_NPOS;
		//Fall through to string handling

		case '"':
		default:
			if (c != '"') {
				if (!simple)
					return STRING_NPOS;
				--pos;
			}
			string = parse_string(buffer, length, pos, simple);
			if (string == STRING_NPOS)
				return STRING_NPOS;
			set_token_primitive(tokens, capacity, *current, JSON_STRING, pos, string);
			++(*current);
			//Skip terminating '"' (optional for simplified)
			if (!simple || ((pos + string < length) && (buffer[pos + string] == '"')))
				++string;
			return pos + string;
		}
	}

	return STRING_NPOS;
}

size_t
json_parse(const char* buffer, size_t size, json_token_t* tokens, size_t capacity) {
	unsigned int current = 0;
	set_token_id(tokens, capacity, current, 0, 0);
	set_token_primitive(tokens, capacity, current, JSON_UNDEFINED, 0, 0);
	if (parse_value(buffer, size, 0, tokens, capacity, &current, false) == STRING_NPOS)
		return 0;
	return current;
}

size_t
sjson_parse(const char* buffer, size_t size, json_token_t* tokens, size_t capacity) {
	unsigned int current = 0;
	size_t pos = skip_whitespace(buffer, size, 0);
	if ((pos < size) && (buffer[pos] != '{')) {
		set_token_id(tokens, capacity, current, 0, 0);
		set_token_complex(tokens, capacity, current, JSON_OBJECT);
		++current;
		if (parse_object(buffer, size, pos, tokens, capacity, &current, true) == STRING_NPOS)
			return 0;
		return current;
	}
	if (parse_value(buffer, size, pos, tokens, capacity, &current, true) == STRING_NPOS)
		return 0;
	return current;
}