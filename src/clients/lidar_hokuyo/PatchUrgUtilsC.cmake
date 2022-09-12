message("Patching urg_utils.c to prevent implicit function declaration")

set(FILE_TO_PATCH ${CMAKE_CURRENT_BINARY_DIR}/current/src/urg_utils.c)

set(PREPEND_CONTENTS "
    #include \"urg_detect_os.h\"
    #if !defined(URG_WINDOWS_OS)
    #include <unistd.h>
    #endif
")

file(READ ${FILE_TO_PATCH} FILE_CONTENTS)
file(WRITE ${FILE_TO_PATCH} "${PREPEND_CONTENTS} ${FILE_CONTENTS}")
