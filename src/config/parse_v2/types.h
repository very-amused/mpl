#pragma once
#include "error.h"
#include <stdint.h>
#include <stdbool.h>

#define CONFIG_TYPE(VARIANT) \
	VARIANT(Config_I32) \
	VARIANT(Config_BOOL) \
	VARIANT(Config_STR) \
	VARIANT(Config_TRACK_QUEUE)

enum ConfigType {
	CONFIG_TYPE(ENUM_VAL)
};

static const enum ConfigType Config_VOID = (enum ConfigType)-1;

static const inline char *ConfigType_name(enum ConfigType t) {
	switch (t) {
		CONFIG_TYPE(ENUM_KEY)
	}
	return DEFAULT_ERR_NAME;
}

// Get a ConfigType's name w/o 'Config_' prefix
static const inline char *ConfigType_pretty_name(enum ConfigType t) {
	const char *name = ConfigType_name(t);
	static const size_t offset = sizeof("Config_")-1;
	return &name[offset];
}

#undef CONFIG_TYPE

// Get the byte size of a ConfigType
static const inline size_t ConfigType_size(enum ConfigType t) {
	switch (t) {
		case Config_I32:
			return sizeof(int32_t);
		case Config_BOOL:
			return sizeof(bool);
		case Config_STR:
			return sizeof(char *);
		case Config_TRACK_QUEUE:
			return sizeof(void *);
	}
	return 0;
}

// A value holding data of some ConfigType
typedef struct ConfigVal {
	enum ConfigType type;
	union {
		int32_t val_i32;
		bool val_bool;
		char *val_str;
		void *val_ptr;
	};
} ConfigVal;
