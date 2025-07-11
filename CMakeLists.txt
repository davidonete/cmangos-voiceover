#
# Copyright (C) 2005-2018 MaNGOS project <http://getmangos.com/>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#

set(LIBRARY_NAME voiceover)

#Base files
file(GLOB voiceover_source ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)
    source_group("voiceover" FILES ${voiceover_source})

#Define base source library
set(LIBRARY_SRCS
  ${voiceover_source}
)

include_directories(${CMAKE_SOURCE_DIR}/src/modules/modules/src)
add_definitions(-DENABLE_MODULES)
add_definitions(-DENABLE_VOICEOVER)

# Define Expansion
if ( ${CMAKE_PROJECT_NAME} MATCHES "Classic")
  add_definitions(-DEXPANSION=0)
endif()
if ( ${CMAKE_PROJECT_NAME} MATCHES "TBC")
  add_definitions(-DEXPANSION=1)
endif()
if ( ${CMAKE_PROJECT_NAME} MATCHES "WoTLK")
  add_definitions(-DEXPANSION=2)
endif()

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SRCS})

target_link_libraries(${LIBRARY_NAME}
  modules
  shared
  RecastNavigation::Detour
)

if(UNIX)
  # Both systems don't have libdl and don't need them
  if (NOT (CMAKE_SYSTEM_NAME STREQUAL "FreeBSD" OR CMAKE_SYSTEM_NAME STREQUAL "NetBSD"))
    target_link_libraries(${LIBRARY_NAME} dl)
  endif()
endif()

set_target_properties(${LIBRARY_NAME} PROPERTIES PROJECT_LABEL "VoiceOver")
set_target_properties(${LIBRARY_NAME} PROPERTIES FOLDER "Modules")

# Install config files
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/voiceover.conf.dist.in ${CMAKE_CURRENT_BINARY_DIR}/voiceover.conf.dist)
if (NOT CONF_INSTALL_DIR)
  set(CONF_INSTALL_DIR ${CONF_DIR})
endif()
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/voiceover.conf.dist DESTINATION ${CONF_INSTALL_DIR})