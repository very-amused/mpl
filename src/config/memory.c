#include "memory.h"

void ConfigRegister_init(ConfigRegister *r) {
	r->val.type = Config_VOID;
	r->is_owned = false;
}

void ConfigRegister_clear(ConfigRegister *r) {
	if (r->is_owned) {
		free(r->val.val_ptr);
	}
	r->val.type = Config_VOID;
	r->is_owned = false;
}

void ConfigRegister_store(ConfigRegister *r, ConfigVal val, bool is_owned) {
	r->val = val;
	r->is_owned = is_owned;
}

void Memory_init(Memory *mem) {
	// Initialize registers
	ConfigRegister_init(&mem->ret);
}

void Memory_deinit(Memory *mem) {
	// Clear registers
	ConfigRegister_clear(&mem->ret);
}
