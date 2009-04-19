#include <vd2/system/filesys.h>
#include "test.h"

DEFINE_TEST(Filesys) {
	// WILDCARD TESTS

	// Basic non-wildcard tests
	TEST_ASSERT(VDFileWildMatch(L"", L"random.bin") == false);
	TEST_ASSERT(VDFileWildMatch(L"random.bin", L"random.bin") == true);
	TEST_ASSERT(VDFileWildMatch(L"random.bin", L"randum.bin") == false);
	TEST_ASSERT(VDFileWildMatch(L"random.bin", L"randum.bi") == false);
	TEST_ASSERT(VDFileWildMatch(L"random.bin", L"randum.binx") == false);
	TEST_ASSERT(VDFileWildMatch(L"random.bin", L"RANDOM.BIN") == true);
	TEST_ASSERT(VDFileWildMatch(L"random.bin", L"xrandom.bin") == false);

	// ? tests
	TEST_ASSERT(VDFileWildMatch(L"random.b?n", L"random.bin") == true);
	TEST_ASSERT(VDFileWildMatch(L"random.bin?", L"random.bin") == false);
	TEST_ASSERT(VDFileWildMatch(L"?random.bin", L"random.bin") == false);

	// * tests
	TEST_ASSERT(VDFileWildMatch(L"*", L"random.bin") == true);
	TEST_ASSERT(VDFileWildMatch(L"random*", L"random.bin") == true);
	TEST_ASSERT(VDFileWildMatch(L"random.bin*", L"random.bin") == true);
	TEST_ASSERT(VDFileWildMatch(L"*random.bin", L"random.bin") == true);
	TEST_ASSERT(VDFileWildMatch(L"random*.bin", L"random.bin") == true);
	TEST_ASSERT(VDFileWildMatch(L"random*bin", L"random.bin") == true);
	TEST_ASSERT(VDFileWildMatch(L"random**bin", L"random.bin") == true);
	TEST_ASSERT(VDFileWildMatch(L"random*bin", L"random.bin.bin") == true);
	TEST_ASSERT(VDFileWildMatch(L"random*bin", L"random.ban.bin") == true);
	TEST_ASSERT(VDFileWildMatch(L"ran*?*bin", L"random.bin") == true);
	TEST_ASSERT(VDFileWildMatch(L"*.bin", L"random.bin") == true);
	TEST_ASSERT(VDFileWildMatch(L"*n*", L"random.bin") == true);
	TEST_ASSERT(VDFileWildMatch(L"*om*and*", L"random.bin") == false);

	return 0;
}

