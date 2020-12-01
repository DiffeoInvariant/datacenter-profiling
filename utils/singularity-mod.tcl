#%Module1.0#####################################################################
##
## modules singularity/${SINGULARITY_VERSION}.
##
## modulefiles/singularity/${SINGULARITY_VERSION}.
##
proc ModulesHelp { } {
    global version modroot
    puts stderr "singularity/${SINGULARITY_VERSION} - sets the environment for Singularity ${SINGULARITY_VERSION}"
}

module-whatis "Sets the environment for Singularity ${VERSION}"

set  topdir /apps/singularity/${SINGULARITY_VERSION}
set  version ${SINGULARITY_VERSION}
set  sys     linux86
prepend-path PATH \$topdir/bin
SINGULARITY_MODULEFILE
