TEMPLATE = subdirs

SUBDIRS += \
    backend \
    benchmarks \
    frontend \
    integration \

requires(qtHaveModule(testlib))
