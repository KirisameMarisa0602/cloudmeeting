# sdk_link.pri - Link client against SDK and commonlib
INCLUDEPATH += $$PWD/../sdk/include $$PWD/../commonlib/include

# Static library dependencies with absolute paths for testing
LIBS += /home/runner/work/cloudmeeting/cloudmeeting/sdk/libsdk.a /home/runner/work/cloudmeeting/cloudmeeting/commonlib/libcommonlib.a