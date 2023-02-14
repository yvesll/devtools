/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gtest/gtest.h"
#include "XmlChecker.h"

#include <list>
#include <string>

using namespace std;

const string testDataFolder = string(TEST_FOLDER) + "data";

class XmlValidatorTests : public ::testing::Test {
protected:
    virtual void SetUp() {
      // Initialize XMLValidator object
      checker = new XmlChecker();
    }

    virtual void TearDown() {
      // Clean up XMLValidator object
      delete checker;
    }

    XmlChecker* checker;
};

// Test case for valid XML file
TEST_F(XmlValidatorTests, validate_pdsc) {
  string packXsd = string(PACKXSD_FOLDER) + "/PACK.xsd";
  string pdscFile = testDataFolder + "/valid.pdsc";

  // Validate XML file against schema
  EXPECT_TRUE(checker->Validate(pdscFile, packXsd));
}

// Test case for valid XML file
TEST_F(XmlValidatorTests, invalidate_pdsc) {
  string packXsd = string(PACKXSD_FOLDER) + "/PACK.xsd";
  string pdscFile = testDataFolder + "/invalid.pdsc";

  // Validate XML file against schema
  EXPECT_FALSE(checker->Validate(pdscFile, packXsd));
}
