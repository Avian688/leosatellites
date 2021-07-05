# LEOSatelliteSimulator

**This project provides communication and mobility models that are built around the OS3 framework for OMNeT++/INET.**

**To install this project, you must import the archive file of the project as an "Existing Project into Workspace".**

**Then, the working directory should be set to the /src/ folder of the leosatellites project. It is vital that every directory mentioned is the respective directory for  For example:**
cd $HOME/omnetpp-5.6.2/samples/leosatellites/src

**Then the make file must be generated with the following command:**

opp_makemake --make-so -f --deep -O out -KINET_PROJ=$HOME/omnetpp-5.6.2/samples/inet -KOS3_PROJ=$HOME/omnetpp-5.6.2/samples/os3 -DINET_IMPORT '-I$(OS3_PROJ)/src' '-I$(INET_PROJ)/src' -I/usr/include/curl '-L$(INET_PROJ)/src' '-L$(OS3_PROJ)/src' '-lINET$(D)' '-los3$(D)'

**Which must then be followed by:

make MODE=release && make MODE=debug

**Simulations can then be run by changing the working directory to a specific simulation.**
