always_built_SUBDIRS =					\
	common accessibility				\
	default-applications font			\
	background keyboard mouse sound			\
	theme-switcher ui-properties			\
	keybindings network windows

if HAVE_RANDR
randr_SUBDIRS = display
else
randr_SUBDIRS = 
endif

if BUILD_ABOUTME
aboutme_SUBDIRS = about-me
else
aboutme_SUBDIRS =
endif

SUBDIRS =  $(always_built_SUBDIRS) $(randr_SUBDIRS) $(aboutme_SUBDIRS)

DIST_SUBDIRS = \
	$(always_built_SUBDIRS) display $(aboutme_SUBDIRS)
