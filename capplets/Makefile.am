always_built_SUBDIRS =					\
	common accessibility				\
	default-applications desktop-links font		\
	background keyboard mouse sound			\
	file-types theme-switcher ui-properties		\
	keybindings $(LOCALIZATION_CAPPLET) network windows

if HAVE_RANDR
randr_SUBDIRS = display
else
randr_SUBDIRS = 
endif

SUBDIRS =  $(always_built_SUBDIRS) $(randr_SUBDIRS)

DIST_SUBDIRS = \
	$(always_built_SUBDIRS) display



