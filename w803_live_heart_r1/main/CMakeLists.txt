list(APPEND ADD_INCLUDE "../Config" "../Drivers" "../BSP" "../App" "../Algorithm")
list(APPEND ADD_SRCS "src/main.c" "../BSP/board_uart.c" "../Drivers/link_stream.c" "../Drivers/radar_iq.c" "../Drivers/rx_state.c"
                     "../App/snapshot_heart.c" "../Algorithm/heart_rate_core.c")
register_component()
target_compile_options(main PRIVATE -Wall -Wextra -Werror -ffp-contract=off -fno-fast-math)
target_compile_definitions(main PRIVATE HR_DEFAULT_FREQ_ROTATION=1)
