cmake_minimum_required(VERSION 2.8)

include($ENV{VITASDK}/common.cmake)

project(FireBaseCurl)

find_package(CURL)
find_package(OpenSSL)


add_executable(${PROJECT_NAME}
    $ENV{VITASDKINC}/common/debugScreen.c
    src/main.c
)

target_link_libraries(${PROJECT_NAME}
    SceLibKernel_stub
    SceDisplay_stub
    SceCtrl_stub
    SceSysmodule_stub
    SceNet_stub
    SceNetCtl_stub
    SceHttp_stub
    SceSsl_stub
    SceGxm_stub
    SceAppUtil_stub
    SceCommonDialog_stub
    ${CURL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    z
    m
)

vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME} UNSAFE)