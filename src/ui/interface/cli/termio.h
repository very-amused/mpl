#pragma once
#include "ui/interface/cli/termio_thread.h"

typedef struct TermIO TermIO;

TermIO *TermIO_new(EventQueue *eq, KeybindMap *keybinds);

void TermIO_set_input_mode(TermIO *io, enum InputMode mode);
