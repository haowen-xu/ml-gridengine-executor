//
// Created by 许昊文 on 2018/11/20.
//

#include <catch2/catch.hpp>
#include "src/Utils.h"
#include "macros.h"


TEST_CASE("Sizes are formatted to human readable texts", "[Utils]") {
  // < 1K
  REQUIRE_EQUALS("0B", Utils::formatSize(0));
  REQUIRE_EQUALS("1B", Utils::formatSize(1));
  REQUIRE_EQUALS("999B", Utils::formatSize(999));
  REQUIRE_EQUALS("0.98K", Utils::formatSize(1000));
  REQUIRE_EQUALS("0.99K", Utils::formatSize(1018));

  // >= 1K , < 1M
  REQUIRE_EQUALS("1K", Utils::formatSize(1019));
  REQUIRE_EQUALS("1K", Utils::formatSize(1024));
  REQUIRE_EQUALS("1.01K", Utils::formatSize(1035));
  REQUIRE_EQUALS("1.1K", Utils::formatSize(1127));
  REQUIRE_EQUALS("2K", Utils::formatSize(2048));
  REQUIRE_EQUALS("999K", Utils::formatSize(999 * 1024));
  REQUIRE_EQUALS("0.98M", Utils::formatSize(1000 * 1024));

  // >= 1M , < 1G
  REQUIRE_EQUALS("1M", Utils::formatSize(1024 * 1024));
  REQUIRE_EQUALS("1.99M", Utils::formatSize(2038 * 1024));
  REQUIRE_EQUALS("2M", Utils::formatSize(2048 * 1024));
  REQUIRE_EQUALS("999M", Utils::formatSize(999L * 1024L * 1024L));
  REQUIRE_EQUALS("0.98G", Utils::formatSize(1000L * 1024L * 1024L));

  // >= 1G
  REQUIRE_EQUALS("1G", Utils::formatSize(1024L * 1024L * 1024L));
  REQUIRE_EQUALS("500G", Utils::formatSize(500L * 1024L * 1024L * 1024L));
}
