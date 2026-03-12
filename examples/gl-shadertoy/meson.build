# Copyright 2024 Joel Winarske
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# gl-shadertoy only makes sense when EGL is enabled (guaranteed by caller).

executable(
  'gl-shadertoy',
  'shadertoy.cc',
  include_directories: include_directories('../../third_party/cxxopts-3.2.1/include'),
  dependencies: [waypp_dep, wayland_gen_dep, cxxopts_dep, threads_dep] + egl_deps,
  install: true,
)
