/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/scopeguard.h>
#include <gtest/gtest.h>
#include <libavb/libavb.h>
#include <ziparchive/zip_archive.h>

#include "apex_file.h"
#include "apex_preinstalled_data.h"

using android::base::Result;

static std::string testDataDir = android::base::GetExecutableDirectory() + "/";

namespace android {
namespace apex {
namespace {

TEST(ApexFileTest, GetOffsetOfSimplePackage) {
  const std::string filePath = testDataDir + "apex.apexd_test.apex";
  Result<ApexFile> apexFile = ApexFile::Open(filePath);
  ASSERT_TRUE(apexFile);

  int32_t zip_image_offset;
  size_t zip_image_size;
  {
    ZipArchiveHandle handle;
    int32_t rc = OpenArchive(filePath.c_str(), &handle);
    ASSERT_EQ(0, rc);
    auto close_guard =
        android::base::make_scope_guard([&handle]() { CloseArchive(handle); });

    ZipEntry entry;
    rc = FindEntry(handle, "apex_payload.img", &entry);
    ASSERT_EQ(0, rc);

    zip_image_offset = entry.offset;
    EXPECT_EQ(zip_image_offset % 4096, 0);
    zip_image_size = entry.uncompressed_length;
    EXPECT_EQ(zip_image_size, entry.compressed_length);
  }

  EXPECT_EQ(zip_image_offset, apexFile->GetImageOffset());
  EXPECT_EQ(zip_image_size, apexFile->GetImageSize());
}

TEST(ApexFileTest, GetOffsetMissingFile) {
  const std::string filePath = testDataDir + "missing.apex";
  Result<ApexFile> apexFile = ApexFile::Open(filePath);
  ASSERT_FALSE(apexFile);
  EXPECT_NE(std::string::npos,
            apexFile.error().message().find("Failed to open package"))
      << apexFile.error();
}

TEST(ApexFileTest, GetApexManifest) {
  const std::string filePath = testDataDir + "apex.apexd_test.apex";
  Result<ApexFile> apexFile = ApexFile::Open(filePath);
  ASSERT_TRUE(apexFile);
  EXPECT_EQ("com.android.apex.test_package", apexFile->GetManifest().name());
  EXPECT_EQ(1u, apexFile->GetManifest().version());
}

TEST(ApexFileTest, VerifyApexVerity) {
  const std::string filePath = testDataDir + "apex.apexd_test.apex";
  Result<ApexFile> apexFile = ApexFile::Open(filePath);
  ASSERT_TRUE(apexFile) << apexFile.error();

  auto verity_or = apexFile->VerifyApexVerity();
  ASSERT_TRUE(verity_or) << verity_or.error();

  const ApexVerityData& data = *verity_or;
  EXPECT_NE(nullptr, data.desc.get());
  EXPECT_EQ(std::string("368a22e64858647bc45498e92f749f85482ac468"
                        "50ca7ec8071f49dfa47a243c"),
            data.salt);
  EXPECT_EQ(std::string("705d8ec15be38fe416ed75045056434132758008"),
            data.root_digest);
}

// TODO: May consider packaging a debug key in debug builds (again).
TEST(ApexFileTest, DISABLED_VerifyApexVerityNoKeyDir) {
  const std::string filePath = testDataDir + "apex.apexd_test.apex";
  Result<ApexFile> apexFile = ApexFile::Open(filePath);
  ASSERT_TRUE(apexFile) << apexFile.error();

  auto verity_or = apexFile->VerifyApexVerity();
  ASSERT_FALSE(verity_or);
}

// TODO(jiyong): re-enable this test. This test is disabled because the build
// system now always bundles the public key that was used to sign the APEX.
// In debuggable build, the bundled public key is used as the last fallback.
// As a result, the verification is always successful (and thus test fails).
// In order to re-enable this test, we have to manually create an APEX
// where public key is not bundled.
TEST(ApexFileTest, DISABLED_VerifyApexVerityNoKeyInst) {
  const std::string filePath = testDataDir + "apex.apexd_test_no_inst_key.apex";
  Result<ApexFile> apexFile = ApexFile::Open(filePath);
  ASSERT_TRUE(apexFile) << apexFile.error();

  auto verity_or = apexFile->VerifyApexVerity();
  ASSERT_FALSE(verity_or);
}

TEST(ApexFileTest, GetBundledPublicKey) {
  const std::string filePath = testDataDir + "apex.apexd_test.apex";
  Result<ApexFile> apexFile = ApexFile::Open(filePath);
  ASSERT_TRUE(apexFile) << apexFile.error();

  const std::string keyPath =
      testDataDir + "apexd_testdata/com.android.apex.test_package.avbpubkey";
  std::string keyContent;
  ASSERT_TRUE(android::base::ReadFileToString(keyPath, &keyContent))
      << "Failed to read " << keyPath;

  EXPECT_EQ(keyContent, apexFile->GetBundledPublicKey());
}

}  // namespace
}  // namespace apex
}  // namespace android

int main(int argc, char** argv) {
  android::base::InitLogging(argv, &android::base::StderrLogger);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
