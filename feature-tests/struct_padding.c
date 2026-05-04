#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

// Test that struct padding behavior is dependable for config functions
struct testStruct {
	// Test Config_I32 alignment
	char c;
	int32_t i32;

	// Test Config_BOOL alignment
	char c2;
	bool b;

	// Test Config_STR alignment
	char c3;
	char *str;

	// Test end padding
	char c4;
};

int main() {
	static const bool is_64bit = sizeof(void *) == 8;

	assert(sizeof(int32_t) == 4);
	assert(sizeof(bool) == 1);
	assert(sizeof(char *) == (is_64bit ? 8 : 4));

	assert(alignof(int32_t) == sizeof(int32_t));
	assert(alignof(bool) == sizeof(bool));
	assert(alignof(char *) == sizeof(char *));

	assert(offsetof(struct testStruct, i32) == 4);
	assert(offsetof(struct testStruct, b) == 9);
	assert(offsetof(struct testStruct, str) == (is_64bit ? 16 : 12));

	assert(sizeof(struct testStruct) == (is_64bit ? 32 : 20));

	return 0;
}
