gcc -g -Wall `pkg-config --cflags --libs gtk+-2.0` -I../ ../randrwrap.c ../monitor-db.c xrandr-capplet.c ../edid-parse.c ../display-name.c scrollarea.c foo-marshal.c -o capplet
