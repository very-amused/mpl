#pragma once


// Parse and evaluate a 'direct-eval' MPL function (one that directly manipulates config state during parsing)
// Returns 0 on success, nonzero on error
int parse_direct_eval(const char *line);
