# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

include_guard()

set(COMPILE_WITH_JEMALLOC ON)

if (DISABLE_JEMALLOC)
  set(COMPILE_WITH_JEMALLOC OFF)
endif()

include(cmake/utils.cmake)

FetchContent_DeclareGitHubWithMirror(rocksdb
  facebook/rocksdb v9.2.1
  MD5=b03db4c602754a2c50aa2967fd07a2a7
)

FetchContent_GetProperties(jemalloc)
FetchContent_GetProperties(snappy)
FetchContent_GetProperties(tbb)

FetchContent_MakeAvailableWithArgs(rocksdb
  CMAKE_MODULE_PATH=${PROJECT_SOURCE_DIR}/cmake/modules # to locate FindJeMalloc.cmake
  Snappy_DIR=${PROJECT_SOURCE_DIR}/cmake/modules # to locate SnappyConfig.cmake
  FAIL_ON_WARNINGS=OFF
  WITH_TESTS=OFF
  WITH_BENCHMARK_TOOLS=OFF
  WITH_CORE_TOOLS=OFF
  WITH_TOOLS=OFF
  WITH_SNAPPY=ON
  WITH_LZ4=ON
  WITH_ZLIB=ON
  WITH_ZSTD=ON
  WITH_GFLAGS=OFF
  WITH_TBB=ON
  USE_RTTI=ON
  ROCKSDB_BUILD_SHARED=OFF
  ROCKSDB_MALLOC_USABLE_SIZE=OFF
  WITH_JEMALLOC=${COMPILE_WITH_JEMALLOC}
  PORTABLE=${PORTABLE}
)

add_library(rocksdb_with_headers INTERFACE)
target_include_directories(rocksdb_with_headers INTERFACE ${rocksdb_SOURCE_DIR}/include)
target_link_libraries(rocksdb_with_headers INTERFACE rocksdb)
