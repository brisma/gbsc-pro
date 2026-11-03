// Golden-file comparison for host screen snapshots (test/golden/osd/<name>.txt, test/golden/oled/<name>.pbm)
#pragma once
#include <string>

// true when `actual` equals the stored golden. GOLDEN_UPDATE=1 rewrites it; a missing golden is written and
// reported as a failure so it gets reviewed; on a difference <name>.actual.txt is written beside it.
bool goldenCheck(const char* name, const std::string& actual);

// Same rules for test/golden/<subdir>/<name>.<ext>; goldenCheck(name, a) = goldenCheckFile("osd", name, "txt", a)
bool goldenCheckFile(const char* subdir, const char* name, const char* ext, const std::string& actual);
