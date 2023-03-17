This project provides communication and mobility models that are built around the OS3 framework for OMNeT++/INET. OMNeT++ 6.0 and INET version 4.4 are used in this project version. The igraph library is used to create a topology every x seconds of the satelite constellations. K-Shortest-path implemention is only present in the dev build of igraph, therefore that required. See the makemake instructions for how the library (and its dependancies) can be added. A igraph project is included within my GitHub repo which can be cloned and referenced for use in this project.

Once the project has been cloned, right click on the project and go onto properties. Then go onto OMNeT++ > Makemake. Then double click on src: makemake to open the make file options. In the Compile tab the absolute path of the OS3 src folder should be specified as an include path. The following examples are done on a Linux system, but the setup should be almost identicle for macOS.

/Volumes/LocalDataHD/av288/omnetpp-6.0/samples/os3/src

Then in the command line, set the enviroment as follows. It is vital that every directory mentioned is the respective directory for your system.

cd $HOME/omnetpp-6.0

. setenv

\# then, the working directory is set to the /src/ folder of the leosatellites project.

cd $HOME/omnetpp-6.0/samples/leosatellites/src

\# note that depending on how inet is installed, it may appear as inet or inet4 (we use inet here) # this command generates the makefile. Make sure that the paths are correct for INET (the INET_PROJ variable) and OS3 (the OS3_PROJ variable).

opp_makemake --make-so -f --deep -KINET4_4_PROJ=$HOME/omnetpp-6.0/samples/inet4.4 -KOS3_PROJ=$HOME/omnetpp-6.0/samples/os3 -DINET_IMPORT '-I/opt/local/include/igraph -I$(OS3_PROJ)/src' '-I$(INET4_4_PROJ)/src' '-I/opt/homebrew/Cellar/curl/7.83.1/include/curl' '-L/opt/local/lib' '-L$(INET4_4_PROJ)/src' '-L$(OS3_PROJ)/src' '-ligraph' '-lxml2 -lz' '-lgmp' '-lblas' '-lglpk' '-llapack' '-larpack' '-lINET$(D)' '-los3$(D)'

\# this command builds the shared library for leosatellites

make MODE=release && make MODE=debug

Simulations can now be run, by first changing to the directory of the respective simulation. For example:

cd $HOME/omnetpp-6.0/samples/leosatellites/simulations/SatSGP4

\# to run in debug mode

opp_run_dbg  -m -u Qtenv -c Experiment1 -n ../../src:..:../../../inet4.4/examples:../../../inet4.4/showcases:../../../inet4.4/src:../../../inet4.4/tests/validation:../../../inet4.4/tests/networks:../../../inet4.4/tutorials:../../../os3/simulations:../../../os3/src -x inet.common.selfdoc;inet.linklayer.configurator.gatescheduling.z3;inet.emulation;inet.showcases.visualizer.osg;inet.examples.emulation;inet.showcases.emulation;inet.transportlayer.tcp_lwip;inet.applications.voipstream;inet.visualizer.osg;inet.examples.voipstream --image-path=../../../inet4.4/images:../../../os3/images -l ../../src/leosatellites -l ../../../inet4.4/src/INET -l ../../../os3/src/os3 --debug-on-errors=true omnetpp.ini

\# to run in release mode

opp_run  -m -u Qtenv -c Experiment1 -n ../../src:..:../../../inet4.4/examples:../../../inet4.4/showcases:../../../inet4.4/src:../../../inet4.4/tests/validation:../../../inet4.4/tests/networks:../../../inet4.4/tutorials:../../../os3/simulations:../../../os3/src -x inet.common.selfdoc;inet.linklayer.configurator.gatescheduling.z3;inet.emulation;inet.showcases.visualizer.osg;inet.examples.emulation;inet.showcases.emulation;inet.transportlayer.tcp_lwip;inet.applications.voipstream;inet.visualizer.osg;inet.examples.voipstream --image-path=../../../inet4.4/images:../../../os3/images -l ../../src/leosatellites -l ../../../inet4.4/src/INET -l ../../../os3/src/os3 --debug-on-errors=true omnetpp.ini

# Source Code Referencing
If you use this code or want to cite its existence in your paper please use the following bibtex:
```
@inproceedings{omnetpp-leosatellites-model,
  author = {Valentine, Aiden and Parisis, George},
  title = {{Developing and experimenting with LEO satellite constellations in OMNeT++}},
  booktitle = {Proceedings of the 8th OMNeT++ Community Summit Conference},
  address = {Hamburg, Germany},
  Year = {2021}
}
```
