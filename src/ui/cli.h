#pragma once


// MPL's CLI user interface. This is the default UI used
typedef struct MPL_CLI MPL_CLI;

MPL_CLI *MPL_CLI_new();
void MPL_CLI_free(MPL_CLI *cli);

