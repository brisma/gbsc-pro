#include "input-types.h"

static const char* const kLong[7] = {"-", "RGB SCART", "RGB SoG", "VGA", "Component", "S-Video", "Composite"};
static const char* const kShort[7] = {"-", "RGB", "SoG", "VGA", "YPbPr", "S-Vid", "CVBS"};

const char* inputLongName(uint8_t t) { return kLong[inputTypeValid(t) ? t : 0]; }
const char* inputShortName(uint8_t t) { return kShort[inputTypeValid(t) ? t : 0]; }
