This project provides communication and mobility models that are built around the OS3 framework for OMNeT++/INET.

Once the project has been cloned, right click on the project and go onto properties. Then go onto OMNeT++ > Makemake. Then double click on src: makemake to open the make file options. In the Compile tab the absolute path of the OS3 src folder should be specified as an include path. The following examples are done on a Linux system, but the setup should be almost identicle for macOS.

/Volumes/LocalDataHD/av288/omnetpp-5.6.2/samples/os3/src

Then in the command line, set the enviroment as follows. It is vital that every directory mentioned is the respective directory for your system.

cd $HOME/omnetpp-5.6.2

. setenv

\# then, the working directory is set to the /src/ folder of the leosatellites project.

cd $HOME/omnetpp-5.6.2/samples/leosatellites/src

\# note that depending on how inet is installed, it may appear as inet or inet4 (we use inet here) # this command generates the makefile. Make sure that the paths are correct for INET (the INET_PROJ variable) and OS3 (the OS3_PROJ variable).

opp_makemake --make-so -f --deep -O out -KINET_PROJ=$HOME/omnetpp-5.6.2/samples/inet -KOS3_PROJ=$HOME/omnetpp-5.6.2/samples/os3 -DINET_IMPORT '-I$(OS3_PROJ)/src' '-I$(INET_PROJ)/src' -I/usr/include/curl '-L$(INET_PROJ)/src' '-L$(OS3_PROJ)/src' '-lINET$(D)' '-los3$(D)'

\# this command builds the shared library for leosatellites

make MODE=release && make MODE=debug

Simulations can now be run, by first changing to the directory of the respective simulation. For example:

cd $HOME/omnetpp-5.6.2/samples/leosatellites/simulations/SatSGP4

\# to run in debug mode

opp_run_dbg -m -u Qtenv -c Experiment-Image -n $HOME/omnetpp-5.6.2/samples/leosatellites/src:..:$HOME/omnetpp-5.6.2/samples/inet/src:$HOME/omnetpp-5.6.2/samples/inet/examples:$HOME/omnetpp-5.6.2/samples/inet/tutorials:$HOME/omnetpp-5.6.2/samples/inet/showcases:$HOME/omnetpp-5.6.2/samples/os3/simulations:$HOME/omnetpp-5.6.2/samples/os3/src --image-path=$HOME/omnetpp-5.6.2/samples/inet/images:$HOME/omnetpp-5.6.2/samples/os3/images -l $HOME/omnetpp-5.6.2/samples/leosatellites/src/leosatellites omnetpp.ini

\# to run in release mode

opp_run -m -u Qtenv -c Experiment-Image -n $HOME/omnetpp-5.6.2/samples/leosatellites/src:..:$HOME/omnetpp-5.6.2/samples/inet/src:$HOME/omnetpp-5.6.2/samples/inet/examples:$HOME/omnetpp-5.6.2/samples/inet/tutorials:$HOME/omnetpp-5.6.2/samples/inet/showcases:$HOME/omnetpp-5.6.2/samples/os3/simulations:$HOME/omnetpp-5.6.2/samples/os3/src --image-path=$HOME/omnetpp-5.6.2/samples/inet/images:$HOME/omnetpp-5.6.2/samples/os3/images -l $HOME/omnetpp-5.6.2/samples/leosatellites/src/leosatellites omnetpp.ini
