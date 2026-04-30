#pragma once
#include "error.h"

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
