INCLUDES = \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(GNOME_INCLUDEDIR) \
	$(XML_CFLAGS) \
	-I$(top_srcdir)/

Screensaversdir = $(datadir)/screensavers
Screensavers_DATA = \
	3d_clock.xml \
	ant.xml \
	atlantis.xml \
	attraction.xml \
	blitspin.xml \
	bouboule.xml \
	braid.xml \
	bsod.xml \
	bubble3d.xml \
	bubbles.xml \
	cage.xml \
	compass.xml \
	coral.xml \
	critical.xml \
	crystal.xml \
	cynosure.xml \
	decayscreen.xml \
	deco.xml \
	deluxe.xml \
	demon.xml \
	discrete.xml \
	distort.xml \
	drift.xml \
	epicycle.xml \
	fadeplot.xml \
	flag.xml \
	flame.xml \
	flow.xml \
	forest.xml \
	glplanet.xml \
	galaxy.xml \
	gears.xml \
	goop.xml \
	grav.xml \
	greynetic.xml \
	halo.xml \
	helix.xml \
	hopalong.xml \
	hypercube.xml \
	ifs.xml \
	imsmap.xml \
	interference.xml \
	jigsaw.xml \
	julia.xml \
	kaleidescope.xml \
	kumppa.xml \
	lament.xml \
	laser.xml \
	lightning.xml \
	lisa.xml \
	lissie.xml \
	lmorph.xml \
	loop.xml \
	maze.xml \
	moebius.xml \
	moire.xml \
	moire2.xml \
	morph3d.xml \
	mountain.xml \
	munch.xml \
	noseguy.xml \
	pedal.xml \
	penetrate.xml \
	penrose.xml \
	pipes.xml \
	pyro.xml \
	qix.xml \
	rd-bomb.xml \
	rocks.xml \
	rorschach.xml \
	rotor.xml \
	rubiks_cube.xml \
	sierpinski.xml \
	slidescreen.xml \
	slip.xml \
	sonar.xml \
	sphere.xml \
	spiral.xml \
	spotlight.xml \
	sproingies.xml \
	squiral.xml \
	stairs.xml \
	starfish.xml \
	strange.xml \
	superquadrics.xml \
	swirl.xml \
	triangle.xml \
	truchet.xml \
	vines.xml \
	worm.xml \
	xflame.xml \
	xjack.xml \
	xlyap.xml \
	xmatrix.xml \
	xroger.xml

screensavers_msgs = \
	3d_clock.xml.h \
	ant.xml.h \
	atlantis.xml.h \
	attraction.xml.h \
	blitspin.xml.h \
	bouboule.xml.h \
	braid.xml.h \
	bsod.xml.h \
	bubble3d.xml.h \
	bubbles.xml.h \
	cage.xml.h \
	compass.xml.h \
	coral.xml.h \
	critical.xml.h \
	crystal.xml.h \
	cynosure.xml.h \
	decayscreen.xml.h \
	deco.xml.h \
	deluxe.xml.h \
	demon.xml.h \
	discrete.xml.h \
	distort.xml.h \
	drift.xml.h \
	epicycle.xml.h \
	fadeplot.xml.h \
	flag.xml.h \
	flame.xml.h \
	flow.xml.h \
	forest.xml.h \
	glplanet.xml.h \
	galaxy.xml.h \
	gears.xml.h \
	goop.xml.h \
	grav.xml.h \
	greynetic.xml.h \
	halo.xml.h \
	helix.xml.h \
	hopalong.xml.h \
	hypercube.xml.h \
	ifs.xml.h \
	imsmap.xml.h \
	interference.xml.h \
	jigsaw.xml.h \
	julia.xml.h \
	kaleidescope.xml.h \
	kumppa.xml.h \
	lament.xml.h \
	laser.xml.h \
	lightning.xml.h \
	lisa.xml.h \
	lissie.xml.h \
	lmorph.xml.h \
	loop.xml.h \
	maze.xml.h \
	moebius.xml.h \
	moire.xml.h \
	moire2.xml.h \
	morph3d.xml.h \
	mountain.xml.h \
	munch.xml.h \
	noseguy.xml.h \
	pedal.xml.h \
	penetrate.xml.h \
	penrose.xml.h \
	pipes.xml.h \
	pyro.xml.h \
	qix.xml.h \
	rd-bomb.xml.h \
	rocks.xml.h \
	rorschach.xml.h \
	rotor.xml.h \
	rubiks_cube.xml.h \
	sierpinski.xml.h \
	slidescreen.xml.h \
	slip.xml.h \
	sonar.xml.h \
	sphere.xml.h \
	spiral.xml.h \
	spotlight.xml.h \
	sproingies.xml.h \
	squiral.xml.h \
	stairs.xml.h \
	starfish.xml.h \
	strange.xml.h \
	superquadrics.xml.h \
	swirl.xml.h \
	triangle.xml.h \
	truchet.xml.h \
	vines.xml.h \
	worm.xml.h \
	xflame.xml.h \
	xjack.xml.h \
	xlyap.xml.h \
	xmatrix.xml.h \
	xroger.xml.h

EXTRA_DIST = $(Screensavers_DATA) $(screensavers_msgs) README

noinst_DATA = $(screensavers_msgs)

noinst_PROGRAMS = extract-labels

extract_labels_SOURCES = extract-labels.c

extract_labels_LDADD = \
	$(GNOME_LIBDIR) \
	$(GNOME_LIBS) \
	$(GNOME_XML_LIB)

#ant.xml.h: $(srcdir)/ant.xml extract-labels
#bsod.xml.h: $(srcdir)/bsod.xml extract-labels
#compass.xml.h: $(srcdir)/compass.xml extract-labels
#flag.xml.h: $(srcdir)/flag.xml extract-labels
#qix.xml.h: $(srcdir)/qix.xml extract-labels
#attraction.xml.h: $(srcdir)/attraction.xml extract-labels
#rorschach.xml.h: $(srcdir)/rorschach.xml extract-labels

%.xml.h: %.xml
	./extract-labels $<
