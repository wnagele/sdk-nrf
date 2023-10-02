#
# Copyright (c) 2021 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

target_include_directories(app PRIVATE .)
target_sources_ifdef(CONFIG_CLOUD_CODEC_NRF_CLOUD app
                     PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/nrf_cloud/nrf_cloud_codec.c)

target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cloud_codec_ringbuffer.c)

# Include JSON convenience APIs if used by the respective cloud codec backend.
if (CONFIG_CLOUD_CODEC_NRF_CLOUD)
        target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/json_helpers.c)
endif()
