always_built_SUBDIRS = \
	common			\
	accessibility	\
	appearance		\
	default-applications	\
	keybindings		\
	keyboard		\
	mouse			\
	network			\
	windows

SUBDIRS = $(always_built_SUBDIRS)

if HAVE_RANDR
SUBDIRS += display
endif

if BUILD_ABOUTME
SUBDIRS += about-me
endif

if BUILD_SOUND
SUBDIRS += sound
endif

DIST_SUBDIRS = \
	$(always_built_SUBDIRS) display about-me sound
