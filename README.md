# Decoding Proprietary Marine Track Files

This package contains tools for decoding Raymarine's
[ARCHIVE.FSH](https://wiki.openstreetmap.org/wiki/ARCHIVE.FSH) files, as well
as Garmin's
[IMG](https://wiki.openstreetmap.org/wiki/OSM_Map_On_Garmin/IMG_File_Format)
archives and the
[TRK](https://wiki.openstreetmap.org/wiki/OSM_Map_On_Garmin/TRK_Subfile_Format)
subfiles.


## Parsefsh

Parsefsh is a tool to convert Raymarine's
[ARCHIVE.FSH](https://wiki.openstreetmap.org/wiki/ARCHIVE.FSH) files to OSM,
GPX, or CSV format.  It is written in portable manner and should compile and
run on most systems as long as the are based on a Little Endian CPU (such as
Intel). It is written on Linux but tested also on FreeBSD and Windows.

Parsefsh reads FSH data from standard input (stdin) and outputs the waypoints,
tracks and routes in OSM, GPX, or CSV format to standard output (stdout). Use
the command line option `-f gpx` or `-f csv` to changes the output format to
GPX or CSV. OSM is the default format. The following command shows how to run
it.

```Shell
parsefsh < ARCHIVE.FSH > archive.osm
```


## Splitimg

Splitimg is a tool which splits Garmin's IMG and ADM files into its subfiles.
[IMG/ADM](https://wiki.openstreetmap.org/wiki/OSM_Map_On_Garmin/IMG_File_Format)
files are actually archives containing a set of other files called subfiles.
Using the appropriate converters, those subfiles may be further processed, e.g.
the tool Parsetrk parses TRK subfiles which contain tracks recorded by marine
chart plotters of Garmin.

Splitimg reads the IMG/ADM file from the standard input and creates the
subfiles in the current working directory or in the directory given by option
`-d`. To run Splitimg simply type the following command:

```Shell
splitimg < GMAPSUPP.IMG
```


## Parsetrk

Parsetrk is a tool to convert Garmin's TRK Subfiles files to OSM, or CSV
format.  It is written in portable manner and should compile and run on most
systems as long as the are based on a Little Endian CPU (such as Intel). It is
written on Linux and currently only tested on Linux.

Parsetrk reads TRK data from standard input and outputs the tracks in OSM, or
CSV format to stdout. Use the command line option -f csv to change the output
format to CSV. OSM is the default format. The following command shows how to
run it.

```Shell
parsetrk < USERDATA.TRK > track.osm
```

### Note

Please note that TRK files usually do not exist as standalone files.  The
Garmin chartplotters store the tracks in ADM files which are archives for a set
of subfiles. Thus, the ADM file has to be split into separate files (such as
USERDATA.TRK) before Parsetrk can be used. The split ADM files you may use the
tool Splitimg. See there for further information.


## Author

Parsefsh is developed and maintained by Bernhard R. Fischer, 2048R/5C5FFD47
<bf@abenteuerland.at>.


## License

Parsefsh is released under GNU GPLv3. 


