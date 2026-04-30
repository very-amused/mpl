extern "C" {
#include "parser.h"
}

ParseNode::~ParseNode() {
	ParseNode_rfree(this);
}
