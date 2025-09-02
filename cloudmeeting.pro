TEMPLATE = subdirs
CONFIG  += ordered

SUBDIRS += \
    commonlib \
    sdk \
    server \
    client

# Build order: libs first
sdk.depends = commonlib
server.depends = commonlib
client.depends = commonlib sdk
