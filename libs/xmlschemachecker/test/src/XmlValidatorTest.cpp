/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gtest/gtest.h"
#include "XmlValidator.h"

#include <list>
#include <string>

using namespace std;

const string testDataFolder = string(TEST_FOLDER) + "data";

class XmlValidatorTests : public ::testing::Test {
protected:
    virtual void SetUp() {
      // Initialize XMLValidator object
      validator = new XmlValidator();
    }

    virtual void TearDown() {
      // Clean up XMLValidator object
      delete validator;
    }

    XmlValidator* validator;
};

// Test case for valid XML file
TEST_F(XmlValidatorTests, validate_pdsc) {
  string schemaFile = testDataFolder + "/PACK.xsd";
  string pdscFile = testDataFolder + "/valid.pdsc";

  // Validate XML file against schema
  EXPECT_TRUE(validator->validate(schemaFile, pdscFile));
}

// Test case for valid XML file
TEST_F(XmlValidatorTests, invalidate_pdsc) {
  string schemaFile = testDataFolder + "/PACK.xsd";
  string pdscFile = testDataFolder + "/invalid.pdsc";

  // Validate XML file against schema
  EXPECT_FALSE(validator->validate(schemaFile, pdscFile));
}
