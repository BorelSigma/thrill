################################################################################
# CTestConfig.cmake
#
# CTest configuration to submit results to my.cdash.org
#
# Part of Project Thrill - http://project-thrill.org
#
# Copyright (C) 2016 Timo Bingmann <tb@panthema.net>
#
# All rights reserved. Published under the BSD-2 license in the LICENSE file.
################################################################################

set(CTEST_PROJECT_NAME "Thrill")
set(CTEST_NIGHTLY_START_TIME "00:00:00 EST")

set(CTEST_DROP_METHOD "http")
set(CTEST_DROP_SITE "panthema.net")
set(CTEST_DROP_LOCATION "/cdash-12/submit.php?project=Thrill")
set(CTEST_DROP_SITE_CDASH TRUE)

################################################################################
