pixmapsdir = $(GNOMECC_PIXMAPS_DIR)/screensavers

pixmaps_DATA = \
	decayscreen.png \
	distort.png \
	jigsaw.png \
	slip.png \
	vidwhacker.png \
	webcollage.png

Screensavers = \
	3d_clock.xml \
	ant.xml \
	atlantis.xml \
	attraction.xml \
	blaster.xml \
	blitspin.xml \
	bouboule.xml \
	braid.xml \
	bsod.xml \
	bubble3d.xml \
	bubbles.xml \
	bumps.xml \
	cage.xml \
	ccurve.xml \
	compass.xml \
	coral.xml \
	cosmos.xml \
	critical.xml \
	crystal.xml \
	cynosure.xml \
	dangerball.xml \
	decayscreen.xml \
	deco.xml \
	deluxe.xml \
	demon.xml \
	discrete.xml \
	distort.xml \
	drift.xml \
	electricsheep.xml \
	epicycle.xml \
	extrusion.xml \
	fadeplot.xml \
	flag.xml \
	flame.xml \
	flow.xml \
	forest.xml \
	galaxy.xml \
	gears.xml \
	gflux.xml \
	glplanet.xml \
	gltext.xml \
	goban.xml \
	goop.xml \
	grav.xml \
	greynetic.xml \
	halo.xml \
	helix.xml \
	hopalong.xml \
	hyperball.xml \
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
	moire2.xml \
	moire.xml \
	molecule.xml \
	morph3d.xml \
	mountain.xml \
	munch.xml \
	nerverot.xml \
	noseguy.xml \
	pedal.xml \
	penetrate.xml \
	penrose.xml \
	petri.xml \
	phosphor.xml \
	pipes.xml \
	pulsar.xml \
	pyro.xml \
	qix.xml \
	rd-bomb.xml \
	ripples.xml \
	rocks.xml \
	rorschach.xml \
	rotor.xml \
	rotzoomer.xml \
	rubiks_cube.xml \
	rubik.xml \
	shadebobs.xml \
	sierpinski3d.xml \
	sierpinski.xml \
	slidescreen.xml \
	slip.xml \
	sonar.xml \
	speedmine.xml \
	sphere.xml \
	spiral.xml \
	spotlight.xml \
	sproingies.xml \
	squiral.xml \
	ssystem.xml \
	stairs.xml \
	starfish.xml \
	starwars.xml \
	stonerview.xml \
	strange.xml \
	superquadrics.xml \
	swirl.xml \
	t3d.xml \
	triangle.xml \
	truchet.xml \
	vidwhacker.xml \
	vines.xml \
	wander.xml \
	webcollage.xml \
	whirlwindwarp.xml \
	whirlygig.xml \
	worm.xml \
	xaos.xml \
	xdaliclock.xml \
	xearth.xml \
	xfishtank.xml \
	xflame.xml \
	xjack.xml \
	xlyap.xml \
	xmatrix.xml \
	xmountains.xml \
	xrayswarm.xml \
	xroger.xml \
	xsnow.xml \
	xspirograph.xml \
	xteevee.xml \
	zoom.xml 

saversdir = $(GNOMECC_SCREENSAVERS_DIR)
savers_DATA = $(Screensavers)

EXTRA_DIST = $(Screensavers) README $(pixmaps_DATA)

ADFILE = $(XSCREENSAVER_PREFIX)/lib/X11/app-defaults/XScreenSaver

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(GNOMECC_SCREENSAVERS_DIR)
	@PERL@ magic.pl --adfile $(ADFILE) --merge $(DESTDIR)$(GNOMECC_SCREENSAVERS_DIR)
