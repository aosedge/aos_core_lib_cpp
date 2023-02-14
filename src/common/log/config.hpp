// SPDX-License-Identifier: Apache-2.0
//
// Copyright (C) 2023 Renesas Electronics Corporation.
// Copyright (C) 2023 EPAM Systems, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef LOG_CONFIG_HPP_
#define LOG_CONFIG_HPP_

/**
 * Log levels.
 *
 */
#define CONFIG_LOG_LEVEL_DISABLE 0
#define CONFIG_LOG_LEVEL_ERROR   1
#define CONFIG_LOG_LEVEL_WARNING 2
#define CONFIG_LOG_LEVEL_INFO    3
#define CONFIG_LOG_LEVEL_DEBUG   4

/**
 * Configures log level.
 *
 */
#ifndef CONFIG_LOG_LEVEL
#define CONFIG_LOG_LEVEL CONFIG_LOG_LEVEL_DEBUG
#endif

/**
 * Max log line size.
 *
 */
#ifndef CONFIG_LOG_LINE_SIZE
#define CONFIG_LOG_LINE_SIZE 120
#endif

#endif
