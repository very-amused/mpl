#pragma once
#include "error.h"
#include <stdint.h>
#include <stdbool.h>

#define CONFIG_TYPE(VARIANT) \
	VARIANT(Config_I32) \
	VARIANT(Config_BOOL) \
	VARIANT(Config_STR)

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
	}
	return 0;
}
