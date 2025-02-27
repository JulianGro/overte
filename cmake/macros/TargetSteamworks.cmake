#
#  Copyright 2015 High Fidelity, Inc.
#  Created by Clement Brisset on 6/8/2016
#
#  Distributed under the Apache License, Version 2.0.
#  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
#
macro(TARGET_STEAMWORKS)
    find_package(Steamworks REQUIRED)
    target_link_libraries(${TARGET_NAME} Steam::Works)
endmacro()
