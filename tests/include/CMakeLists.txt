#
# Copyright (C) 2024 EPAM Systems, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

set(TARGET aostestcore)

# ######################################################################################################################
# Target
# ######################################################################################################################

add_library(${TARGET} INTERFACE)
target_include_directories(${TARGET} INTERFACE .)
