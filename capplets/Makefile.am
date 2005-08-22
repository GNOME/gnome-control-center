always_built_SUBDIRS =					\
	common accessibility				\
	default-applications font			\
	background keyboard mouse sound			\
	theme-switcher ui-properties			\
	keybindings network windows

SUBDIRS = $(always_built_SUBDIRS)

if HAVE_RANDR
SUBDIRS += display
endif

if BUILD_ABOUTME
SUBDIRS += about-me
endif

DIST_SUBDIRS = \
	$(always_built_SUBDIRS) display about-me
