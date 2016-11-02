
#include "scripting/ScriptingTestFixture.h"

class BitOpsTest : public test::scripting::ScriptingTestFixture {
 public:
	BitOpsTest() : test::scripting::ScriptingTestFixture(0) {
		pushModDir("bitops");
	}
};

TEST_F(BitOpsTest, AND) {
	this->EvalTestScript();
}

TEST_F(BitOpsTest, OR) {
	this->EvalTestScript();
}

TEST_F(BitOpsTest, XOR) {
	this->EvalTestScript();
}

TEST_F(BitOpsTest, toggleBit) {
	this->EvalTestScript();
}

TEST_F(BitOpsTest, checkBit) {
	this->EvalTestScript();
}

TEST_F(BitOpsTest, addBit) {
	this->EvalTestScript();
}