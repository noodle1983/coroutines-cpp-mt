//
// Created by Konstantin Gredeskoul on 5/16/17.
//
#include "task.h"
#include "gtest/gtest.h"

#include "processor_types.h"

using namespace std;


#define VI vector<long long>

class CoroutinesCppMtTest : public ::testing::Test {

protected:
  //VI numerators   = {5, 9, 17, 933345453464353416L};
  //VI denominators = {2, 3, 19, 978737423423423499L};
  //VI divisions    = {2, 3, 0, 0};
  //VI remainders   = {1, 0, 17, 933345453464353416};

  virtual void SetUp() {
  };

  virtual void TearDown() {
  };

  virtual void verify(int index) {
    //Fraction       f        = Fraction{numerators.at(index), denominators.at(index)};
    //DivisionResult expected = DivisionResult{divisions.at(index), remainders.at(index)};
    //DivisionResult result   = Division(f).divide();
    //EXPECT_EQ(result.division, expected.division);
    //EXPECT_EQ(result.remainder, expected.remainder);
  }
};

TEST_F(CoroutinesCppMtTest, Wait_Task_On_Main_Thread) {
	nd::Worker::markMainThread();
    auto helloTask = []()->nd::Task{
		cout << "hello in thread id:0x" << std::hex << this_thread::get_id() << endl;
        co_return;
	}();
    helloTask.runOnProcessor(-1, 0);

    cout << "main1 in thread id:0x" << std::hex << this_thread::get_id() << endl;
    helloTask.wait();
    cout << "main2 in thread id:0x" << std::hex << this_thread::get_id() << endl;
}

TEST_F(CoroutinesCppMtTest, 9_DivideBy_3) {
  verify(1);
}

TEST_F(CoroutinesCppMtTest, 17_DivideBy_19) {
  verify(2);
}

TEST_F(CoroutinesCppMtTest, Long_DivideBy_Long) {
  verify(3);
}

TEST_F(CoroutinesCppMtTest, DivisionByZero) {
  //try {
  //  FAIL() << "Expected divide() method to throw DivisionByZeroException";
  //} catch (DivisionByZero const &err) {
  //  EXPECT_EQ(err.what(), DIVISION_BY_ZERO_MESSAGE);
  //}
  //catch (...) {
  //  FAIL() << "Expected DivisionByZeroException!";
  //}
}

