#pragma once
#include <stddef.h>
#include <stdatomic.h>

#ifndef BUFSIZ
#define BUFSIZ 8192
#endif

// InputMode tells the UI how to collect input data
enum InputMode {
	InputMode_KEY, // Get one key of input at a time without buffering
	InputMode_TEXT // Get buffered text input
};
